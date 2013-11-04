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

#include "mapserver.h"
#include "v8_mapscript.h"

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

  Point::Initialize(global);
  Line::Initialize(global);
  Shape::Initialize(global);

  /* we don't need this, since the shape object will be free by MapServer */
  /* Persistent<Object> persistent_shape; */
  Shape *shape_ = new Shape(shape);
  shape_->setLayer(layer); // hack, should change in a near future. 
  Handle<Value> ext = External::New(shape_);      
  global->Set(String::New("shape"),
              Shape::Constructor()->NewInstance(1, &ext));
  shape_->addRef(); /* this object should not be freed by the v8 GC */

  Handle<Value> result = msV8ExecuteScript(filename);
  if (!result.IsEmpty() && !result->IsUndefined()) {
    String::AsciiValue ascii(result);
    free(shape_);
    return msStrdup(*ascii);
  }

  free(shape_);
  return NULL;
}

/* for geomtransform, we don't have the mapObj */
shapeObj *msV8TransformShape(shapeObj *shape, const char* filename)
{
  Isolate *isolate = Isolate::GetCurrent();
  V8Context *v8context = (V8Context*)isolate->GetData();
  
  if (*filename == '\0') {
    msSetError(MS_V8ERR, "Invalid javascript filename: \"%s\".", "msGeomTransformShape()", filename);
    return NULL;
  }

  HandleScope handle_scope(v8context->isolate);
  /* execution context */
  Local<Context> context = Local<Context>::New(v8context->isolate, v8context->context);
  Context::Scope context_scope(context);
  Handle<Object> global = context->Global();

  /* we don't need this, since the shape object will be free by MapServer */
  /* Persistent<Object> persistent_shape; */
  // Handle<ObjectTemplate> layer_obj_templ = ObjectTemplate::New();
  // layer_obj_templ->SetInternalFieldCount(1);
  // Handle<Object> layer_obj = layer_obj_templ->NewInstance();
  // layer_obj->SetInternalField(0, External::New(layer));   
  Shape* shape_ = new Shape(shape);
  Handle<Value> ext = External::New(shape_);      
  global->Set(String::New("shape"),
              Shape::Constructor()->NewInstance(1, &ext));

  Handle<Value> result = msV8ExecuteScript(filename);
  if (!result.IsEmpty() && result->IsObject()) {
    Local<External> wrap = Local<External>::Cast(result->ToObject()->GetInternalField(0));
    void *ptr = wrap->Value();
    shapeObj *new_shape = static_cast<shapeObj*>(ptr);
    if (shape == new_shape) {
      new_shape = (shapeObj *)msSmallMalloc(sizeof(shapeObj));
      msInitShape(new_shape);
      msCopyShape(shape, new_shape);
      return new_shape;
    }
    else
    {
      return new_shape;
    }
  }
  
  return NULL;
}
                             
#endif /* USE_V8_MAPSCRIPT */
