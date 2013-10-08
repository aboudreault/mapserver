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
#include <string>
#include <stack>
#include <map>
#include <v8.h>

using std::string;
using std::stack;
using std::map;
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
using v8::Integer;
using v8::Number;
using v8::Undefined;
using v8::Null;
using v8::Function;
using v8::FunctionTemplate;
using v8::ObjectTemplate;
using v8::AccessorInfo;
using v8::None;
using v8::ReadOnly;
using v8::PROHIBITS_OVERWRITING;
using v8::External;
using v8::Arguments;
using v8::TryCatch;
using v8::Message;
using v8::ThrowException;

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

#define ADD_GETTER(name, property_type, property, v8_type) this->obj_template->SetAccessor(String::New(name), \
                   msV8Getter<T, property_type, &T::property, v8_type>, \
                   0, \
                   Handle<Value>(), \
                   PROHIBITS_OVERWRITING, \
                   ReadOnly)

#define ADD_DOUBLE_GETTER(name, property) ADD_GETTER(name, double, property, Number)
#define ADD_INTEGER_GETTER(name, property) ADD_GETTER(name, int, property, Integer)
#define ADD_STRING_GETTER(name, property) ADD_GETTER(name, char*, property, String)

#define ADD_ACCESSOR(name, property_type, property, v8_type) this->obj_template->SetAccessor(String::New(name), \
                     getter<property_type, &T::property, v8_type>, \
                     msV8Setter<T, property_type, &T::property>, \
                     Handle<Value>(), \
                     PROHIBITS_OVERWRITING, \
                     None)

#define ADD_DOUBLE_ACCESSOR(name, property) ADD_ACCESSOR(name, double, property, Number)
#define ADD_INTEGER_ACCESSOR(name, property) ADD_ACCESSOR(name, double, property, Integer)
#define ADD_STRING_ACCESSOR(name, property) ADD_ACCESSOR(name, char*, property, String)

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
static Handle<Object> msV8WrapLineObj(Isolate *isolate, lineObj *line,
                                      Handle<Object> parent);
static Handle<Object> msV8WrapPointObj(Isolate *isolate, pointObj *point,
                                       Handle<Object> parent);

template<typename T>
class V8Object
{
 private:
  template <typename V, V T::*mptr, typename R>
    static Handle<Value> getter(Local<String> property,
                                const AccessorInfo &info);
 protected:
  T* obj;
  Handle<ObjectTemplate> obj_template;
  
 public: 
  V8Object(T* obj);

  
};
#endif
