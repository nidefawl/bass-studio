#include "modules.h"
#ifdef _WIN32
#include "logging.h"
#include "str_util.h"
#include "host/host_plugin_window.h"
#include "host/pluginmanager.h"
#include "host/plugin/vst_plugin.h"
#include "host/mainctrl.h"
#include <windows.h>
#include <winuser.h>
#include <tchar.h>

#include "math/vec.h"
#include "math/seq_math.h"
#include "platform/win/platform_win.h"

#define WINDOW_HANDLE HWND
#define WIN32API_CALLBACK_TYPE __stdcall

LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);// window.cpp

namespace {
    const TCHAR* gWindowClassName = _T("_DAW_PLUG_WIN");
    std::vector<host_plugin_window*> host_plugin_window_list;
    void addWindow(host_plugin_window* window) {
        host_plugin_window_list.push_back(window);
    }
    void removeWindow(host_plugin_window* window) {
        auto it = std::find(host_plugin_window_list.begin(), host_plugin_window_list.end(), window);
        if (it != host_plugin_window_list.end())
            host_plugin_window_list.erase(it);
    }
    host_plugin_window* getWindowByHWND(WINDOW_HANDLE hwnd) {
        auto it = std::find_if(host_plugin_window_list.begin(), host_plugin_window_list.end(), [hwnd](host_plugin_window* window) {
            return window->getHWND() == hwnd;
        });
        if (it != host_plugin_window_list.end())
            return *it;
        return nullptr;
    }

    void captureWindow(HWND hwnd, ImageBuf& capturedFrame) {
        if (!IsWindowVisible(hwnd)) {
            capturedFrame.w = 0;
            capturedFrame.h = 0;
            capturedFrame.bytes.resize(0);
            capturedFrame.bitdepth = 4;
            return;
        }
        static hires_timer_t time2;
        time2.reset();
        HDC hdc = GetDCEx(hwnd, nullptr, DCX_CACHE);
        if (hdc) {
            static hires_timer_t time;
            RECT rectClientArea;
            GetClientRect(hwnd, &rectClientArea);

            const int width         = rectClientArea.right - rectClientArea.left;
            const int height        = rectClientArea.bottom - rectClientArea.top;
            const int bytesPerPixel = 4;
            const int bufSizeBitmap = width * height * bytesPerPixel;
            BITMAPINFO info{};
            info.bmiHeader.biSize        = sizeof(info.bmiHeader);
            info.bmiHeader.biWidth       = width;
            info.bmiHeader.biHeight      = -height;// negative for top-down pixel array
            info.bmiHeader.biPlanes      = 1;
            info.bmiHeader.biBitCount    = bytesPerPixel * 8;
            info.bmiHeader.biCompression = BI_RGB;
            info.bmiHeader.biSizeImage   = bufSizeBitmap;
            void* pVoidBitmap;
            HBITMAP bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, &pVoidBitmap, nullptr, 0);
            if (!bitmap) {
                log_printf("Failed creating bitmap: %lu\n", GetLastError());
            }

            HDC memDC       = CreateCompatibleDC(hdc);
            HBITMAP prevOBj = (HBITMAP) SelectObject(memDC, bitmap);
            //if (!PrintWindow(hwnd, memDC, PW_CLIENTONLY)) {
            //    log_printf("PrintWindow failed: %d\n", GetLastError());
            //}
            time.reset();
            if (!BitBlt(memDC, 0, 0, width, height, hdc, 0, 0, SRCCOPY)) {
                log_printf("BitBlt failed: %lu\n", GetLastError());
            }
            static int calls    = 0;
            static double total = 0.0;
            total += time.getTimeDouble();
            if (++calls % 60 == 0) {
                double perCall = total / calls;
                log_lf(Log::L_DEBUG, "bitblt %f\n", perCall);
                total = 0;
                calls = 0;
            }
            SelectObject(memDC, prevOBj);
            DeleteDC(memDC);
            ReleaseDC(hwnd, hdc);

            int64_t bufSize        = width * height * 4;
            capturedFrame.w        = width;
            capturedFrame.h        = height;
            capturedFrame.bitdepth = 4;
            capturedFrame.bytes.resize(bufSize);
            uint8_t* pBitmapData = reinterpret_cast<uint8_t*>(pVoidBitmap);
            uint8_t* pOut        = capturedFrame.bytes.data();

            for (int idx = 0; idx < bufSizeBitmap; idx += bytesPerPixel) {
                *pOut++ = pBitmapData[idx + 2];
                *pOut++ = pBitmapData[idx + 1];
                *pOut++ = pBitmapData[idx + 0];
                *pOut++ = 0xFF;
            }
            DeleteObject(bitmap);
        }
        static int calls2    = 0;
        static double total2 = 0.0;
        total2 += time2.getTimeDouble();
        if (++calls2 % 60 == 0) {
            double perCall = total2 / calls2;
            log_lf(Log::L_DEBUG, "total %f\n", perCall);
            total2 = 0;
            calls2 = 0;
        }
    }
    LRESULT CALLBACK PluginWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        host_plugin_window* window = reinterpret_cast<host_plugin_window*>((LONG_PTR) GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (window) {
            auto* plugin = window->getPlugin();
            switch (message) {
                case WM_MOUSEACTIVATE:
                case WM_SETFOCUS: {
                    /**
                     * Alwasy focus first child window:
                     * Assuming only one child window exists inside this host window
                     */
                    HWND top_child = GetWindow(hwnd, GW_CHILD);
                    if (top_child) {
                        SetFocus(top_child);
                        return 0;
                    }
                    break;
                }
                case WM_ERASEBKGND: {
                    return 1;// don't draw background
                }
                case WM_SIZE: {
                    UINT width = LOWORD(lParam);
                    UINT height = HIWORD(lParam);
                    window->onResize(ivec2(width, height));
                    return 0;
                }
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                case WM_KEYUP:
                case WM_SYSKEYUP:
                    /**
                     * Note that this host window will not have focus and
                     * keyboard input is directly processed by the child window.
                     * This means this is most likely dead code, but we keep it as fallback
                     */
                    if (window->isRedirectingKeysToDawMainWindow())
                    {
                        HWND hwndMain = getMainHWND();
                        if (hwndMain) {
                            return appWndProc(hwndMain, message, wParam, lParam);
                        }
                    }
                    break;
                case WM_SIZING: {
                    RECT* newSize = reinterpret_cast<RECT*>(lParam);
                    RECT oldSize;
                    GetWindowRect(hwnd, &oldSize);
                    RECT clientSize;
                    GetClientRect(hwnd, &clientSize);
                    auto diffX = (newSize->right - newSize->left) - (oldSize.right - oldSize.left);
                    auto diffY = (newSize->bottom - newSize->top) - (oldSize.bottom - oldSize.top);

                    ivec2 newClientSize = { (clientSize.right - clientSize.left),
                                            (clientSize.bottom - clientSize.top) };
                    newClientSize.x += diffX;
                    newClientSize.y += diffY;

                    ivec2 constraintSize = plugin->constrainWindowSize(window, newClientSize);
                    if (constraintSize != newClientSize) {
                        diffX = (oldSize.right - oldSize.left) - (clientSize.right - clientSize.left);
                        diffY = (oldSize.bottom - oldSize.top) - (clientSize.bottom - clientSize.top);
                        newSize->right  = newSize->left + static_cast<LONG>(constraintSize.x + diffX);
                        newSize->bottom = newSize->top + static_cast<LONG>(constraintSize.y + diffY);
                    }
                    return TRUE;
                }
                case WM_CLOSE: {
                    window->close();
                    return 0;
                }
                default:
                    break;
            }
        }
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    void registerWindowClass(HINSTANCE instance) {
        static bool once = true;
        if (!once)
            return;
        once = true;

        WNDCLASSEX wcex{};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style         = /* CS_HREDRAW | CS_VREDRAW | */ CS_OWNDC | CS_DBLCLKS;
        wcex.lpfnWndProc   = PluginWndProc;
        wcex.hInstance     = instance;
        wcex.hCursor       = LoadCursor(instance, IDC_ARROW);
        wcex.hbrBackground = nullptr;
        wcex.lpszClassName = gWindowClassName;
        RegisterClassEx(&wcex);
    }
}// namespace

