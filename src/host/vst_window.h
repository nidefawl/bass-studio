
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
//TODO: make this Window (XID) (requires include, requires namespaced Cursor or rename)
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
#include <stdint.h>
#include "math/vec.h"
#include "str_util.h"
#include "fileio.h"

class vstplugin;

//------------------------------------------------------------------------
struct GLFWwindow;
namespace vst_window_mgr {
	void destroyAllVSTWindows();
	bool isVstWindow(WINDOW_HANDLE);
}
class vst_window
{
public:
	static vst_window* make(vstplugin* plugin, const String& name, ivec2 size, bool resizeable);
	static vst_window* getVSTWindow(WINDOW_HANDLE handle);
	bool init (vstplugin* plugin, const String& name, ivec2 size, bool resizeable);
	WINDOW_HANDLE getHWND () const;

	void show ();
	void close ();
	void destroy ();
	void resize (ivec2 newSize);
	ivec2 getContentSize ();

	void updateWindow();
	void captureWindowFrame();

	std::vector<vst_window*>& getWindows ();
	vstplugin* getPlugin() {
		return plugin;
	}
private:
	vstplugin* plugin = NULL;
	bool isChildWindow = false;
public:
	ImageBuf capturedFrame;
	int64_t captureTime = 0;
#ifdef _WIN32
	LRESULT CALLBACK proc (UINT message, WPARAM wParam, LPARAM lParam);
	WINDOW_HANDLE hwnd = NULL;
#endif
#if defined(__linux__) || defined(__APPLE__)
	WINDOW_HANDLE hwnd = 0;
	GLFWwindow* glfw = NULL;
#endif
};
