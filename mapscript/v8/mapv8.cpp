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

#include <map>
#include "v8_mapscript.h"

#define SET_TEXT_ACCESSOR(obj_templ, name, obj_type, property, v8_type) obj_templ->SetAccessor(String::New(name), \
                     msV8Getter<obj_type, &obj_type::property, v8_type>, \
                     msV8Setter<obj_type, &obj_type::property> , \
                     Handle<Value>(), \
                     PROHIBITS_OVERWRITING, \
                     None)

#define SET_GETTER_OLD(obj_templ, name, obj_type, property_type, property, v8_type) obj_templ->SetAccessor(String::New(name), \
                   msV8Getter<obj_type, property_type, &obj_type::property, v8_type>, \
                   0, \
                   Handle<Value>(), \
                   PROHIBITS_OVERWRITING, \
                   ReadOnly)

#define SET_ACCESSOR_OLD(obj_templ, name, obj_type, property_type, property, v8_type) obj_templ->SetAccessor(String::New(name), \
                     msV8Getter<obj_type, property_type, &obj_type::property, v8_type>, \
                     msV8Setter<obj_type, property_type, &obj_type::property>, \
                     Handle<Value>(), \
                     PROHIBITS_OVERWRITING, \
                     None)

static char *msV8GetCString(Local<Value> value, const char *fallback = "");
static Handle<Object> msV8WrapShapeObj(Isolate *isolate, layerObj *layer,
                                       shapeObj *shape, Persistent<Object> *po);

/* MAPSERVER OBJECT WRAPPERS */
/* should be moved somewhere else if we expose more objects */

/* This is currently only an example how to wrap a C object to expose it to
 * JavaScript. This needs to be investigated more if we decide to create a
 * complete MS object binding for v8. */

template <typename T, typename V, V T::*mptr, typename R>
static Handle<Value> msV8Getter(Local<String> property,
    const AccessorInfo &info)
{
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  T *o = static_cast<T*>(ptr);
  return R::New(o->*mptr);
}

/* Specific getter for char* */
template <typename T, char* T::*mptr, typename R>
static Handle<Value> msV8Getter(Local<String> property,
    const AccessorInfo &info)
{
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  T *o = static_cast<T*>(ptr);
  if (o->*mptr == NULL)
    return Null();

  return R::New(o->*mptr);
}

template <typename T, typename V, V T::*mptr>
static void msV8Setter(Local<String> property, Local<Value> value,
    const AccessorInfo &info)
{
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  if (value->IsInt32()) {
    static_cast<T*>(ptr)->*mptr = value->Int32Value();
  }
  else if (value->IsNumber()) {
    static_cast<T*>(ptr)->*mptr = value->NumberValue();
  }
}

/* Specific setter for char* */
template <typename T, char* T::*mptr>
static void msV8Setter(Local<String> property, Local<Value> value,
    const AccessorInfo &info)
{
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  char *cvalue = static_cast<T*>(ptr)->*mptr;
  if (cvalue)
    msFree(cvalue);
  static_cast<T*>(ptr)->*mptr = msV8GetCString(value);
}

static void msV8WeakShapeObjCallback(Isolate *isolate, Persistent<Object> *object,
                                     shapeObj *shape)
{
  msFreeShape(shape);
  object->Dispose();
  object->Clear();
}

static void msV8WeakAttMapCallback(Isolate *isolate, Persistent<Object> *object,
                                   map<string, int> *map)
{
  delete map;
  object->Dispose();
  object->Clear();
}

static Handle<Value> msV8ShapeObjNew(const Arguments& args)
{
  Local<Object> self = args.Holder();
  shapeObj *shape;
  shape = (shapeObj *)msSmallMalloc(sizeof(shapeObj));
  msInitShape(shape);
  if(args.Length() >= 1) {
    shape->type = args[0]->Int32Value();
  }
  else {
    shape->type = MS_SHAPE_NULL;
  }

  //  Persistent<Object> persistent_shape;
  return self;//msV8WrapShapeObj(Isolate::GetCurrent(), NULL, shape, &persistent_shape);  
}