host_plugin_window* host_plugin_window::make(effectbase* plugin, const String& name, ivec2 size, bool resizeable) {
    auto* window = new host_plugin_window();
    if (window->init(plugin, name, size, resizeable))
        return window;
    return nullptr;
}

namespace getWindowInstance {
    void destroyAllPluginWindows() {
        for (host_plugin_window* w : host_plugin_window_list) {
            HWND hwnd = w->getHWND();
            if (hwnd) {
                ShowWindow(hwnd, false);
                DestroyWindow(hwnd);
            }
        }
        host_plugin_window_list.clear();
        UnregisterClass(gWindowClassName, GetModuleHandle(nullptr));
    }
    bool isPluginWindow(HWND hwnd) {
        return nullptr != host_plugin_window::getWindowInstance(hwnd);
    }
}// namespace getWindowInstance

host_plugin_window* host_plugin_window::getWindowInstance(HWND handle) {
    dbgassert(handle);
    while (handle) {
        if (reinterpret_cast<int64_t>(GetProp(handle, "_DAW_PLWIN")) == int64_t{7}) {
            return getWindowByHWND(handle);
        }
        WINDOWINFO info{};
        info.cbSize = sizeof(info);
        GetWindowInfo(handle, &info);
        if (info.dwStyle & WS_CHILD) {
            handle = GetParent(handle);
        } else
            break;
    }
    return nullptr;
}

std::vector<host_plugin_window*>& host_plugin_window::getWindows() {
    return host_plugin_window_list;
}

