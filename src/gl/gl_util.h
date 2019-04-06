#pragma once
#include <vector>
#include "math/seq_math.h"
#include "str_util.h"

void enableGlDebugCallback();
bool checkGLError(const char* s);
int getStatus(int obj, int type);
String getLog(int logtype, int obj);
int compileShader(int type, const String& src);
bool isGLContextPresent();
inline void gldPerspective(double* mat4x4, double fovy, double aspect, double zNear, double zFar)
{
	const double f = 1.0 / tan(fovy * M_PI / 360);
    const double xform[16] =
    {
        f / aspect, 0, 0, 0,
        0,          f, 0, 0,
        0,          0, (zFar + zNear)/(zNear - zFar), -1,
        0,          0, 2*zFar*zNear/(zNear - zFar), 0
    };
    memcpy(mat4x4, xform, (4*4)*sizeof(double));
}
inline void glfOrtho(double* mat4x4, float left, float right, float bottom, float top, float fnear, float ffar)
{
	const float a = 2.0f / (right - left);
    const float b = 2.0f / (top - bottom);
    const float c = -2.0f / (ffar - fnear);

    const float tx = - (right + left)/(right - left);
    const float ty = - (top + bottom)/(top - bottom);
    const float tz = - (ffar + fnear)/(ffar - fnear);

    const double ortho[16] = {
        a, 0, 0, 0,
        0, b, 0, 0,
        0, 0, c, 0,
        tx, ty, tz, 1
    };
    memcpy(mat4x4, ortho, (4*4)*sizeof(double));
}
inline void downsampledResolution(int w, int h, int fac, int& wd, int& hd) {
	if (fac != 1) {
		int ssrW = w/fac;
		int ssrH = h/fac;
		if (ssrW%2!=0)
			ssrW++;
		if (ssrH%2!=0)
			ssrH++;
		if (ssrW<1)ssrW=1;
		if (ssrH<1)ssrH=1;
		wd = ssrW;
		hd = ssrH;;
	} else {
		wd = w;
		hd = h;
	}
}
