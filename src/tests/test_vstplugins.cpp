#include "str_util.h"
#include "tests/common/test_common.h"
#include "../host/vst_host.h"
#include "../host/plugin/vst_plugin.h"
#include "tls.h"
#include "project.h"
#include "appconfig.h"
#include "thread.h"
#include "fileio.h"
#include "platform.h"
#include "platform/win/platform_win.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <exception>

namespace {
    int32_t exitStatusCode = 0;

#if defined(_WIN32)
    struct TestCaseEntry {
        String pathToDll;
        int expectedStatus;
    };
    std::vector<TestCaseEntry> dllFilesToTest;

    size_t currentFileIdx = 0;

    void TickTest() {
        static vstpluginloadres res(0, nullptr);
        static int currentTimerTick = 0;
        static int numPluginsTested = 0;
        auto* host                  = vsthost::getInstance();
        if (res.plugin == nullptr) {
            if (currentFileIdx >= dllFilesToTest.size()) {
                PostQuitMessage(exitStatusCode);
                return;
            }
            TestCaseEntry testCase = dllFilesToTest[currentFileIdx++];
            res = host->loadPlugin(testCase.pathToDll, 0);
            if (res.result != testCase.expectedStatus) {
                printf("loadPlugin: %s %d => ERROR\n", StringAsCStr(testCase.pathToDll), res.result);
                exitStatusCode = 1;
                PostQuitMessage(exitStatusCode);
                return;
            } else {
                printf("loadPlugin: %s %d => GOOD\n", StringAsCStr(testCase.pathToDll), res.result);
            }
            if (res.result < 0) {
                res = vstpluginloadres(0, nullptr);
            }
        } else {
            bool hasUI = res.plugin->getFlagsVST() & effFlagsHasEditor;
            if (hasUI && currentTimerTick == 10) {
                res.plugin->show();
            } else if (hasUI && currentTimerTick == 30) {
                res.plugin->close();
            } else if ((hasUI && currentTimerTick == 40) || (!hasUI && currentTimerTick == 10)) {
                host->unloadPlugin(res.plugin, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
                res = vstpluginloadres(0, nullptr);
                currentTimerTick = -1;
                numPluginsTested++;
            } else {
                host->onTick();
                res.plugin->updateWindow();
            }
            currentTimerTick++;
        }
    }

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_DESTROY:
                PostQuitMessage(exitStatusCode);
                return exitStatusCode;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void createWin32Window() {
        WNDCLASS wc;

        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.lpszClassName = "Window";
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
        wc.lpszMenuName  = nullptr;
        wc.lpfnWndProc   = WndProc;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);

        RegisterClass(&wc);
        HWND hwnd = CreateWindow(wc.lpszClassName,
                                 "Window",
                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 100, 100, 350, 250,
                                 nullptr,
                                 nullptr,
                                 wc.hInstance,
                                 nullptr);
        dbgassert(hwnd != nullptr);

        setMainHWND(hwnd);

        RECT rcWindow;
        MONITORINFO monInfo;
        monInfo.cbSize = sizeof(monInfo);
        GetWindowRect(hwnd, &rcWindow);
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &monInfo);
        SetWindowPos(hwnd,
                     HWND_TOP,
                     ((monInfo.rcWork.right - monInfo.rcWork.left) - (rcWindow.right - rcWindow.left)) / 2,
                     ((monInfo.rcWork.bottom - monInfo.rcWork.top) - (rcWindow.bottom - rcWindow.top)) / 2,
                     0, 0,// Ignores size arguments.
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
#endif

}// namespace

extern volatile bool fataError;
void on_terminate();
void on_unexpected();

int main(int, char*[]) {
    seqthreads::registerThread("mainthread");

    std::set_terminate(on_terminate);
    std::set_unexpected(on_unexpected);
#ifdef USE_WIN32_EXC_HOOKS
    setExceptionHandler();
#endif

    dllFilesToTest.push_back(TestCaseEntry{ String("MISSING.dll"), -2 });
    std::vector<FileFound> files;
    findFilesWithExt("cpp-test-data/plugins-vst2-ok/", PLATFORM_PLUGIN_EXT, true, files);
    for (const FileFound& file : files) {
        dllFilesToTest.push_back(TestCaseEntry{ String(file.path), 0 });
    }

    int retVal     = 0;
    auto audiohost = std::make_unique<vsthost>();
    vsthost::assignMasterCallback(audiohost.get());
    daw_tls::tlsinstance _tls;
    _tls.tlsInitialized = true;
    _tls.config         = new app_config_t{};
    _tls.host           = audiohost.get();
    daw_tls::setTls(_tls);
    try {

#if defined(_WIN32)
        createWin32Window();
        double tmLastTick = getTimeHPC();
        bool quit         = false;
        while (!fataError && !quit) {
            double tmNow = getTimeHPC();
            if (tmNow - tmLastTick >= 0.025) {
                tmLastTick = tmNow;
                TickTest();
            }
            DWORD timeout = 5;
            MsgWaitForMultipleObjects(0, nullptr, FALSE, timeout, QS_ALLEVENTS);
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    retVal = static_cast<int>(msg.wParam);
                    quit   = true;
                    break;
                }
                DispatchMessage(&msg);
            }
        }
        printf("Test result: %d\n", retVal);
#endif
    } catch (std::exception& e) {
        printf("std::exception: %s\n", e.what());
        fataError = true;
    }
    _tls.host->unload();
    _tls.host->destroy();
    return fataError ? 0x5A5A5A5A : retVal;
}