bool host_plugin_window::init(effectbase* _plugin, const String& name, ivec2 size, bool resizeable) {
    HWND mainHWND = getMainHWND();
    assert(mainHWND);
    setRedirectKeysToDawMainWindow(!resizeable);
    this->plugin       = _plugin;
    HINSTANCE instance = GetModuleHandle(nullptr);
    registerWindowClass(instance);

    DWORD exStyle    = WS_EX_APPWINDOW;
    DWORD dwStyle    = WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS;
    if (resizeable) {
        dwStyle |= WS_SIZEBOX | WS_MAXIMIZEBOX;
    }

    hwnd = CreateWindowEx(exStyle, gWindowClassName, StringAsCStr(name), dwStyle,
                          0, 0, size.x, size.y, mainHWND, nullptr, instance, nullptr);
    if (!hwnd) {
        log_lf(Log::L_ERROR, "%s\n", StringAsCStr(FormatErrorMessage(GetLastError(), "Failed creating host_plugin_window: CreateWindowEx returned null")));
    }

    if (hwnd) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(mainHWND));
        SetProp(hwnd, "_DAW_PLWIN", reinterpret_cast<HANDLE>(int64_t{7}));

        // auto plugWindowSize = plugin->getWindowSize();
        // if (plugWindowSize.x > 0 && plugWindowSize.y > 0) {
            // resize(size);
        // }
        RECT rcOwner;
        RECT rcDlg;
        RECT rc;
        GetWindowRect(mainHWND, &rcOwner);
        GetWindowRect(hwnd, &rcDlg);
        CopyRect(&rc, &rcOwner);
        OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
        OffsetRect(&rc, -rc.left, -rc.top);
        OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);
        SetWindowPos(hwnd,
                        HWND_TOP,
                        rcOwner.left + (rc.right / 2),
                        rcOwner.top + (rc.bottom / 2),
                        0, 0,// Ignores size arguments.
                        SWP_NOSIZE);
        addWindow(this);
    }
    return hwnd != nullptr;
}

void host_plugin_window::close() {
    SetActiveWindow(getMainHWND());
    RECT rcDlg;
    if (GetWindowRect(hwnd, &rcDlg)) {
        plugin->storeWindowPosSize({rcDlg.left, rcDlg.top, rcDlg.right - rcDlg.left, rcDlg.bottom - rcDlg.top});
    }
    plugin->onClose();
    destroy();
}

void host_plugin_window::destroy() {
    plugin->onWindowDestroy();
    SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
    DestroyWindow(hwnd);
    removeWindow(this);
    delete this;
}

void host_plugin_window::show(ivec4 posSize, bool bSetPos, bool bSetSize) {
    auto flags = SWP_NOCOPYBITS | SWP_SHOWWINDOW;
    if (!bSetPos) {
        flags |= SWP_NOMOVE;
        posSize.x = posSize.y = 0;
    }
    if (!bSetSize) {
        flags |= SWP_NOSIZE;
        posSize.z = posSize.w = 0;
    }
    SetWindowPos(hwnd, HWND_TOP, posSize.x, posSize.y, posSize.z, posSize.w, flags);
    plugin->onShow(this);
    // auto plugWindowSize = plugin->getWindowSize();
    if (bSetSize) {
        resize(ivec2(posSize.z, posSize.w));
    }
}

ivec2 host_plugin_window::getContentSize() const {
    RECT r;
    GetClientRect(hwnd, &r);
    return { r.right - r.left, r.bottom - r.top };
}

void host_plugin_window::captureWindowFrame() {
    capturedFrame.w = 0;
    capturedFrame.h = 0;
    capturedFrame.bytes.clear();
    captureWindow(hwnd, capturedFrame);
}

void host_plugin_window::updateWindow() const {
    UpdateWindow(hwnd);
}

void host_plugin_window::onResize (ivec2 newSize)
{
	plugin->onWindowResize(newSize);
}

void host_plugin_window::resize(ivec2 newSize) const {
    if (getContentSize() == newSize)
        return;
    WINDOWINFO windowInfo{};
    windowInfo.cbSize = sizeof(WINDOWINFO);
    GetWindowInfo(hwnd, &windowInfo);
    RECT clientRect{};
    clientRect.right  = newSize.x;
    clientRect.bottom = newSize.y;
    AdjustWindowRectEx(&clientRect, windowInfo.dwStyle, false, windowInfo.dwExStyle);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, clientRect.right - clientRect.left,
                    clientRect.bottom - clientRect.top, SWP_NOMOVE | SWP_NOCOPYBITS | SWP_NOACTIVATE);
}

void host_plugin_window::setPosition(ivec2 newPos) {
    SetWindowPos(hwnd, HWND_TOP, newPos.x, newPos.y, 0, 0, SWP_NOSIZE | SWP_NOCOPYBITS | SWP_NOACTIVATE);
}

WINDOW_HANDLE host_plugin_window::getHWND() const {
    return hwnd;
}
#endif
