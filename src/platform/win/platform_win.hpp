#pragma once
#ifdef _WIN32
#include <windows.h>

extern "C" {
HWND getMainHWND();
void setMainHWND(HWND hwnd);
}
bool isRunningOnWine();
#endif