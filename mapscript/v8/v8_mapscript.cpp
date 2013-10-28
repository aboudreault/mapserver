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

char* getStringValue(Local<Value> value, const char *fallback)
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

template <typename T, typename V, V T::*mptr, typename R>
Handle<Value> getter(Local<String> property,
                     const AccessorInfo &info)
{
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  T *o = static_cast<T*>(ptr);

  return R::New(o->*mptr);
}

template<typename T, char* T::*mptr, typename R>
Handle<Value> getter(Local<String> property,
                                        const AccessorInfo &info)
{
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  T *o = static_cast<T*>(ptr);

  if (o->*mptr == NULL)
    return R::New("");

  return R::New(o->*mptr);
}

template<typename T, typename V, V T::*mptr>
void setter(Local<String> property, Local<Value> value,
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

template<typename T, char* T::*mptr>
void setter(Local<String> property, Local<Value> value,
            const AccessorInfo &info)
{
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  char *cvalue = static_cast<T*>(ptr)->*mptr;

  if (cvalue)
    msFree(cvalue);

  static_cast<T*>(ptr)->*mptr = V8Object<T>::getStringValue(value);
}

// template<typename T>
// template <typename V, V T::*mptr, typename R>
// Handle<Value> V8Object<T>::getter(Local<String> property,
//                                   const AccessorInfo &info)
// {
//   Local<Object> self = info.Holder();
//   Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
//   void *ptr = wrap->Value();
//   T *o = static_cast<T*>(ptr);

//   return R::New(o->*mptr);
// }

// template<typename T>
// template<char* T::*mptr, typename R>
// Handle<Value> V8Object<T>::getter(Local<String> property,
//                                   const AccessorInfo &info)
// {
//   Local<Object> self = info.Holder();
//   Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
//   void *ptr = wrap->Value();
//   T *o = static_cast<T*>(ptr);

//   if (o->*mptr == NULL)
//     return R::New("");

//   return R::New(o->*mptr);
// }

// template<typename T>
// template<typename V, V T::*mptr>
// void V8Object<T>::setter(Local<String> property, Local<Value> value,
//                          const AccessorInfo &info)
// {
//   Local<Object> self = info.Holder();
//   Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
//   void *ptr = wrap->Value();
//   if (value->IsInt32()) {
//     static_cast<T*>(ptr)->*mptr = value->Int32Value();
//   }
//   else if (value->IsNumber()) {
//     static_cast<T*>(ptr)->*mptr = value->NumberValue();
//   }
// }

// template<typename T>
// template<char* T::*mptr>
// void V8Object<T>::setter(Local<String> property, Local<Value> value,
//                          const AccessorInfo &info)
// {
//   Local<Object> self = info.Holder();
//   Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
//   void *ptr = wrap->Value();
//   char *cvalue = static_cast<T*>(ptr)->*mptr;

//   if (cvalue)
//     msFree(cvalue);

//   static_cast<T*>(ptr)->*mptr = getStringValue(value);

// }

template<typename T>
char* V8Object<T>::getStringValue(Local<Value> value, const char *fallback)
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

template<typename T>
V8Object<T>::V8Object(T* obj, Handle<Object> parent)
{
  this->obj = obj;

  if (!parent.IsEmpty()) {
    this->parent = parent;
  }

  this->obj_template = ObjectTemplate::New();
  this->obj_template->SetInternalFieldCount(1);
  //  this->func_template = makeObjectTemplate<T>(this->obj);
}

template<typename T>
Handle<Object> V8Object<T>::newInstance()
{
  Handle<String> key = String::New("__obj__");
  Handle<String> key_parent = String::New("__parent__");
  Handle<External> external_obj = External::New(this->obj);

  this->func_template->InstanceTemplate()->Set(key, external_obj);
  if (!parent.IsEmpty())
    this->func_template->InstanceTemplate()->Set(key_parent, parent);
  Handle<Object> obj = this->func_template->GetFunction()->NewInstance();

  obj->SetInternalField(0, external_obj);
  obj->Delete(key);

  if (!parent.IsEmpty()) {
    obj->SetHiddenValue(String::New("__parent__"), this->parent);
  }

  return obj;
}

template<typename T>
void V8Object<T>::setInternalField(Handle<Object> obj, T *p)
{
  obj->SetInternalField(0, External::New(p));
}

template<typename T>
void V8Object<T>::addFunction(const char* name, InvocationCallback function)
{
  this->func_template->InstanceTemplate()->Set(String::New(name),
                          FunctionTemplate::New(function));
}

template <typename T>
Handle<Function> V8Object<T>::getConstructor()
{
  return this->func_template->GetFunction();
}

#endif
