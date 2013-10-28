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
#include <map>

using namespace v8;
using std::map;
using std::string;

Persistent<FunctionTemplate> Shape::constructor;

void Shape::Initialize(Handle<Object> target)
{
  HandleScope scope;

  Handle<FunctionTemplate> c = FunctionTemplate::New(Shape::New);
  c->InstanceTemplate()->SetInternalFieldCount(1);
  c->SetClassName(String::NewSymbol("shapeObj"));

  SET_ATTRIBUTE_RO(c, "numvalues", getProp);
  SET_ATTRIBUTE_RO(c, "numlines", getProp);
  SET_ATTRIBUTE_RO(c, "index", getProp);
  SET_ATTRIBUTE_RO(c, "type", getProp);
  SET_ATTRIBUTE_RO(c, "tileindex", getProp);
  SET_ATTRIBUTE_RO(c, "classindex", getProp);
  SET_ATTRIBUTE(c, "text", getProp, setProp);    

  /* create the attribute map */
  //map<string, int> *attributes_map = new map<string, int>();
  Handle<ObjectTemplate> attributes_templ = ObjectTemplate::New();
  attributes_templ->SetInternalFieldCount(2);
  // attributes_templ->SetNamedPropertyHandler(attributeGetValue,
  //                                           attributeSetValue);
  SET(c, "attributes", attributes_templ);

  NODE_SET_PROTOTYPE_METHOD(c, "clone", clone);
  NODE_SET_PROTOTYPE_METHOD(c, "line", getLine);
  NODE_SET_PROTOTYPE_METHOD(c, "add", addLine);
  NODE_SET_PROTOTYPE_METHOD(c, "setGeometry", setGeometry);
  
  target->Set(String::NewSymbol("shapeObj"), c->GetFunction());

  constructor.Reset(Isolate::GetCurrent(), c);
}

Shape::~Shape()
{
  msFreeShape(this->get());
}

Handle<Function> Shape::Constructor()
{
  return (*Shape::constructor)->GetFunction();
}

void Shape::New(const v8::FunctionCallbackInfo<Value>& args)
{
  HandleScope scope;

  if (args[0]->IsExternal())
  {
    Local<External> ext = Local<External>::Cast(args[0]);
    void *ptr = ext->Value();
    Shape *shape = static_cast<Shape*>(ptr);
    shape->Wrap(args.Holder());
  }
  else
  {
    shapeObj *s = (shapeObj *)msSmallMalloc(sizeof(shapeObj));

    msInitShape(s);
    if(args.Length() >= 1) {
      s->type = args[0]->Int32Value();
    }
    else {
      s->type = MS_SHAPE_NULL;
    }

    Shape *shape = new Shape(s);
    shape->Wrap(args.Holder());
  }
}

Shape::Shape(shapeObj *s):
  ObjectWrap()
{
  this->this_ = s;
}

void Shape::getProp(Local<String> property,
                    const PropertyCallbackInfo<Value>& info)
{
    HandleScope scope;
    Shape* s = ObjectWrap::Unwrap<Shape>(info.Holder());
    std::string name = TOSTR(property);
    Handle<Value> value = Undefined();

    if (name == "numvalues")
      value = Integer::New(s->get()->numvalues);
    else if (name == "numlines")
      value = Integer::New(s->get()->numlines);
    else if (name == "index")
      value = Number::New(s->get()->index);
    else if (name == "type")
      value = Integer::New(s->get()->type);
    else if (name == "tileindex")
      value = Integer::New(s->get()->tileindex);
    else if (name == "classindex")
      value = Integer::New(s->get()->classindex);
    else if (name == "text")
      value = String::New(s->get()->text);  
    
    info.GetReturnValue().Set(value);
}

void Shape::setProp(Local<String> property,
                    Local<Value> value,
                    const PropertyCallbackInfo<void>& info)
{
    HandleScope scope;
    Shape* s = ObjectWrap::Unwrap<Shape>(info.Holder());
    std::string name = TOSTR(property);

    if (name == "text") {
      if (s->get()->text)
        msFree(s->get()->text);
      s->get()->text = getStringValue(value);
    }

}

void Shape::clone(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  HandleScope scope;
  Shape* s = ObjectWrap::Unwrap<Shape>(args.Holder());  
  shapeObj *shape = s->get();

  shapeObj *new_shape = (shapeObj *)msSmallMalloc(sizeof(shapeObj));
  msInitShape(new_shape);
  msCopyShape(shape, new_shape);

  /* we need this to copy shape attributes */
  // Handle<String> key_parent = String::New("__parent__");
  // if (!SELF->GetHiddenValue(key_parent).IsEmpty()) {
  //   V8Shape s(new_shape, self->GetHiddenValue(key_parent)->ToObject());
  //   Handle<Object> obj = s.newInstance();
  //   obj->DeleteHiddenValue(key_parent);
  //   return obj;
  // }
  // else {

  Shape *ns = new Shape(new_shape);
  Handle<Value> ext = External::New(ns);  
  args.GetReturnValue().Set(Shape::Constructor()->NewInstance(1, &ext));  
}

