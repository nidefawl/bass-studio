#ifdef _WIN32
#include "str_util.h"
#include "host/vst_window.h"
#include "host/vst_host.h"
#include "host/plugin/vst_plugin.h"
#include "host/mainctrl.h"
#include <Windows.h>
#include <winuser.h>
#include <tchar.h>

#include "math/vec.h"
#include "math/seq_math.h"
#include "platform/win/platform_win.h"

#define WINDOW_HANDLE HWND
#define WIN32API_CALLBACK_TYPE __stdcall

LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);// window.cpp

namespace {
    const TCHAR* gWindowClassName = _T("VSTHOSTWINDOW");
    std::vector<vst_window*> vst_window_list;
    void addWindow(vst_window* window) {
        vst_window_list.push_back(window);
    }
    void removeWindow(vst_window* window) {
        auto it = std::find(vst_window_list.begin(), vst_window_list.end(), window);
        if (it != vst_window_list.end())
            vst_window_list.erase(it);
    }
    vst_window* getWindowByHWND(WINDOW_HANDLE hwnd) {
        auto it = std::find_if(vst_window_list.begin(), vst_window_list.end(), [hwnd](vst_window* window) {
            return window->getHWND() == hwnd;
        });
        if (it != vst_window_list.end())
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
                log_printf("Failed creating bitmap: %d\n", GetLastError());
            }

            HDC memDC       = CreateCompatibleDC(hdc);
            HBITMAP prevOBj = (HBITMAP) SelectObject(memDC, bitmap);
            //if (!PrintWindow(hwnd, memDC, PW_CLIENTONLY)) {
            //    log_printf("PrintWindow failed: %d\n", GetLastError());
            //}
            time.reset();
            if (!BitBlt(memDC, 0, 0, width, height, hdc, 0, 0, SRCCOPY)) {
                log_printf("BitBlt failed: %d\n", GetLastError());
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
        vst_window* window = reinterpret_cast<vst_window*>((LONG_PTR) GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (window) {
            vstplugin* plugin = window->getPlugin();
            switch (message) {
                case WM_ERASEBKGND: {
                    return 1;// don't draw background
                }
                case WM_SIZE: {
                    plugin->onResize(window, window->getContentSize());
                    return 0;
                }
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                case WM_KEYUP:
                case WM_SYSKEYUP:
                    {
                        HWND hwndMain = getMainHWND();
                        if (hwndMain) {
                            return appWndProc(hwndMain, message, wParam, lParam);
                        }
                    }
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

                    ivec2 constraintSize = plugin->constrainSize(window, newClientSize);
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

vst_window* vst_window::make(vstplugin* plugin, const String& name, ivec2 size, bool resizeable) {
    auto* window = new vst_window();
    if (window->init(plugin, name, size, resizeable))
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
        UnregisterClass(gWindowClassName, GetModuleHandle(nullptr));
    }
    bool isVstWindow(HWND hwnd) {
        return nullptr != vst_window::getVSTWindow(hwnd);
    }
}// namespace vst_window_mgr

vst_window* vst_window::getVSTWindow(HWND handle) {
    dbgassert(handle);
    while (handle) {
        if (reinterpret_cast<int64_t>(GetProp(handle, "_DAW_VST2WIN")) == int64_t{7}) {
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

std::vector<vst_window*>& vst_window::getWindows() {
    return vst_window_list;
}

bool vst_window::init(vstplugin* _plugin, const String& name, ivec2 size, bool resizeable) {
    HWND mainHWND = getMainHWND();
    assert(mainHWND);
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
        log_lf(Log::L_ERROR, "%s", StringAsCStr(FormatErrorMessage(GetLastError(), "Failed creating vst_window: CreateWindowEx returned null")));
    }

    if (hwnd) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(mainHWND));
        SetProp(hwnd, "_DAW_VST2WIN", reinterpret_cast<HANDLE>(int64_t{7}));

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

void vst_window::close() {
    SetActiveWindow(getMainHWND());
    plugin->onClose();
    destroy();
}

void vst_window::destroy() {
    plugin->onWindowDestroy();
    SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
    DestroyWindow(hwnd);
    removeWindow(this);
    delete this;
}

void vst_window::show() {
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOCOPYBITS | SWP_SHOWWINDOW);
    plugin->onShow(this);
}

ivec2 vst_window::getContentSize() const {
    RECT r;
    GetClientRect(hwnd, &r);
    return { r.right - r.left, r.bottom - r.top };
}

void vst_window::captureWindowFrame() {
    capturedFrame.w = 0;
    capturedFrame.h = 0;
    capturedFrame.bytes.clear();
    captureWindow(hwnd, capturedFrame);
}

void vst_window::updateWindow() const {
    UpdateWindow(hwnd);
}

void vst_window::resize(ivec2 newSize) const {
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

WINDOW_HANDLE vst_window::getHWND() const {
    return hwnd;
}
#endif