static Handle<Value> msV8ShapeObjClone(const Arguments& args)
{
  Local<Object> self = args.Holder();
  layerObj *layer = NULL;  
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  shapeObj *shape = static_cast<shapeObj*>(ptr);

  if (self->InternalFieldCount()  == 2) {
    wrap = Local<External>::Cast(self->GetInternalField(1));
    ptr = wrap->Value();
    layer = static_cast<layerObj*>(ptr);
  }

  shapeObj *new_shape = (shapeObj *)msSmallMalloc(sizeof(shapeObj));
  msInitShape(new_shape);
  msCopyShape(shape, new_shape);

  Persistent<Object> persistent_shape;
  return msV8WrapShapeObj(Isolate::GetCurrent(), layer, new_shape, &persistent_shape);
}

static Handle<Value> msV8ShapeObjGetLine(const Arguments& args)
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


static Handle<Value> msV8ShapeObjAddLine(const Arguments& args)
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

static Handle<Value> msV8ShapeObjGetValue(Local<String> name,
                                          const AccessorInfo &info)
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

  if (iter == indexes->end()) return Handle<Value>();

  const int &index = (*iter).second;
  return String::New(values[index]);
}

static Handle<Value> msV8ShapeObjSetValue(Local<String> name,
                                 Local<Value> value,
                                 const AccessorInfo &info)
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
    return Handle<Value>();
  }

  const int &index = (*iter).second;
  msFree(values[index]);
  values[index] = msStrdup(*utf8_value);
  return Handle<Value>();
}

static Handle<Value> msV8ShapeObjCleanGeometry(const Arguments& args)
{
  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  shapeObj *shape = static_cast<shapeObj*>(ptr);

  for (int c = 0; c < shape->numlines; c++) {
    free(shape->line[c].point);
  }
  if (shape->line) free(shape->line);
  shape->line = NULL;
  shape->numlines = 0;

  return Undefined();
}

/* Shape object wrapper. Maybe we could create a generic template class
 * that would handle that stuff */
