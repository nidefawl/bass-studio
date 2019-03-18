#ifdef _WIN32
#include "../../vst_window.h"
#include "../../vst_host.h"
#include "../../plugin/vst_plugin.h"
#include "../host/mainctrl.h"
#ifdef _WIN32
#include <windows.h>
#include <winuser.h>
#define WINDOW_HANDLE HWND
#endif

#include <tchar.h>
#include <Windows.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
using glm::ivec2;
#include "leak_detect.h"

#define WIN32API_CALLBACK_TYPE __stdcall
HWND getMainHWND(); // window.cpp
LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam); // window.cpp

namespace {
	const TCHAR* gWindowClassName = _T("VSTHOSTWINDOW");
	static std::vector<vst_window*> vst_window_list;
	static void addWindow (vst_window* window)
	{
		vst_window_list.push_back (window);
	}
	static void removeWindow (vst_window* window)
	{
		auto it = std::find (vst_window_list.begin (), vst_window_list.end (), window);
		if (it != vst_window_list.end ())
			vst_window_list.erase(it);
	}
	static vst_window* getWindowByHWND (WINDOW_HANDLE hwnd)
	{
		auto it = std::find_if(vst_window_list.begin (), vst_window_list.end (), [hwnd](vst_window* window) {
			return window->getHWND() == hwnd;
		});
		if (it != vst_window_list.end ())
			return *it;
		return nullptr;
	}

	LRESULT CALLBACK PluginWndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		vst_window* window = reinterpret_cast<vst_window*> ((LONG_PTR)GetWindowLongPtr (hwnd, GWLP_USERDATA));
		if (window) {
			vstplugin* plugin = window->getPlugin();
			switch (message)
			{
				case WM_ERASEBKGND:
				{
					return 1; // don't draw background
				}
				case WM_PAINT:
				{
					PAINTSTRUCT ps{};
					BeginPaint(hwnd, &ps);
					EndPaint(hwnd, &ps);
					return 0;
				}
				case WM_SIZE:
				{
					plugin->onResize(window, window->getContentSize ());
					break;
				}
		        case WM_KEYDOWN:
		        case WM_SYSKEYDOWN:
		        case WM_KEYUP:
		        case WM_SYSKEYUP:
		        	my_printf("key \n", 0);
					{
						HWND hwndMain = getMainHWND();
						if (hwndMain) {
							return appWndProc(hwndMain, message, wParam, lParam);
						}
					}
		        	break;
				case WM_SIZING:
				{
					RECT* newSize = reinterpret_cast<RECT*> (lParam);
					RECT oldSize;
					GetWindowRect (hwnd, &oldSize);
					RECT clientSize;
					GetClientRect (hwnd, &clientSize);

					auto diffX = (newSize->right - newSize->left) - (oldSize.right - oldSize.left);
					auto diffY = (newSize->bottom - newSize->top) - (oldSize.bottom - oldSize.top);

					ivec2 newClientSize = {(clientSize.right - clientSize.left),
					                      (clientSize.bottom - clientSize.top)};
					newClientSize.x += diffX;
					newClientSize.y += diffY;

					ivec2 constraintSize = plugin->constrainSize (window, newClientSize);
					if (constraintSize != newClientSize)
					{
						auto diffX = (oldSize.right - oldSize.left) - (clientSize.right - clientSize.left);
						auto diffY = (oldSize.bottom - oldSize.top) - (clientSize.bottom - clientSize.top);
						newSize->right = newSize->left + static_cast<LONG> (constraintSize.x + diffX);
						newSize->bottom = newSize->top + static_cast<LONG> (constraintSize.y + diffY);
					}
					return TRUE;
				}
				case WM_CLOSE:
				{
					window->close();
					return 1;
				}
			}
		}
		return DefWindowProc (hwnd, message, wParam, lParam);
	}
	void registerWindowClass (HINSTANCE instance)
	{
		static bool once = true;
		if (!once)
			return;
		once = true;

		WNDCLASSEX wcex {};

		wcex.cbSize = sizeof (WNDCLASSEX);

		wcex.style = CS_DBLCLKS;
		wcex.lpfnWndProc = PluginWndProc;
		wcex.hInstance = instance;
		wcex.hCursor = LoadCursor (instance, IDC_ARROW);
		wcex.hbrBackground = nullptr;
		wcex.lpszClassName = gWindowClassName;

		RegisterClassEx (&wcex);
	}
}


