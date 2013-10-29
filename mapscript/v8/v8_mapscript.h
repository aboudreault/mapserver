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

#ifndef V8_MAPSCRIPT_H
#define V8_MAPSCRIPT_H

#include "mapserver-config.h"
#ifdef USE_V8_MAPSCRIPT

#define V8_ALLOW_ACCESS_TO_RAW_HANDLE_CONSTRUCTOR 1

#include "mapserver.h"
#include <v8.h>
#include <string>
#include <stack>
#include "v8_object_wrap.hpp"
#include "point.hpp"
#include "line.hpp"
#include "shape.hpp"

using namespace v8;

using std::string;
using std::stack;

class V8Context
{
public:
  V8Context(Isolate *isolate)
    : isolate(isolate) {}
  Isolate *isolate;
  stack<string> paths; /* for relative paths and the require function */
  Persistent<Context> context;
};

#define V8CONTEXT(map) ((V8Context*) (map)->v8context)

inline void NODE_SET_PROTOTYPE_METHOD(v8::Handle<v8::FunctionTemplate> recv,
                                      const char* name,
                                      v8::FunctionCallback callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(callback);
  recv->InstanceTemplate()->Set(v8::String::NewFromUtf8(isolate, name),
                                t->GetFunction());
}
#define NODE_SET_PROTOTYPE_METHOD NODE_SET_PROTOTYPE_METHOD

#define TOSTR(obj) (*String::Utf8Value((obj)->ToString()))

#define SET(target, name, value)                 \
  (target)->PrototypeTemplate()->Set(String::NewSymbol(name), value);

#define SET_ATTRIBUTE(t, name, get, set)   \
  t->InstanceTemplate()->SetAccessor(String::NewSymbol(name), get, set)

#define SET_ATTRIBUTE_RO(t, name, get)             \
  t->InstanceTemplate()->SetAccessor(              \
        String::NewSymbol(name),                   \
        get, 0,                                    \
        Handle<Value>(),                           \
        DEFAULT,                                   \
        static_cast<PropertyAttribute>(            \
          ReadOnly|DontDelete))

#define NODE_DEFINE_CONSTANT(target, name, constant)     \
    (target)->Set(String::NewSymbol(name),               \
                  Integer::New(constant),                \
                  static_cast<PropertyAttribute>(        \
                  ReadOnly|DontDelete));

char* getStringValue(Local<Value> value, const char *fallback="");

// kill
/* those getter/setter cannot be function due to c++ templating
   limitation. another solution could be used if/when needed. */
#define ADD_GETTER(name, property_type, property, v8_type) func_template->InstanceTemplate()->SetAccessor(String::New(name), \
                   getter<T, property_type, &T::property, v8_type>, \
                   0, \
                   Handle<Value>(), \
                   PROHIBITS_OVERWRITING, \
                   ReadOnly)

#define ADD_DOUBLE_GETTER(name, property) ADD_GETTER(name, double, property, Number)
#define ADD_INTEGER_GETTER(name, property) ADD_GETTER(name, int, property, Integer)
#define ADD_LONG_GETTER(name, property) ADD_GETTER(name, long, property, Integer)
#define ADD_STRING_GETTER(name, property) func_template->InstanceTemplate()->SetAccessor(String::New(name), \
                   getter<T, &T::property, String>, \
                   0, \
                   Handle<Value>(), \
                   PROHIBITS_OVERWRITING, \
                   ReadOnly)

#define ADD_ACCESSOR(name, property_type, property, v8_type) func_template->InstanceTemplate()->SetAccessor(String::New(name), \
                     getter<T, property_type, &T::property, v8_type>, \
                     setter<T, property_type, &T::property>, \
                     Handle<Value>(), \
                     PROHIBITS_OVERWRITING, \
                     None)

#define ADD_DOUBLE_ACCESSOR(name, property) ADD_ACCESSOR(name, double, property, Number)
#define ADD_INTEGER_ACCESSOR(name, property) ADD_ACCESSOR(name, int, property, Integer)
#define ADD_LONG_ACCESSOR(name, property) ADD_ACCESSOR(name, long, property, Integer)
#define ADD_STRING_ACCESSOR(name, property) func_template->InstanceTemplate()->SetAccessor(String::New(name), \
                     getter<T, &T::property, String>, \
                     setter<T, &T::property>, \
                     Handle<Value>(), \
                     PROHIBITS_OVERWRITING, \
                     None)

#define ADD_FUNCTION(name, function)  func_template->InstanceTemplate()->Set(String::New(name), FunctionTemplate::New(function));

template<typename T>
class V8Object
{
 private:
  void addFunction(const char* name, InvocationCallback function);

 protected:
  T* obj;
  Handle<ObjectTemplate> obj_template;
  Handle<String> classname;
  Handle<Object> parent;
  Handle<Object> value; /* v8 value */
  Handle<FunctionTemplate> func_template;

 public:
  V8Object(T* obj, Handle<Object> parent = Handle<Object>());

  Handle<Object> newInstance();

  Handle<Function> getConstructor();
  static void setInternalField(Handle<Object> obj, T *p);
  static char *getStringValue(Local<Value> value, const char *fallback="");
};

#endif

#endif
