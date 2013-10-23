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

#include "v8_mapscript.h"

static void msV8LineObjDestroy(Isolate *isolate, Persistent<Object> *object,
                               lineObj *line)
{
  free(line->point);
  free(line);
  object->Dispose();
  object->Clear();
}

Handle<Value> msV8LineObjNew(const Arguments& args)
{
  Local<Object> self = args.Holder();

  if (!self->Has(String::New("__obj__"))) {
    lineObj *line = (lineObj *)msSmallMalloc(sizeof(lineObj));
    
    line->numpoints=0;
    line->point=NULL;

    V8Line::setInternalField(self, line);
    Persistent<Object> pobj;
    pobj.Reset(Isolate::GetCurrent(), self);
    pobj.MakeWeak(line, msV8LineObjDestroy);        
  }

  return self;
}

Handle<Value> msV8LineObjGetPoint(const Arguments& args)
{
  if (args.Length() < 1 || !args[0]->IsInt32()) {
    ThrowException(String::New("Invalid argument"));
    return Undefined();
  }

  int index = args[0]->Int32Value();
  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  lineObj *line = static_cast<lineObj*>(ptr);

  if (index < 0 || index >= line->numpoints)
  {
    ThrowException(String::New("Invalid point index."));
    return Undefined();
  }

  Point *point = new Point(&line->point[index]);
  Handle<Value> ext = External::New(point);  
  return Point::Constructor()->NewInstance(1, &ext);
}

Handle<Value> msV8LineObjAddXY(const Arguments& args)
{
  if (args.Length() < 2 || !args[0]->IsNumber() || !args[1]->IsNumber()) {
    ThrowException(String::New("Invalid argument"));
    return Undefined();
  }

  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  lineObj *line = static_cast<lineObj*>(ptr);

  if(line->numpoints == 0) /* new */
    line->point = (pointObj *)msSmallMalloc(sizeof(pointObj));
  else /* extend array */
    line->point = (pointObj *)msSmallRealloc(line->point, sizeof(pointObj)*(line->numpoints+1));

  line->point[line->numpoints].x = args[0]->NumberValue();
  line->point[line->numpoints].y = args[1]->NumberValue();
#ifdef USE_POINT_Z_M
  if (args.Length() > 2 && args[2]->IsNumber())
    line->point[line->numpoints].m = args[2]->NumberValue();
#endif

  line->numpoints++;

  return Undefined();
}

Handle<Value> msV8LineObjAddXYZ(const Arguments& args)
{
  if (args.Length() < 3 || !args[0]->IsNumber() ||
      !args[1]->IsNumber() || !args[2]->IsNumber()) {
    ThrowException(String::New("Invalid argument"));
    return Undefined();
  }

  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  lineObj *line = static_cast<lineObj*>(ptr);

  if(line->numpoints == 0) /* new */
    line->point = (pointObj *)msSmallMalloc(sizeof(pointObj));
  else /* extend array */
    line->point = (pointObj *)msSmallRealloc(line->point, sizeof(pointObj)*(line->numpoints+1));

  line->point[line->numpoints].x = args[0]->NumberValue();
  line->point[line->numpoints].y = args[1]->NumberValue();
#ifdef USE_POINT_Z_M
  line->point[line->numpoints].z = args[2]->NumberValue();
  if (args.Length() > 3 && args[3]->IsNumber())
    line->point[line->numpoints].m = args[3]->NumberValue();
#endif
  line->numpoints++;

  return Undefined();
}

Handle<Value> msV8LineObjAddPoint(const Arguments& args)
{
  if (args.Length() < 1 || !args[0]->IsObject() ||
      !args[0]->ToObject()->GetConstructorName()->Equals(String::New("pointObj"))) {
    ThrowException(String::New("Invalid argument"));
    return Undefined();
  }

  Local<Object> point_obj = args[0]->ToObject();
  Local<Object> self = args.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void *ptr = wrap->Value();
  lineObj *line = static_cast<lineObj*>(ptr);
  wrap = Local<External>::Cast(point_obj->GetInternalField(0));
  ptr = wrap->Value();
  pointObj *point = static_cast<pointObj*>(ptr);

  if(line->numpoints == 0) /* new */
    line->point = (pointObj *)msSmallMalloc(sizeof(pointObj));
  else /* extend array */
    line->point = (pointObj *)msSmallRealloc(line->point, sizeof(pointObj)*(line->numpoints+1));

  line->point[line->numpoints].x = point->x;
  line->point[line->numpoints].y = point->y;
#ifdef USE_POINT_Z_M
  line->point[line->numpoints].z = point->z;
  if (args.Length() > 3 && args[3]->IsNumber())
    line->point[line->numpoints].m = point->m;
#endif
  line->numpoints++;

  return Undefined();
}

#endif
