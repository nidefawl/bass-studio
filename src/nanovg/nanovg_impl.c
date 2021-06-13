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
#ifdef _WIN32
#include <windows.h>

//TODO: make a C wrapper function to call platform.h getTimeMillisd()
// (right now we cannot call c++ function from c source)
// or switch compiler for nanovg.c to c++
static float startOffset;
void resetShaderTimeOffset() {
	startOffset = (float)timeGetTime();
}
float glnvg__getTimeMillisf()
{
	DWORD dwTime = timeGetTime();
	return (float) dwTime-startOffset;
}
#else
#error TODO IMPLEMENT glnvg__getTimeMillisf
#endif
