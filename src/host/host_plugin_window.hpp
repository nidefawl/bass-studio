#pragma once

#include "logging.hpp"
#include "host/plugin/modules.hpp"
#ifdef _WIN32
#include <windows.h>
#define WINDOW_HANDLE HWND
#endif

#ifdef __APPLE__
#if __x86_64__ || __ppc64__
#define WINDOW_HANDLE unsigned long
#else
#define WINDOW_HANDLE unsigned int
#endif
#endif

#ifdef __linux__
//TODO: make this Window (XID) (requires include, requires namespaced DAW::Cursor or rename)
#if __x86_64__ || __ppc64__
#define WINDOW_HANDLE unsigned long
#else
#define WINDOW_HANDLE unsigned int
#endif
#endif
#ifndef WINDOW_HANDLE
#error "Platform not supported"
#endif
#include <vector>
#include "types.hpp"
#include "math/vec.hpp"
#include "str_util.hpp"
#include "fileio.hpp"

class effectbase;

//------------------------------------------------------------------------
struct GLFWwindow;
namespace host_plugin_window_mgr {
    void destroyAllPluginWindows();
    bool isPluginWindow(WINDOW_HANDLE);
}// namespace vst_window_mgr
class host_plugin_window {
    bool bRedirectKeysToDawMainWindow = false;
public:
    static host_plugin_window* make(effectbase* plugin, const String& name, ivec2 size, bool resizeable);
    static host_plugin_window* getWindowInstance(WINDOW_HANDLE handle);
    bool init(effectbase* plugin, const String& name, ivec2 size, bool resizeable);
    WINDOW_HANDLE getHWND() const;
    WINDOW_HANDLE getWindowHandle() const;

    void show(ivec4 posSize, bool bSetPos, bool bSetSize);
    void close();
    void destroy();
    void resize(ivec2 newSize) const;
    void onResize(ivec2 newSize);
    ivec2 getContentSize() const;
    void setPosition(ivec2 newPos);
    void storePosition();

    void updateFromMainThread() const;
#if defined(__linux__)
    GLFWwindow* getGlfwWindow() const { return glfw; }
#endif
    void captureWindowFrame();
    bool isRedirectingKeysToDawMainWindow() const {
        return bRedirectKeysToDawMainWindow;
    }
    void setRedirectKeysToDawMainWindow(bool b) {
        bRedirectKeysToDawMainWindow = b;
    }

    static std::vector<host_plugin_window*>& getWindows();
    effectbase* getPlugin() {
        return plugin;
    }

private:
    effectbase* plugin  = nullptr;

public:
    ImageBuf capturedFrame;
#ifdef _WIN32
    WINDOW_HANDLE hwnd = nullptr;
#endif
#if defined(__linux__) || defined(__APPLE__)
    GLFWwindow* glfw = nullptr;
    WINDOW_HANDLE hwnd = 0;
#endif
#if defined(__APPLE__)
    WINDOW_HANDLE cocoaView = 0;
#endif
};
