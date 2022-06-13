#pragma once

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
#include "types.h"
#include "math/vec.h"
#include "str_util.h"
#include "fileio.h"

class vstplugin;

//------------------------------------------------------------------------
struct GLFWwindow;
namespace vst_window_mgr {
    void destroyAllVSTWindows();
    bool isVstWindow(WINDOW_HANDLE);
}// namespace vst_window_mgr
class vst_window {
public:
    static vst_window* make(vstplugin* plugin, const String& name, ivec2 size, bool resizeable);
    static vst_window* getVSTWindow(WINDOW_HANDLE handle);
    bool init(vstplugin* plugin, const String& name, ivec2 size, bool resizeable);
    WINDOW_HANDLE getHWND() const;

    void show();
    void close();
    void destroy();
    void resize(ivec2 newSize) const;
    void onResize(ivec2 newSize);
    ivec2 getContentSize() const;
    void setPosition(ivec2 newPos);

    void updateWindow() const;
    void captureWindowFrame();

    static std::vector<vst_window*>& getWindows();
    vstplugin* getPlugin() {
        return plugin;
    }

private:
    vstplugin* plugin  = nullptr;

public:
    ImageBuf capturedFrame;
#ifdef _WIN32
    WINDOW_HANDLE hwnd = nullptr;
#endif
#if defined(__linux__) || defined(__APPLE__)
    WINDOW_HANDLE hwnd = 0;
    GLFWwindow* glfw   = NULL;
#endif
};
