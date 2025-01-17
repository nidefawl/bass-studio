#pragma once
#ifdef _WIN32
#ifndef GLFWwindow
struct GLFWwindow;
#endif
struct appwindow_size_t {
    char data[64]{};
    unsigned char type = 0;
    unsigned char valid = 0;
};
bool restoreWindowPos(GLFWwindow* glfw, appwindow_size_t* size);
bool saveWindowPos(GLFWwindow* glfw, appwindow_size_t* size);
#endif
