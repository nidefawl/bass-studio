#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "host/host_plugin_window.hpp"

constinit size_t SIZEOF_WINDOWPLACEMENT = sizeof(WINDOWPLACEMENT);
bool saveWindowPos(GLFWwindow* glfw, appwindow_size_t* size) {
    HWND hwnd = glfwGetWin32Window(glfw);
    WINDOWPLACEMENT p{};
    p.length = sizeof(WINDOWPLACEMENT);
    size->type = 1; // WIN32
    size->valid = GetWindowPlacement(hwnd, &p) != 0;
    size_t sizeData = sizeof(size->data);
    if (size->valid && SIZEOF_WINDOWPLACEMENT <= sizeData) {
        memcpy(&size->data[0], &p, sizeof(WINDOWPLACEMENT));
    }
    return true;
}

bool saveHostWindowPos(host_plugin_window* hostWindow, appwindow_size_t* size) {
    HWND hwnd = hostWindow->getHWND();
    WINDOWPLACEMENT p{};
    p.length = sizeof(WINDOWPLACEMENT);
    size->type = 1; // WIN32
    size->valid = GetWindowPlacement(hwnd, &p) != 0;
    if (size->valid && sizeof(WINDOWPLACEMENT) <= sizeof(size->data)) {
        memcpy(&size->data[0], &p, sizeof(WINDOWPLACEMENT));
    }
    return true;
}

bool restoreHostWindowPos(host_plugin_window* hostWindow, appwindow_size_t* size) {
    HWND hwnd = hostWindow->getHWND();
    size_t sizeData = sizeof(size->data);
    if (size->valid && size->type == 1 && SIZEOF_WINDOWPLACEMENT <= sizeData) {
        WINDOWPLACEMENT p{};
        memcpy(&p, &size->data[0], sizeof(WINDOWPLACEMENT));
        p.length = sizeof(WINDOWPLACEMENT);
        return SetWindowPlacement(hwnd, &p) != 0;
    }
    return false;
}

bool restoreWindowPos(GLFWwindow* glfw, appwindow_size_t* size) {
    size_t sizeData = sizeof(size->data);
    if (size->valid && size->type == 1 && SIZEOF_WINDOWPLACEMENT <= sizeData) {
        HWND hwnd = glfwGetWin32Window(glfw);
        WINDOWPLACEMENT p{};
        memcpy(&p, &size->data[0], sizeof(WINDOWPLACEMENT));
        p.length = sizeof(WINDOWPLACEMENT);
        return SetWindowPlacement(hwnd, &p) != 0;
    }
    return false;
}
#endif
