
#include "vst_window.h"
#include "vst_host.h"
#include "vst_plugin.h"
#include "mainctrl.h"

#include <tchar.h>
#include <Windows.h>
#include "leak_detect.h"

#define WIN32API_CALLBACK_TYPE __stdcall
HWND getMainHWND(); // window.cpp
LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam); // window.cpp

namespace {
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
	static vst_window* getWindowByHWND (HWND hwnd)
	{
		auto it = std::find_if(vst_window_list.begin (), vst_window_list.end (), [hwnd](vst_window* window) {
			return window->getHWND() == hwnd;
		});
		if (it != vst_window_list.end ())
			return *it;
		return nullptr;
	}
}

static const TCHAR* gWindowClassName = _T("VSTHOSTWINDOW");

vst_window* vst_window::make (vstplugin* plugin, const String& name, Size size, bool resizeable, HINSTANCE instance)
{
	vst_window* window = new vst_window();
	if (window->init (plugin, name, size, resizeable, instance))
		return window;
	return nullptr;
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

static LRESULT CALLBACK PluginWndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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

				Size newClientSize = {(clientSize.right - clientSize.left),
				                      (clientSize.bottom - clientSize.top)};
				newClientSize.width += diffX;
				newClientSize.height += diffY;

				Size constraintSize = plugin->constrainSize (window, newClientSize);
				if (constraintSize != newClientSize)
				{
					auto diffX = (oldSize.right - oldSize.left) - (clientSize.right - clientSize.left);
					auto diffY = (oldSize.bottom - oldSize.top) - (clientSize.bottom - clientSize.top);
					newSize->right = newSize->left + static_cast<LONG> (constraintSize.width + diffX);
					newSize->bottom = newSize->top + static_cast<LONG> (constraintSize.height + diffY);
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

//------------------------------------------------------------------------
void vst_window::registerWindowClass (HINSTANCE instance)
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

//------------------------------------------------------------------------
std::vector<vst_window*>& vst_window::getWindows ()
{
	return vst_window_list;
}

//------------------------------------------------------------------------
bool vst_window::init(vstplugin* plugin, const String& name, Size size, bool resizeable, HINSTANCE instance)
{
	this->plugin = plugin;
	registerWindowClass (instance);
	DWORD exStyle = WS_EX_APPWINDOW;
	DWORD dwStyle = WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS;
	if (resizeable)
		dwStyle |= WS_SIZEBOX | WS_MAXIMIZEBOX;
	HWND parent = getMainHWND();
	hwnd = CreateWindowEx (exStyle, gWindowClassName, StringAsCStr(name), dwStyle,
            0, 0, size.width, size.height, nullptr, nullptr, instance, nullptr);
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
	plugin->onShow(this);
}

//------------------------------------------------------------------------
Size vst_window::getContentSize ()
{
	RECT r;
	GetClientRect (hwnd, &r);
	return {r.right - r.left, r.bottom - r.top};
}



void vst_window::updateDisplay() {
//	InvalidateRect(hwnd, NULL, TRUE);
	InvalidateRgn(hwnd, NULL, TRUE);
//    RedrawWindow( hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN );
//	UpdateWindow(hwnd);
//	my_printf("updateDisplay %d\n", getTimeMillis());
}
//------------------------------------------------------------------------
void vst_window::resize (Size newSize)
{
	if (getContentSize () == newSize)
		return;
	WINDOWINFO windowInfo;
	GetWindowInfo (hwnd, &windowInfo);
	RECT clientRect {};
	clientRect.right = newSize.width;
	clientRect.bottom = newSize.height;
	AdjustWindowRectEx (&clientRect, windowInfo.dwStyle, false, windowInfo.dwExStyle);
	SetWindowPos (hwnd, HWND_TOP, 0, 0, clientRect.right - clientRect.left,
	              clientRect.bottom - clientRect.top, SWP_NOMOVE | SWP_NOCOPYBITS | SWP_NOACTIVATE);
}

//------------------------------------------------------------------------
HWND vst_window::getHWND () const
{
	return hwnd;
}
