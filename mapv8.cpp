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
#include <fstream>
#include <streambuf>
#include <v8.h>

using namespace v8;

/* This function load a javascript file in memory (v8::String) */
static Handle<String> msV8ReadFile(const char *name)
{
  FILE* file = fopen(name, "rb");
  if (file == NULL) return Handle<String>();

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

/* Function to load javascript dependencies */
static Handle<Value> require(const Arguments& args)
{
  for (int i = 0; i < args.Length(); i++) {
    String::Utf8Value str(args[i]);

    Handle<String> source = msV8ReadFile(*str);
    if (source.IsEmpty())
    {
      return ThrowException(String::New("Error loading file"));
    }

    Handle<Script> script = Script::Compile(source);
    return script->Run();    
  }

  return Undefined();
}

/* Function alert: print to debug file */
static Handle<Value> alert(const Arguments& args)
{
  for (int i = 0; i < args.Length(); i++) {
    String::Utf8Value str(args[i]);
    msDebug("msV8ExecuteScript: %s\n", *str);
  }

  return Undefined();
}

/* Create and return a v8 context. Thread safe. */
static Handle<Context> msV8CreateContext(Isolate *isolate, Handle<ObjectTemplate> global)
{
//Handle<ObjectTemplate> global = ObjectTemplate::New();

  // We should  also write print, read load and quit handlers

  global->Set(String::New("require"), FunctionTemplate::New(require));
  global->Set(String::New("alert"), FunctionTemplate::New(alert));

  return Context::New(isolate, NULL, global);
}

char* msV8ExecuteScript(const char *filename, layerObj *layer, shapeObj *shape)
{
  TryCatch try_catch;  
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope handle_scope(isolate);
  int i;

  Handle<ObjectTemplate> global = ObjectTemplate::New();
  Handle<ObjectTemplate> shape_attributes = ObjectTemplate::New();
  for (i=0; i<layer->numitems;++i) {
    shape_attributes->Set(String::New(layer->items[i]),
                          String::New(shape->values[i]));
  }

  global->Set(String::New("shape_attributes"), shape_attributes);

  Handle<Context> context = msV8CreateContext(isolate, global);
  Context::Scope context_scope(context);

  Handle<String> source = msV8ReadFile(filename);
  if (source.IsEmpty())
  {
    msDebug("msV8ExecuteScript(): Invalid or empty Javascript file: \"%s\".\n", filename);
    //ThrowException(String::New("Error loading file"));
    return msStrdup("");
  }

  Handle<Script> script = Script::Compile(source);
  if (script.IsEmpty()) {
    if (try_catch.HasCaught()) {
      String::Utf8Value exception(try_catch.Exception());
      const char* exception_string = *exception;
      Handle<Message> message = try_catch.Message();
      if (!message.IsEmpty()) {
        msSetError(MS_MISCERR, "Javascript Exception: \"%s\".", "msV8ExecuteScript()", exception_string);
      }
    }
    return msStrdup("");
  }
    
  Handle<Value> result = script->Run();
  if (result.IsEmpty()) {
    if (try_catch.HasCaught()) {
      String::Utf8Value exception(try_catch.Exception());
      const char* exception_string = *exception;
      Handle<Message> message = try_catch.Message();
      if (!message.IsEmpty()) {
        msSetError(MS_MISCERR, "Javascript Exception: \"%s\".", "msV8ExecuteScript()", exception_string);
      }
    }
  } else if (!result.IsEmpty() && !result->IsUndefined()) {
    String::AsciiValue ascii(result);
    return msStrdup(*ascii);
  }

  return msStrdup("");
}

int test_v8()
{
  TryCatch try_catch;

  Isolate* isolate = Isolate::GetCurrent();

  // Create a stack-allocated handle scope.
  HandleScope handle_scope(isolate);
  Handle<ObjectTemplate> global = ObjectTemplate::New();
  Handle<Context> context = msV8CreateContext(isolate, global);

  // Here's how you could create a Persistent handle to the context, if needed.
  Persistent<Context> persistent_context(isolate, context);
  
  // Enter the created context for compiling and
  // running the hello world script. 
  Context::Scope context_scope(context);

  // Create a string containing the JavaScript source code.
  //Handle<String> source = String::New("'Hello' + ', World!'");

  Handle<String> source = msV8ReadFile("/usr/src/mapserver/mapserver-master/test.js");
  if (source.IsEmpty())
  {
    ThrowException(String::New("Error loading file"));
    return MS_FAILURE;
  }

  // Compile the source code.
  Handle<Script> script = Script::Compile(source);
  
  // Run the script to get the result.
  Handle<Value> result = script->Run();
  if (result.IsEmpty()) {
    if (try_catch.HasCaught()) {
      String::Utf8Value exception(try_catch.Exception());
      const char* exception_string = *exception;
       printf("--> %s\n", exception_string);
      Handle<Message> message = try_catch.Message();
      if (!message.IsEmpty()) {
        printf("--> %s\n", exception_string);
      }
    }
  } else if (!result->IsUndefined()) {
    String::AsciiValue ascii(result);
    printf("%s\n", *ascii);
  }


  // The persistent handle needs to be eventually disposed.
  persistent_context.Dispose();

  return 0;
}

#endif /* USE_V8 */
