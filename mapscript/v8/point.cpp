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

static void msV8PointObjDestroy(Isolate *isolate, Persistent<Object> *object,
                                pointObj *point)
{
  msFree(point);
  object->Dispose();
  object->Clear();
}

Handle<Value> msV8PointObjNew(const Arguments& args)
{
  Local<Object> self = args.Holder();

  if (!self->Has(String::New("__obj__"))) {
    pointObj *p = (pointObj *)msSmallMalloc(sizeof(pointObj));
    
    p->x = 0;
    p->y = 0;
#ifdef USE_POINT_Z_M
    p->z = 0;
    p->m = 0;
#endif
  
    V8Point::setInternalField(self, p);
    Persistent<Object> pobj;
    pobj.Reset(Isolate::GetCurrent(), self);
    pobj.MakeWeak(p, msV8PointObjDestroy);    
  }

  return self;  
}

Handle<Value> msV8PointObjSetXY(const Arguments& args)
{
  if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsNumber()) {
    ThrowException(String::New("Invalid argument"));
    return Undefined();
  }

  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  pointObj *point = static_cast<pointObj*>(ptr);

  point->x = args[0]->NumberValue();
  point->y = args[1]->NumberValue();

  return Undefined();
}

Handle<Value> msV8PointObjSetXYZ(const Arguments& args)
{
  if (args.Length() < 3 ||
      !args[0]->IsNumber() || !args[1]->IsNumber() || !args[2]->IsNumber()) {
    ThrowException(String::New("Invalid argument"));
    return Undefined();
  }

  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  pointObj *point = static_cast<pointObj*>(ptr);

  point->x = args[0]->NumberValue();
  point->y = args[1]->NumberValue();

#ifdef USE_POINT_Z_M
  point->z = args[2]->NumberValue();
  if (args.Length() > 3 && args[3]->IsNumber()) {
    point->m = args[3]->NumberValue();
  }
#endif
  return Undefined();
}


#endif
