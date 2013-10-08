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

template<typename T>
V8Object<T>::V8Object(T* obj)
{
  this->obj = obj;
  this->obj_template = ObjectTemplate::New();
  this->obj_template->SetInternalFieldCount(1);
  ADD_DOUBLE_ACCESSOR("x", x);
}

template<typename T>
template <typename V, V T::*mptr, typename R>
Handle<Value> V8Object<T>::getter(Local<String> property,
                                  const AccessorInfo &info)
{
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  T *o = static_cast<T*>(ptr);
  
  return R::New(o->*mptr);
}

template class V8Object<pointObj>;

#endif
