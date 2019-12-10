#pragma once
#ifdef _WIN32
#include <Windows.h>
#endif

extern "C" {
HWND getMainHWND();
void setMainHWND(HWND hwnd);
}
