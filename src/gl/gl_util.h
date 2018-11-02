#pragma once
#include <vector>
#include "str_util.h"

void enableGlDebugCallback();
bool checkGLError(const char* s);
int getStatus(int obj, int type);
String getLog(int logtype, int obj);
int compileShader(int type, String& src);
bool isGLContextPresent();
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
