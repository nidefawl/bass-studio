#pragma once
#ifdef _WIN32
#ifndef GLFWwindow
struct GLFWwindow;
#endif
struct appwindow_size_t {
    char data[64]{};
    bool valid = false;
};
bool restoreWindowPos(GLFWwindow* glfw, appwindow_size_t* size);
bool saveWindowPos(GLFWwindow* glfw, appwindow_size_t* size);
#endif
