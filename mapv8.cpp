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
#ifdef USE_V8

#include "mapserver.h"
#include <string>
#include <stack>
#include <streambuf>
#include <v8.h>

using std::string;
using std::stack;
using v8::Isolate;
using v8::Context;
using v8::Persistent;
using v8::HandleScope;
using v8::Handle;
using v8::Script;
using v8::Local;
using v8::Object;
using v8::Value;
using v8::String;
using v8::Undefined;
using v8::FunctionTemplate;
using v8::ObjectTemplate;
using v8::Arguments;
using v8::TryCatch;
using v8::Message;
using v8::ThrowException;

class V8Context {
public:
  V8Context(Isolate *isolate)
    : isolate(isolate) {}
  Isolate *isolate;
  stack<string> paths; /* for relative paths and the require directive */
  Persistent<Context> context;
};

#define V8CONTEXT(map) ((V8Context*) (map)->v8context)

/* INTERNAL JAVASCRIPT FUNCTIONS */

/* Get c char from a v8 string. Caller has to free the returned value. */
static char *msV8GetCString(Local<Value> value, const char *fallback = "") {
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

/* This function load a javascript file in memory for its execution. */
static Handle<Value> msV8ReadFile(V8Context* v8context, const char *name)
{
  char path[MS_MAXPATHLEN];

  /* construct the path */
  msBuildPath(path, v8context->paths.top().c_str(), name);
  char *filepath = msGetPath(path);
  v8context->paths.push(filepath);
  free(filepath);
  
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    return ThrowException(String::New("Error loading file"));
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

/* Handler for Javascript Exceptions. Not exposed to JavaScript, used internally.
   Most of the code from v8 shell example.
*/
void msV8ReportException(TryCatch* try_catch, const char *msg = "") {
  HandleScope handle_scope;

  if (!try_catch || !try_catch->HasCaught()) {
    msSetError(MS_V8ERR, "Error: %s.", "msV8ReportException()", msg);
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
    msSetError(MS_V8ERR, "Javascript Exception: %s", "msV8ReportException()",
	       sourceline_string);
    String::Utf8Value stack_trace(try_catch->StackTrace());
    if (stack_trace.length() > 0) {
      const char* stack_trace_string = *stack_trace;
      msSetError(MS_V8ERR, "Javascript Exception: %s", "msV8ReportException()",
		 stack_trace_string);
    }
  }
}

/* Returns a compiled javascript script. */
static Handle<Script> msV8CompileScript(Handle<String> source, Handle<String> script_name)
{
  if (source.IsEmpty() || source->Length() == 0) {
    ThrowException(String::New("No source to compile"));
    return Handle<Script>();
  }

  Handle<Script> script = Script::Compile(source, script_name);
  if (script.IsEmpty()) {
    return Handle<Script>();
  }

  return script;
}

/* Runs a compiled script and return the result */
static Handle<Value> msV8RunScript(Handle<Script> script)
{
  if (script.IsEmpty()) {
    ThrowException(String::New("No script to run"));
    return Handle<Value>();
  }

  TryCatch try_catch;  
  Handle<Value> result = script->Run();
  if (result.IsEmpty()) {
    return try_catch.HasCaught() ? try_catch.ReThrow():
      ThrowException(String::New("Error running script"));
  }

  return result;
}

/* END OF INTERNAL JAVASCRIPT FUNCTIONS */

/* JAVASCRIPT EXPOSED FUNCTIONS */

/* JavaScript Function to load javascript dependencies.
   Exposed to JavaScript as 'require()'. */
static Handle<Value> msV8Require(const Arguments& args)
{
  Isolate *isolate = Isolate::GetCurrent();
  V8Context *v8context = (V8Context*)isolate->GetData();
    
  for (int i = 0; i < args.Length(); i++) {
    String::Utf8Value str(args[i]);

    Handle<Value> source = msV8ReadFile(v8context, *str);
    Handle<String> script_name = String::New(msStripPath(*str));
    TryCatch try_catch;
    Handle<Script> script = msV8CompileScript(source->ToString(), script_name);
    if (script.IsEmpty()) {
        return try_catch.HasCaught() ? try_catch.ReThrow():
          ThrowException(String::New("Error compiling script"));
    }
    
    Handle<Value> result = msV8RunScript(script);
    v8context->paths.pop();
    if (result.IsEmpty()) {
      return try_catch.HasCaught() ? try_catch.ReThrow():
        ThrowException(String::New("Error executing script"));
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

/* temporary test function. this method will return a Handle<Value>. */
char* msV8ExecuteScript(mapObj *map, const char *filename, layerObj *layer, shapeObj *msshape)
{
  V8Context* v8context = V8CONTEXT(map);
  int i;
  HandleScope handle_scope(v8context->isolate);

  Local<Context> context = Local<Context>::New(v8context->isolate, v8context->context);

  Context::Scope context_scope(context);  
  Handle<Object> global = context->Global();

  /* This will be a proper object exposed to JS with dynamic access to attributes (and not a copy),
     currently only for test purpose. */
  Handle<ObjectTemplate> shape = ObjectTemplate::New();
  Handle<ObjectTemplate> attributes = ObjectTemplate::New();
  for (i=0; i<layer->numitems; ++i) {
    attributes->Set(String::New(layer->items[i]),
                    String::New(msshape->values[i]));
  }

  shape->Set(String::New("attributes"), attributes);
  global->Set(String::New("shape"), shape->NewInstance());  

  TryCatch try_catch;
  Handle<Value> source = msV8ReadFile(v8context, filename);
  if (source.IsEmpty()) {
    msDebug("msV8ExecuteScript(): Invalid or empty Javascript file: \"%s\".\n", filename);
    return msStrdup("");
  }

  Handle<String> script_name = String::New(msStripPath((char*)filename));
  Handle<Script> script = Script::Compile(source->ToString(), script_name);
  if (script.IsEmpty()) {
    if (try_catch.HasCaught()) {
      msV8ReportException(&try_catch);
    }
    return msStrdup("");
  }

  Handle<Value> result = script->Run();
  v8context->paths.pop();  
  if (result.IsEmpty()) {
    if (try_catch.HasCaught()) {
      msV8ReportException(&try_catch);
    }
  } else if (!result.IsEmpty() && !result->IsUndefined()) {
    String::AsciiValue ascii(result);
    return msStrdup(*ascii);
  }

  return msStrdup("");
}

#endif /* USE_V8 */