static Handle<Object> msV8WrapShapeObj(Isolate *isolate, layerObj *layer,
                                       shapeObj *shape, Persistent<Object> *po)
{
  Handle<ObjectTemplate> shape_templ = ObjectTemplate::New();
  shape_templ->SetInternalFieldCount(layer ? 2 : 1);

  SET_GETTER_OLD(shape_templ, "numvalues", shapeObj, int, numvalues, Integer);
  SET_GETTER_OLD(shape_templ, "numlines", shapeObj, int, numlines, Integer);
  SET_GETTER_OLD(shape_templ, "index", shapeObj, long, index, Integer);
  SET_GETTER_OLD(shape_templ, "type", shapeObj, int, type, Integer);
  SET_GETTER_OLD(shape_templ, "tileindex", shapeObj, int, tileindex, Integer);
  SET_ACCESSOR_OLD(shape_templ, "classindex", shapeObj, int, classindex, Integer);
  SET_TEXT_ACCESSOR(shape_templ, "text", shapeObj, text, String);

  shape_templ->Set(String::New("clone"), FunctionTemplate::New(msV8ShapeObjClone));
  shape_templ->Set(String::New("line"), FunctionTemplate::New(msV8ShapeObjGetLine));
  shape_templ->Set(String::New("add"), FunctionTemplate::New(msV8ShapeObjAddLine));
  shape_templ->Set(String::New("cleanGeometry"), FunctionTemplate::New(msV8ShapeObjCleanGeometry));  

  Handle<Object> obj = shape_templ->NewInstance();
  obj->SetInternalField(0, External::New(shape));
  if (layer) {
    obj->SetInternalField(1, External::New(layer)); /* needed for the clone method? layer items.. */
  }
  obj->SetHiddenValue(String::New("__classname__"), String::New("shapeObj"));

  /* shape attributes */
  map<string, int> *attributes_map = new map<string, int>();
  if (layer) {
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
  attributes->SetHiddenValue(String::New("__parent__"), obj);
  obj->Set(String::New("attributes"), attributes);
  Persistent<Object> attributes_po(isolate, attributes);
  attributes_po.MakeWeak(attributes_map, msV8WeakAttMapCallback);

  if (po) { /* A Persistent object have to be passed if v8 have to free some memory */
    po->Reset(isolate, obj);
    po->MakeWeak(shape, msV8WeakShapeObjCallback);
  }

  return obj;
}


// /* Line object wrapper. we can only access line through a shape */
// static Handle<Object> msV8WrapLineObj(Isolate *isolate, lineObj *line,
//                                       Handle<Object> parent)
// {
//   Handle<ObjectTemplate> line_templ = ObjectTemplate::New();
//   line_templ->SetInternalFieldCount(1);

//   //SET_GETTER_OLD(line_templ, "numpoints", lineObj, int, numpoints, Integer);
//   line_templ->SetAccessor(String::New("numpoints"),
//                           msV8Getter<lineObj, int, &lineObj::numpoints, Integer>, 0,Handle<Value>(), PROHIBITS_OVERWRITING,
//                           ReadOnly);

  
//   line_templ->Set(String::New("point"), FunctionTemplate::New(msV8LineObjGetPoint));
//   line_templ->Set(String::New("addXY"), FunctionTemplate::New(msV8LineObjAddXY));
//   line_templ->Set(String::New("addXYZ"), FunctionTemplate::New(msV8LineObjAddXYZ));
//   line_templ->Set(String::New("add"), FunctionTemplate::New(msV8LineObjAddPoint));

//   Handle<Object> obj = line_templ->NewInstance();
//   obj->SetInternalField(0, External::New(line));
//   obj->SetHiddenValue(String::New("__parent__"), parent);
//   obj->SetHiddenValue(String::New("__classname__"), String::New("lineObj"));

//   return obj;
// }

/* END OF MAPSERVER OBJECT WRAPPERS */

/* INTERNAL JAVASCRIPT FUNCTIONS */

/* Get C char from a v8 string. Caller has to free the returned value. */
static char *msV8GetCString(Local<Value> value, const char *fallback)
{
  if (value->IsString()) {
    String::AsciiValue string(value);
    char *str = (char *) malloc(string.length() + 1);
    strcpy(str, *string);
    return str;
  }
  char *str = (char *) malloc(strlen(fallback) + 1);
  strcpy(str, fallback);
  return str;
}

/* Handler for Javascript Exceptions. Not exposed to JavaScript, used internally.
   Most of the code from v8 shell example.
*/
void msV8ReportException(TryCatch* try_catch, const char *msg = "")
{
  HandleScope handle_scope;

  if (!try_catch || !try_catch->HasCaught()) {
    msSetError(MS_V8ERR, "%s.", "msV8ReportException()", msg);
    return;
  }

  String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = *exception;
  Handle<Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    msSetError(MS_V8ERR, "Javascript Exception: %s.", "msV8ReportException()",
               exception_string);
  } else {
    String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = *filename;
    int linenum = message->GetLineNumber();
    msSetError(MS_V8ERR, "Javascript Exception: %s:%i: %s", "msV8ReportException()",
               filename_string, linenum, exception_string);
    String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = *sourceline;
    msSetError(MS_V8ERR, "Exception source line: %s", "msV8ReportException()",
               sourceline_string);
    String::Utf8Value stack_trace(try_catch->StackTrace());
    if (stack_trace.length() > 0) {
      const char* stack_trace_string = *stack_trace;
      msSetError(MS_V8ERR, "Exception StackTrace: %s", "msV8ReportException()",
                 stack_trace_string);
    }
  }
}

/* This function load a javascript file in memory. */
static Handle<Value> msV8ReadFile(V8Context *v8context, const char *name)
{
  char path[MS_MAXPATHLEN];

  /* construct the path */
  msBuildPath(path, v8context->paths.top().c_str(), name);
  char *filepath = msGetPath(path);
  v8context->paths.push(filepath);
  free(filepath);

  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    char err[MS_MAXPATHLEN+21];
    sprintf(err, "Error opening file: %s", path);
    msV8ReportException(NULL, err);
    return Handle<String>(String::New(""));
  }

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = static_cast<int>(fread(&chars[i], 1, size - i, file));
    i += read;
  }

  fclose(file);
  Handle<String> result = String::New(chars, size);
  delete[] chars;
  return result;
}

/* Returns a compiled javascript script. */
static Handle<Script> msV8CompileScript(Handle<String> source, Handle<String> script_name)
{
  TryCatch try_catch;

  if (source.IsEmpty() || source->Length() == 0) {
    msV8ReportException(NULL, "No source to compile");
    return Handle<Script>();
  }

  Handle<Script> script = Script::Compile(source, script_name);
  if (script.IsEmpty() && try_catch.HasCaught()) {
    msV8ReportException(&try_catch);
  }

  return script;
}

/* Runs a compiled script */
static Handle<Value> msV8RunScript(Handle<Script> script)
{
  if (script.IsEmpty()) {
    ThrowException(String::New("No script to run"));
    return Handle<Value>();
  }

  TryCatch try_catch;
  Handle<Value> result = script->Run();

  if (result.IsEmpty() && try_catch.HasCaught()) {
    msV8ReportException(&try_catch);
  }

  return result;
}

