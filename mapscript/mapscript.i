/* ===========================================================================
   $Id$
 
   Project:  MapServer
   Purpose:  SWIG interface file for the MapServer mapscript module
   Author:   Steve Lime 
             Sean Gillies, sgillies@frii.com
             
   ===========================================================================
   Copyright (c) 1996-2001 Regents of the University of Minnesota.
   
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:
 
   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.
 
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
   =========================================================================*/
   

%module mapscript

%{
#include "../../map.h"
#include "../../maptemplate.h"
#include "../../mapogcsld.h"
#include "../../mapows.h"
#include "../../cgiutil.h"
#include "../../mapcopy.h"
#include "../../maperror.h"
#include "../../mapprimitive.h"
#include "../../mapshape.h"

#ifdef SWIGPYTHON
#include "pygdioctx/pygdioctx.h"
#endif

%}

// Problem with SWIG CSHARP typemap for pointers
#ifndef SWIGCSHARP
%include typemaps.i
#endif

%include constraints.i

%include carrays.i
%array_class(int, intarray)

/* ===========================================================================
   Supporting 'None' as an argument to attribute accessor functions

   Typemaps to support NULL in attribute accessor functions
   provided to Steve Lime by David Beazley and tested for Python
   only by Sean Gillies.
   
   With the use of these typemaps we can execute statements like

     layer.group = None

   ========================================================================= */

#ifdef __cplusplus
%typemap(memberin) char * {
  if ($1) delete [] $1;
  if ($input) {
     $1 = ($1_type) (new char[strlen($input)+1]);
     strcpy((char *) $1,$input);
  } else {
     $1 = 0;
  }
}
%typemap(memberin,warning="451:Setting const char * member may leak
memory.") const char * {
  if ($input) {
     $1 = ($1_type) (new char[strlen($input)+1]);
     strcpy((char *) $1,$input);
  } else {
     $1 = 0;
  }
}
%typemap(globalin) char * {
  if ($1) delete [] $1;
  if ($input) {
     $1 = ($1_type) (new char[strlen($input)+1]);
     strcpy((char *) $1,$input);
  } else {
     $1 = 0;
  }
}
%typemap(globalin,warning="451:Setting const char * variable may leak
memory.") const char * {
  if ($input) {
     $1 = ($1_type) (new char[strlen($input)+1]);
     strcpy((char *) $1,$input);
  } else {
     $1 = 0;
  }
}
#else
%typemap(memberin) char * {
  if ($1) free((char*)$1);
  if ($input) {
     $1 = ($1_type) malloc(strlen($input)+1);
     strcpy((char*)$1,$input);
  } else {
     $1 = 0;
  }
}
%typemap(memberin,warning="451:Setting const char * member may leak
memory.") const char * {
  if ($input) {
     $1 = ($1_type) malloc(strlen($input)+1);
     strcpy((char*)$1,$input);
  } else {
     $1 = 0;
  }
}
%typemap(globalin) char * {
  if ($1) free((char*)$1);
  if ($input) {
     $1 = ($1_type) malloc(strlen($input)+1);
     strcpy((char*)$1,$input);
  } else {
     $1 = 0;
  }
}
%typemap(globalin,warning="451:Setting const char * variable may leak
memory.") const char * {
  if ($input) {
     $1 = ($1_type) malloc(strlen($input)+1);
     strcpy((char*)$1,$input);
  } else {
     $1 = 0;
  }
}
#endif // __cplusplus

/* =========================================================================
   Exceptions

   Note: Python exceptions are in pymodule.i
   ====================================================================== */
#ifndef SWIGPYTHON
%include "../mserror.i"
#endif

/* =========================================================================
   Language-specific module code
   =======================================================================*/

/* Python */
#ifdef SWIGPYTHON
%include "pymodule.i"
#endif //SWIGPYTHON

/* Ruby */
#ifdef SWIGRUBY
%include "rbmodule.i"
#endif

/* Tcl */
#ifdef SWIGTCL8
%module Mapscript

%{
/* static global copy of Tcl interp */
static Tcl_Interp *SWIG_TCL_INTERP;
%}

%init %{
#ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
    return TCL_ERROR;
  }
  /* save Tcl interp pointer to be used in getImageToVar() */
  SWIG_TCL_INTERP = interp;
#endif //USE_TCL_STUBS
%}
#endif // SWIGTCL8

/* =========================================================================
   Wrap MapServer structs into mapscript classes
   =========================================================================*/

%include "../../map.h"
%include "../../mapprimitive.h"
%include "../../mapshape.h"
%include "../../mapproject.h"
%include "../../mapsymbol.h"

%apply Pointer NONNULL { mapObj *map };
%apply Pointer NONNULL { layerObj *layer };

/* =========================================================================
   Class extension methods are now included from separate interface files  
   ====================================================================== */
   
%include "../swiginc/error.i"
%include "../swiginc/map.i"
%include "../swiginc/mapzoom.i"
%include "../swiginc/symbol.i"
%include "../swiginc/symbolset.i"
%include "../swiginc/layer.i"
%include "../swiginc/class.i"
%include "../swiginc/style.i"
%include "../swiginc/rect.i"
%include "../swiginc/image.i"
%include "../swiginc/shape.i"
%include "../swiginc/point.i"
%include "../swiginc/line.i"
%include "../swiginc/shapefile.i"
%include "../swiginc/outputformat.i"
%include "../swiginc/projection.i"
%include "../swiginc/dbfinfo.i"
%include "../swiginc/labelcache.i"
%include "../swiginc/color.i"
%include "../swiginc/hashtable.i"
%include "../swiginc/resultcache.i"
%include "../swiginc/owsrequest.i"

/* =========================================================================
   Language-specific extensions to mapserver classes are included here 
   =======================================================================*/

/* Python */
#ifdef SWIGPYTHON
%include "pyextend.i"
#endif //SWIGPYTHON

/* Ruby */
#ifdef SWIGRUBY
%include "rbextend.i"
#endif

