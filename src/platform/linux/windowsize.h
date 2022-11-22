#pragma once
#ifdef __linux__
#ifndef GLFWwindow
struct GLFWwindow;
#endif

struct appwindow_size_t {
    bool valid = false;
    int hmax   = 0;
    int vmax   = 0;
    int x = 0, y = 0;
    int w = 0, h = 0;
};
bool restoreWindowPos(GLFWwindow* glfw, appwindow_size_t* placement);
bool saveWindowPos(GLFWwindow* glfw, appwindow_size_t* placement);
#endif