/* Execute a javascript file */
static Handle<Value> msV8ExecuteScript(const char *path, int throw_exception = MS_FALSE)
{
  Isolate *isolate = Isolate::GetCurrent();
  V8Context *v8context = (V8Context*)isolate->GetData();

  Handle<Value> source = msV8ReadFile(v8context, path);
  Handle<String> script_name = String::New(msStripPath((char*)path));
  Handle<Script> script = msV8CompileScript(source->ToString(), script_name);
  if (script.IsEmpty()) {
    v8context->paths.pop();
    if (throw_exception) {
      return ThrowException(String::New("Error compiling script"));
    }
  }

  Handle<Value> result = msV8RunScript(script);
  v8context->paths.pop();
  if (result.IsEmpty() && throw_exception) {
    return ThrowException(String::New("Error running script"));
  }

  return result;
}

/* END OF INTERNAL JAVASCRIPT FUNCTIONS */

/* JAVASCRIPT EXPOSED FUNCTIONS */

/* JavaScript Function to load javascript dependencies.
   Exposed to JavaScript as 'require()'. */
static Handle<Value> msV8Require(const Arguments& args)
{
  TryCatch try_catch;

  for (int i = 0; i < args.Length(); i++) {
    String::Utf8Value filename(args[i]);
    msV8ExecuteScript(*filename, MS_TRUE);
    if (try_catch.HasCaught()) {
      return try_catch.ReThrow();
    }
  }

  return Undefined();
}

/* Javascript Function print: print to debug file.
   Exposed to JavaScript as 'print()'. */
static Handle<Value> msV8Print(const Arguments& args)
{
  for (int i = 0; i < args.Length(); i++) {
    String::Utf8Value str(args[i]);
    msDebug("msV8Print: %s\n", *str);
  }

  return Undefined();
}

/* END OF JAVASCRIPT EXPOSED FUNCTIONS */

/* Create and return a v8 context. Thread safe. */
void msV8CreateContext(mapObj *map)
{
  Isolate *isolate = Isolate::GetCurrent();
  V8Context *v8context = new V8Context(isolate);

  HandleScope handle_scope(isolate);

  Handle<ObjectTemplate> global = ObjectTemplate::New();
  global->Set(String::New("require"), FunctionTemplate::New(msV8Require));
  global->Set(String::New("print"), FunctionTemplate::New(msV8Print));
  global->Set(String::New("alert"), FunctionTemplate::New(msV8Print));

  Handle<Context> context_ = Context::New(isolate, NULL, global);
  v8context->context.Reset(isolate, context_);

  v8context->paths.push(map->mappath);
  isolate->SetData(v8context);

  map->v8context = (void*)v8context;
}

void msV8FreeContext(mapObj *map)
{
  V8Context* v8context = V8CONTEXT(map);

  v8context->context.Dispose();
  delete v8context;
}

/* Create a V8 execution context, execute a script and return the feature
 * style. */
char* msV8GetFeatureStyle(mapObj *map, const char *filename, layerObj *layer, shapeObj *shape)
{
  V8Context* v8context = V8CONTEXT(map);

  if (!v8context) {
    msSetError(MS_V8ERR, "V8 Persistent Context is not created.", "msV8ReportException()");
    return NULL;
  }

  HandleScope handle_scope(v8context->isolate);
  /* execution context */
  Local<Context> context = Local<Context>::New(v8context->isolate, v8context->context);
  Context::Scope context_scope(context);
  Handle<Object> global = context->Global();

  /* we don't need this, since the shape object will be free by MapServer */
  /* Persistent<Object> persistent_shape; */
  Handle<Object> shape_ = msV8WrapShapeObj(v8context->isolate, layer, shape, NULL);
  global->Set(String::New("shape"), shape_);

  /* constructor */
  //bad function..
  V8Point p(NULL);
  global->Set(String::New("pointObj"), p.getConstructor());
  V8Line l(NULL);
  global->Set(String::New("lineObj"), l.getConstructor());
              
  Handle<Value> result = msV8ExecuteScript(filename);
  if (!result.IsEmpty() && !result->IsUndefined()) {
    String::AsciiValue ascii(result);
    return msStrdup(*ascii);
  }

  return NULL;
}

#endif /* USE_V8_MAPSCRIPT */
