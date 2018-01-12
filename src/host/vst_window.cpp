
#include "vst_window.h"
#include "vst_host.h"
#include "vst_plugin.h"
#include <tchar.h>
#include <Windows.h>

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
}

static const TCHAR* gWindowClassName = _T("VSTHOSTWINDOW");

vst_window* vst_window::make (vstplugin* plugin, const String& name, Size size, bool resizeable, HINSTANCE instance)
{
	vst_window* window = new vst_window();
	if (window->init (plugin, name, size, resizeable, instance))
		return window;
	return nullptr;
}

LRESULT CALLBACK vst_window::WndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	vst_window* window = reinterpret_cast<vst_window*> ((LONG_PTR)GetWindowLongPtr (hWnd, GWLP_USERDATA));
	if (window)
		return window->proc (message, wParam, lParam);
	return DefWindowProc (hWnd, message, wParam, lParam);
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
	wcex.lpfnWndProc = WndProc;
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
	DWORD exStyle = WS_EX_TOOLWINDOW;
	DWORD dwStyle = WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS;
	if (resizeable)
		dwStyle |= WS_SIZEBOX | WS_MAXIMIZEBOX;
	hwnd = CreateWindowEx (exStyle, gWindowClassName, StringAsCStr(name), dwStyle,
            0, 0, size.width, size.height, nullptr, nullptr, instance, nullptr);
//	CreateWindowExA(DWORD dwExStyle,LPCSTR lpClassName,LPCSTR lpWindowName,DWORD dwStyle,int X,int Y,int nWidth,int nHeight,HWND hWndParent,HMENU hMenu,HINSTANCE hInstance,LPVOID lpParam);
	if (hwnd)
	{
		SetWindowLongPtr (hwnd, GWLP_USERDATA, (__int3264) (LONG_PTR)this);
		addWindow (this);
	}
	return hwnd != nullptr;
}

//------------------------------------------------------------------------
LRESULT vst_window::proc (UINT message, WPARAM wParam, LPARAM lParam)
{


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
			plugin->onResize(this, getContentSize ());
			break;
		}
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

			Size constraintSize = plugin->constrainSize (this, newClientSize);
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
			close();
			return 1;
		}
	}
	return DefWindowProc (hwnd, message, wParam, lParam);
}

//------------------------------------------------------------------------
void vst_window::close()
{
	plugin->onClose();
	SetWindowLongPtr (hwnd, GWLP_USERDATA, (__int3264) (LONG_PTR) nullptr);
	ShowWindow(hwnd, false);
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
