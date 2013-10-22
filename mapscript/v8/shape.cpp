/**********************************************************************
 *
 * Project:  MapServer
 * Purpose:  V8 JavaScript Engine Support
 * Author:   Alan Boudreault (aboudreault@mapgears.com)
 *
 **********************************************************************
 * Copyright (c) 2013, Alan Boudreault, MapGears
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies of this Software or works derived from this Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "mapserver-config.h"
#ifdef USE_V8_MAPSCRIPT

#include "v8_mapscript.h"

static void msV8ShapeObjDestroy(Isolate *isolate,
                                Persistent<Object> *object,
                                shapeObj *shape)
{
  msFreeShape(shape);
  object->Dispose();
  object->Clear();
}

static void msV8ObjectMapDestroy(Isolate *isolate,
                                 Persistent<Object> *object,
                                 map<string, int> *map)
{
  delete map;
  object->Dispose();
  object->Clear();
}

Handle<Value> msV8ShapeObjNew(const Arguments& args)
{
  Local<Object> self = args.Holder();
  Handle<String> key = String::New("__obj__");  
  shapeObj *shape;
  
  if (!self->Has(key)) {  
    shape = (shapeObj *)msSmallMalloc(sizeof(shapeObj));

    msInitShape(shape);
    if(args.Length() >= 1) {
      shape->type = args[0]->Int32Value();
    }
    else {
      shape->type = MS_SHAPE_NULL;
    }

    V8Shape::setInternalField(self, shape);
    Persistent<Object> pobj;
    pobj.Reset(Isolate::GetCurrent(), self);
    pobj.MakeWeak(shape, msV8ShapeObjDestroy);
  }
  else
  {
    Local<External> wrap = Local<External>::Cast(self->Get(key));
    void *ptr = wrap->Value();
    shape = static_cast<shapeObj*>(ptr);    
  }

  /* create the attribute map */
  map<string, int> *attributes_map = new map<string, int>();
  Handle<String> key_parent = String::New("__parent__");  
  if (self->Has(key_parent)) {
    layerObj *layer;
    Local<Object> layer_obj = self->Get(key_parent)->ToObject();
    Local<External> wrap = Local<External>::Cast(layer_obj->GetInternalField(0));
    void *ptr = wrap->Value();
    layer = static_cast<layerObj*>(ptr);
    for (int i=0; i<layer->numitems; ++i) {
      (*attributes_map)[string(layer->items[i])] = i;
    }
  }

  Handle<ObjectTemplate> attributes_templ = ObjectTemplate::New();
  attributes_templ->SetInternalFieldCount(2);
  attributes_templ->SetNamedPropertyHandler(msV8ShapeObjGetValue,
                                            msV8ShapeObjSetValue);
  Handle<Object> attributes = attributes_templ->NewInstance();
  attributes->SetInternalField(0, External::New(attributes_map));
  attributes->SetInternalField(1, External::New(shape->values));
  attributes->SetHiddenValue(key_parent, self);
  self->Set(String::New("attributes"), attributes);
  Persistent<Object> attributes_po(Isolate::GetCurrent(), attributes);
  attributes_po.MakeWeak(attributes_map, msV8ObjectMapDestroy);
  
  return self;
}

Handle<Value> msV8ShapeObjClone(const Arguments& args)
{
  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  shapeObj *shape = static_cast<shapeObj*>(ptr);

  shapeObj *new_shape = (shapeObj *)msSmallMalloc(sizeof(shapeObj));
  msInitShape(new_shape);
  msCopyShape(shape, new_shape);

  /* we need this to copy shape attributes */
  Handle<String> key_parent = String::New("__parent__");
  if (!self->GetHiddenValue(key_parent).IsEmpty()) {
    V8Shape s(new_shape, self->GetHiddenValue(key_parent)->ToObject());
    Handle<Object> obj = s.newInstance();
    obj->DeleteHiddenValue(key_parent);
    return obj;
  }
  else {
    V8Shape s(new_shape);    
    return s.newInstance();
  }
}


Handle<Value> msV8ShapeObjGetLine(const Arguments& args)
{
  if (args.Length() < 1 || !args[0]->IsInt32()) {
    ThrowException(String::New("Invalid argument"));
    return Undefined();
  }

  int index = args[0]->Int32Value();

  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  shapeObj *shape = static_cast<shapeObj*>(ptr);

  if (index < 0 || index >= shape->numlines)
  {
    ThrowException(String::New("Invalid line index."));
    return Undefined();
  }

  V8Line l(&shape->line[index], self);
  return l.newInstance();
}


Handle<Value> msV8ShapeObjAddLine(const Arguments& args)
{
  if (args.Length() < 1 || !args[0]->IsObject() ||
      !args[0]->ToObject()->GetConstructorName()->Equals(String::New("lineObj"))) {
    ThrowException(String::New("Invalid argument"));
    return Undefined();
  }

  Local<Object> line_obj = args[0]->ToObject();
  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  shapeObj *shape = static_cast<shapeObj*>(ptr);
  wrap = Local<External>::Cast(line_obj->GetInternalField(0));
  ptr = wrap->Value();
  lineObj *line = static_cast<lineObj*>(ptr);

  msAddLine(shape, line);

  return Undefined();
}

void msV8ShapeObjGetValue(Local<String> name,
                          const PropertyCallbackInfo<Value> &info)
{
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  map<string, int> *indexes = static_cast<map<string, int> *>(ptr);
  wrap = Local<External>::Cast(self->GetInternalField(1));
  ptr = wrap->Value();
  char **values = static_cast<char **>(ptr);

  String::Utf8Value utf8_value(name);
  string key = string(*utf8_value);
  map<string, int>::iterator iter = indexes->find(key);

  if (iter != indexes->end()) {
    const int &index = (*iter).second;
    info.GetReturnValue().Set(String::New(values[index]));
  }
}

void msV8ShapeObjSetValue(Local<String> name,
                          Local<Value> value,
                          const PropertyCallbackInfo<Value> &info)
{
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  map<string, int> *indexes = static_cast<map<string, int> *>(ptr);
  wrap = Local<External>::Cast(self->GetInternalField(1));
  ptr = wrap->Value();
  char **values = static_cast<char **>(ptr);

  String::Utf8Value utf8_name(name), utf8_value(value);
  string key = string(*utf8_name);

  map<string, int>::iterator iter = indexes->find(key);

  if (iter == indexes->end()) {
    ThrowException(String::New("Invalid value name."));
  }
  else
  {
    const int &index = (*iter).second;
    msFree(values[index]);
    values[index] = msStrdup(*utf8_value);
  }
}

Handle<Value> msV8ShapeObjSetGeometry(const Arguments& args)
{
  if (args.Length() < 1 || !args[0]->IsObject() ||
      !args[0]->ToObject()->GetConstructorName()->Equals(String::New("shapeObj"))) {
    ThrowException(String::New("Invalid argument"));
    return Undefined();
  }
  
  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  shapeObj *shape = static_cast<shapeObj*>(ptr);
  wrap = Local<External>::Cast(args[0]->ToObject()->GetInternalField(0));
  ptr = wrap->Value();
  shapeObj *new_shape = static_cast<shapeObj*>(ptr);

  /* clean current shape */
  for (int i = 0; i < shape->numlines; i++) {
    free(shape->line[i].point);
  }
  if (shape->line) free(shape->line);
  shape->line = NULL;
  shape->numlines = 0;

  for (int i = 0; i < new_shape->numlines; i++) {
    msAddLine(shape, &(new_shape->line[i]));
  }
  
  return Undefined();
}

#endif
