
#pragma once

#ifdef _WIN32
#include <windows.h>
#define WINDOW_HANDLE HWND
#endif
#ifdef __linux__
//TODO: make this Window (XID) (requires include, requires namespaced Cursor or rename)
#if __x86_64__ || __ppc64__
#define WINDOW_HANDLE unsigned long
#else
#define WINDOW_HANDLE unsigned int
#endif
#endif
#include <vector>
#include <stdint.h>
#include "str_util.h"
#include <glm/glm.hpp>
#include <glm/vec2.hpp>

class vstplugin;

//------------------------------------------------------------------------
struct GLFWwindow;
namespace vst_window_mgr {
	void destroyAllVSTWindows();
	bool isVstWindow(HWND hwnd);
}
class vst_window
{
public:
	static vst_window* make(vstplugin* plugin, const String& name, glm::ivec2 size, bool resizeable);
	static vst_window* getVSTWindow(WINDOW_HANDLE handle);
	bool init (vstplugin* plugin, const String& name, glm::ivec2 size, bool resizeable);
	WINDOW_HANDLE getHWND () const;

	void show ();
	void close ();
	void destroy ();
	void resize (glm::ivec2 newSize);
	glm::ivec2 getContentSize ();

	void updateDisplay();

	std::vector<vst_window*>& getWindows ();
	vstplugin* getPlugin() {
		return plugin;
	}
private:
	vstplugin* plugin = NULL;
#ifdef _WIN32
	LRESULT CALLBACK proc (UINT message, WPARAM wParam, LPARAM lParam);
	WINDOW_HANDLE hwnd = NULL;
#endif
#ifdef __linux__
	WINDOW_HANDLE hwnd = 0;
	GLFWwindow* glfw = NULL;
#endif
};
