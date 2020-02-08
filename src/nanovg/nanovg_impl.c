#include "glheaders.h"
#ifdef NANOVG_GL2
 #define NANOVG_GL2_IMPLEMENTATION
#else
 #ifndef NANOVG_GL3
  #error "Please define one of NANOVG_GL2, NANOVG_GL3"
 #endif
#define NANOVG_GL3_IMPLEMENTATION
#endif
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
