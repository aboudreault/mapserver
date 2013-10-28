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

#ifndef V8_I_H
#define V8_I_H

#include "mapserver-config.h"
#ifdef USE_V8_MAPSCRIPT

#include <v8.h>

using namespace v8;

Handle<Value> msV8ShapeObjNew(const Arguments& args);
Handle<Value> msV8ShapeObjClone(const Arguments& args);
Handle<Value> msV8ShapeObjGetLine(const Arguments& args);
Handle<Value> msV8ShapeObjAddLine(const Arguments& args);
void  msV8ShapeObjGetValue(Local<String> name,
                           const PropertyCallbackInfo<Value> &info);
void msV8ShapeObjSetValue(Local<String> name,
                          Local<Value> value,
                          const PropertyCallbackInfo<Value> &info);
Handle<Value> msV8ShapeObjSetGeometry(const Arguments& args);

#endif

#endif
