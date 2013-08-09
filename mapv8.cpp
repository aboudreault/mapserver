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
static Handle<String> readfile(const char *name)
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

    Handle<String> source = readfile(*str);
    if (source.IsEmpty())
    {
      return ThrowException(String::New("Error loading file"));
    }

    Handle<Script> script = Script::Compile(source);
    return script->Run();    
  }

  return Undefined();
}

/* Create and return a v8 context. Thread safe. */
static Handle<Context> v8_create_context(Isolate *isolate)
{
  Handle<ObjectTemplate> global = ObjectTemplate::New();

  // We should  also write print, read load and quit handlers

  global->Set(String::New("require"), FunctionTemplate::New(require));

  return Context::New(isolate, NULL, global);
}

int test_v8()
{
  TryCatch try_catch;

  Isolate* isolate = Isolate::GetCurrent();

  // Create a stack-allocated handle scope.
  HandleScope handle_scope(isolate);

  Handle<Context> context = v8_create_context(isolate);

  // Here's how you could create a Persistent handle to the context, if needed.
  Persistent<Context> persistent_context(isolate, context);
  
  // Enter the created context for compiling and
  // running the hello world script. 
  Context::Scope context_scope(context);

  // Create a string containing the JavaScript source code.
  //Handle<String> source = String::New("'Hello' + ', World!'");

  Handle<String> source = readfile("/usr/src/mapserver/mapserver-master/test.js");
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
      const char* exception_string = "dddd";//*exception;
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
