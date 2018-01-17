
#pragma once

#ifdef _WIN32
#include <windows.h>
#endif
#include <vector>
#include <stdint.h>
#include "str_util.h"

class vstplugin;
struct Size {
	int32_t width;
	int32_t height;
};
inline bool operator!= (const Size& lhs, const Size& rhs)
{
	return lhs.width != rhs.width || lhs.height != rhs.height;
}

inline bool operator== (const Size& lhs, const Size& rhs)
{
	return lhs.width == rhs.width && lhs.height == rhs.height;
}
//------------------------------------------------------------------------
class vst_window
{
public:
	static vst_window* make(vstplugin* plugin, const String& name, Size size, bool resizeable);
	static vst_window* getVSTWindow(HWND handle);
	bool init (vstplugin* plugin, const String& name, Size size, bool resizeable);
	void* getHWND () const;

	void show ();
	void close ();
	void destroy ();
	void resize (Size newSize);
	Size getContentSize ();

	void updateDisplay();

	std::vector<vst_window*>& getWindows ();
	vstplugin* getPlugin() {
		return plugin;
	}
private:
	LRESULT CALLBACK proc (UINT message, WPARAM wParam, LPARAM lParam);
	void registerWindowClass (HINSTANCE instance);
	void* hwnd = NULL;
	vstplugin* plugin = NULL;
};
