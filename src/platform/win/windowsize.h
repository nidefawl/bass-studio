#pragma once
#ifdef _WIN32
#ifndef GLFWwindow
struct GLFWwindow;
#endif
#include <windows.h>
struct appwindow_size_t {
    bool valid;
    WINDOWPLACEMENT p{};
    appwindow_size_t() {
        p.length = sizeof(WINDOWPLACEMENT);
        valid    = false;
    }
};
bool restoreWindowPos(GLFWwindow* glfw, appwindow_size_t* size);
bool saveWindowPos(GLFWwindow* glfw, appwindow_size_t* size);
#endif
