#pragma once
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
HWND getMainHWND();
void setMainHWND(HWND hwnd);
}
