#pragma once
#include <vector>
#include "str_util.h"

void enableGlDebugCallback();
bool checkGLError(const char* s);
int getStatus(int obj, int type);
String getLog(int logtype, int obj);
int compileShader(int type, String& src);