vst_window* vst_window::make (vstplugin* plugin, const String& name, ivec2 size, bool resizeable)
{
	vst_window* window = new vst_window();
	if (window->init (plugin, name, size, resizeable))
		return window;
	return nullptr;
}
namespace vst_window_mgr {
	void destroyAllVSTWindows() {
		for (vst_window* w : vst_window_list) {
			HWND hwnd = w->getHWND();
			if (hwnd) {
				ShowWindow(hwnd, false);
				DestroyWindow(hwnd);
			}
		}
		vst_window_list.clear();
		UnregisterClass(gWindowClassName, GetModuleHandle(NULL));
	}
	bool isVstWindow(HWND hwnd) {
		return nullptr != vst_window::getVSTWindow(hwnd);
	}
}
vst_window* vst_window::getVSTWindow(HWND handle)
{
	assert(handle);
	TCHAR clsName_v[512];
	String sChain = "";
	vst_window* vstwinhandle = nullptr;
	while (handle /*&& !vstwinhandle*/) {
		if (GetClassName(handle, clsName_v, 512)) {
			if (!vstwinhandle && strcmp(clsName_v, "VSTHOSTWINDOW") == 0) {
				vstwinhandle = getWindowByHWND(handle);
			}
			if (!strcmp(clsName_v, "Edit")) {
				vstwinhandle = nullptr;
				break;
			}
			if (!sChain.length()) {
				sChain = String(clsName_v);
			} else {
				sChain = String(clsName_v) + " > " + sChain;
			}
		} else {
			my_printf("GetClassName failed\n",0);
		}
		WINDOWINFO info;
		info.cbSize = sizeof(info);
		GetWindowInfo(handle, &info);
		if (info.dwStyle & WS_CHILD) {
			handle = GetParent(handle);
		} else break;
	}
//	my_printf("getVSTWindow %s isPlugin %d\n", StringAsCStr(sChain), vstwinhandle ? 1 : 0);
	return vstwinhandle;
}



//------------------------------------------------------------------------
std::vector<vst_window*>& vst_window::getWindows ()
{
	return vst_window_list;
}

//------------------------------------------------------------------------
bool vst_window::init(vstplugin* plugin, const String& name, ivec2 size, bool resizeable)
{
	this->plugin = plugin;
	HINSTANCE instance = GetModuleHandle(NULL);
	registerWindowClass (instance);
	DWORD exStyle = WS_EX_APPWINDOW;
	DWORD dwStyle = WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS;
	if (resizeable)
		dwStyle |= WS_SIZEBOX | WS_MAXIMIZEBOX;
	HWND parent = getMainHWND();
	hwnd = CreateWindowEx (exStyle, gWindowClassName, StringAsCStr(name), dwStyle,
            0, 0, size.x, size.y, nullptr, nullptr, instance, nullptr);
	if (parent) {

		RECT rcOwner;
		RECT rcDlg;
		RECT rc;
		GetWindowRect(parent, &rcOwner);
		GetWindowRect(hwnd, &rcDlg);
		CopyRect(&rc, &rcOwner);
		OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
		OffsetRect(&rc, -rc.left, -rc.top);
		OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);
		SetWindowPos(hwnd,
			HWND_TOP,
			rcOwner.left + (rc.right / 2),
			rcOwner.top + (rc.bottom / 2),
			0, 0,          // Ignores size arguments.
			SWP_NOSIZE);
	}
//	CreateWindowExA(DWORD dwExStyle,LPCSTR lpClassName,LPCSTR lpWindowName,DWORD dwStyle,int X,int Y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam);
	if (hwnd)
	{
		SetWindowLongPtr (hwnd, GWLP_USERDATA, (__int3264) (LONG_PTR)this);
		addWindow (this);
	}
	return hwnd != nullptr;
}

//------------------------------------------------------------------------
void vst_window::close()
{
	plugin->onClose();
	ShowWindow(hwnd, false);
}

//------------------------------------------------------------------------
void vst_window::destroy()
{
	plugin->onWindowDestroy();
	SetWindowLongPtr (hwnd, GWLP_USERDATA, (__int3264) (LONG_PTR) nullptr);
	DestroyWindow(hwnd);
	removeWindow (this);
}
//------------------------------------------------------------------------
void vst_window::show()
{

	SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOCOPYBITS | SWP_SHOWWINDOW);
	SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (__int3264) (LONG_PTR)getMainHWND());

	plugin->onShow(this);
}

//------------------------------------------------------------------------
ivec2 vst_window::getContentSize ()
{
	RECT r;
	GetClientRect (hwnd, &r);
	return {r.right - r.left, r.bottom - r.top};
}



void vst_window::updateWindow() {
//	InvalidateRect(hwnd, NULL, TRUE);
	InvalidateRgn(hwnd, NULL, TRUE);
//    RedrawWindow( hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN );
//	UpdateWindow(hwnd);
//	my_printf("updateDisplay %d\n", getTimeMillis());
}
//------------------------------------------------------------------------
void vst_window::resize (ivec2 newSize)
{
	if (getContentSize () == newSize)
		return;
	WINDOWINFO windowInfo;
	GetWindowInfo (hwnd, &windowInfo);
	RECT clientRect {};
	clientRect.right = newSize.x;
	clientRect.bottom = newSize.y;
	AdjustWindowRectEx (&clientRect, windowInfo.dwStyle, false, windowInfo.dwExStyle);
	SetWindowPos (hwnd, HWND_TOP, 0, 0, clientRect.right - clientRect.left,
	              clientRect.bottom - clientRect.top, SWP_NOMOVE | SWP_NOCOPYBITS | SWP_NOACTIVATE);
}

//------------------------------------------------------------------------
WINDOW_HANDLE vst_window::getHWND () const
{
	return hwnd;
}
#endif
