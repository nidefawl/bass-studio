#pragma once
#ifdef _WIN32
#ifndef GLFWwindow
struct GLFWwindow;
#endif
#include <windows.h>
struct windowsize {
    bool valid;
    WINDOWPLACEMENT p{};
    windowsize() {
        p.length = sizeof(WINDOWPLACEMENT);
        valid    = false;
    }
};
bool restoreWindowPos(GLFWwindow* glfw, windowsize* size);
bool saveWindowPos(GLFWwindow* glfw, windowsize* size);
#endif
