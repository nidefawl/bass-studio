#pragma once
#if defined(__linux__) || defined(__APPLE__)
#ifndef GLFWwindow
struct GLFWwindow;
#endif

struct appwindow_size_linux_t {
    bool valid = false;
    int hmax   = 0;
    int vmax   = 0;
    int x = 0, y = 0;
    int w = 0, h = 0;
};
struct appwindow_size_t {
    char data[64]{};
    unsigned char type = 0;
    unsigned char valid = 0;
};
bool restoreWindowPos(GLFWwindow* glfw, appwindow_size_t* placement);
bool saveWindowPos(GLFWwindow* glfw, appwindow_size_t* placement);
#endif