void Shape::getLine(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    ThrowException(String::New("Invalid argument"));
    return;
  }

  int index = args[0]->Int32Value();

  Shape* s = ObjectWrap::Unwrap<Shape>(args.Holder());  
  shapeObj *shape = s->get();  

  if (index < 0 || index >= shape->numlines)
  {
    ThrowException(String::New("Invalid line index."));
    return;
  }

  Line *line = new Line(&shape->line[index]);
  Handle<Value> ext = External::New(line);  
  args.GetReturnValue().Set(Line::Constructor()->NewInstance(1, &ext));
}


void Shape::addLine(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsObject() ||
      !args[0]->ToObject()->GetConstructorName()->Equals(String::New("lineObj"))) {
    ThrowException(String::New("Invalid argument"));
    return;
  }

  Shape* s = ObjectWrap::Unwrap<Shape>(args.Holder());  
  shapeObj *shape = s->get();  

  Line* l = ObjectWrap::Unwrap<Line>(args[0]->ToObject());  
  lineObj *line = l->get();
  
  msAddLine(shape, line);
}

void Shape::setGeometry(const v8::FunctionCallbackInfo<v8::Value>& args)
{
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsObject() ||
      !args[0]->ToObject()->GetConstructorName()->Equals(String::New("shapeObj"))) {
    ThrowException(String::New("Invalid argument"));
    return;
  }

  Shape* s = ObjectWrap::Unwrap<Shape>(args.Holder());  
  shapeObj *shape = s->get();
  Shape* ns = ObjectWrap::Unwrap<Shape>(args[0]->ToObject());  
  shapeObj *new_shape = ns->get();
  
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
  
  return;
}

void Shape::attributeGetValue(Local<String> name,
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

void Shape::attributeSetValue(Local<String> name,
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

void Shape::attributeMapDestroy(Isolate *isolate,
                                Persistent<Object> *object,
                                map<string, int> *map)
{
  delete map;
  object->Dispose();
  object->Clear();
}

/// kill
// Handle<Value> msV8ShapeObjNew(const Arguments& args)
// {
//   Local<Object> self = args.Holder();
//   Handle<String> key = String::New("__obj__");  
//   shapeObj *shape;
  
//   if (!self->Has(key)) {  
//     shape = (shapeObj *)msSmallMalloc(sizeof(shapeObj));

//     msInitShape(shape);
//     if(args.Length() >= 1) {
//       shape->type = args[0]->Int32Value();
//     }
//     else {
//       shape->type = MS_SHAPE_NULL;
//     }

//     V8Shape::setInternalField(self, shape);
//     Persistent<Object> pobj;
//     pobj.Reset(Isolate::GetCurrent(), self);
//     //pobj.MakeWeak(shape, msV8ShapeObjDestroy);
//   }
//   else
//   {
//     Local<External> wrap = Local<External>::Cast(self->Get(key));
//     void *ptr = wrap->Value();
//     shape = static_cast<shapeObj*>(ptr);    
//   }

//   /* create the attribute map */
//   map<string, int> *attributes_map = new map<string, int>();
//   Handle<String> key_parent = String::New("__parent__");  
//   if (self->Has(key_parent)) {
//     layerObj *layer;
//     Local<Object> layer_obj = self->Get(key_parent)->ToObject();
//     Local<External> wrap = Local<External>::Cast(layer_obj->GetInternalField(0));
//     void *ptr = wrap->Value();
//     layer = static_cast<layerObj*>(ptr);
//     for (int i=0; i<layer->numitems; ++i) {
//       (*attributes_map)[string(layer->items[i])] = i;
//     }
//   }

//   Handle<ObjectTemplate> attributes_templ = ObjectTemplate::New();
//   attributes_templ->SetInternalFieldCount(2);
//   attributes_templ->SetNamedPropertyHandler(msV8ShapeObjGetValue,
//                                             msV8ShapeObjSetValue);
//   Handle<Object> attributes = attributes_templ->NewInstance();
//   attributes->SetInternalField(0, External::New(attributes_map));
//   attributes->SetInternalField(1, External::New(shape->values));
//   attributes->SetHiddenValue(key_parent, self);
//   self->Set(String::New("attributes"), attributes);
//   Persistent<Object> attributes_po(Isolate::GetCurrent(), attributes);
//   //attributes_po.MakeWeak(attributes_map, msV8ObjectMapDestroy);
  
//   return self;
// }

#endif
