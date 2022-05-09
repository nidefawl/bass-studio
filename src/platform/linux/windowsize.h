#pragma once
#ifdef __linux__
#ifndef GLFWwindow
struct GLFWwindow;
#endif
struct windowsize {
    bool valid = false;
    int hmax   = 0;
    int vmax   = 0;
    int x = 0, y = 0;
    int w = 0, h = 0;
};
bool restoreWindowPos(GLFWwindow* glfw, windowsize* placement);
bool saveWindowPos(GLFWwindow* glfw, windowsize* placement);
#endif
