#include "TestBase.hpp"
#include "host/host_plugin_loadresult.h"
#include "str_util.h"
#include "common/test_common.h"
#include "host/host_pluginmanager.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/host.h"
#include "host/host_pluginmanager.h"
#include "tls.h"
#include "host/project/project.h"
#include "appconfig.h"
#include "thread.h"
#include "fileio.h"
#include "platform.h"

#ifdef _WIN32
#include "platform/win/platform_win.h"
#include <windows.h>
#endif

#include <exception>

namespace {

    using DAW::Host::SharedLibState;
    using DAW::Host::SharedLibPluginType;
    using DAW::Host::LoadResultSharedLibrary;
    using DAW::Host::LoadResultPluginImpl;

    int32_t exitStatusCode = 0;

#if defined(_WIN32)
    struct TestCaseEntry {
        String pathToDll;
        SharedLibState expectedState;
        SharedLibPluginType expectedType;
    };
    std::vector<TestCaseEntry> dllFilesToTest;

    size_t currentFileIdx = 0;
    void TickTest() {
        static LoadResultPluginImpl res{LoadResultSharedLibrary::FromError(SharedLibState::FILE_NOT_FOUND, "")};
        static int currentTimerTick = 0;
        static int numPluginsTested = 0;
        auto* host = daw_tls::getTls().host;
        if (res.plugin == nullptr || res.vstPlugin == nullptr) {
            if (currentFileIdx >= dllFilesToTest.size()) {
                PostQuitMessage(exitStatusCode);
                return;
            }
            TestCaseEntry testCase = dllFilesToTest[currentFileIdx++];
            auto loadresult = host->loadPlugin(testCase.pathToDll, 0);
            res = *loadresult;
            if (res.library.state != testCase.expectedState || res.library.type != testCase.expectedType) {
                printf("loadPlugin: %s %d => ERROR\n", StringAsCStr(testCase.pathToDll), static_cast<int32_t>(res.library.state));
                exitStatusCode = 1;
                PostQuitMessage(exitStatusCode);
                return;
            } else {
                printf("loadPlugin: %s %d => GOOD\n", StringAsCStr(testCase.pathToDll), static_cast<int32_t>(res.library.state));
            }
            if (res.library.state != SharedLibState::SUCCESS) {
                res = LoadResultPluginImpl{LoadResultSharedLibrary::FromError(SharedLibState::FILE_NOT_FOUND, "")};
            }
        } else {
            bool hasUI = res.vstPlugin->getFlagsVST() & effFlagsHasEditor;
            if (hasUI && currentTimerTick == 10) {
                res.plugin->showWindow(false);
            } else if (hasUI && currentTimerTick == 30) {
                res.plugin->closeWindow();
            } else if ((hasUI && currentTimerTick == 40) || (!hasUI && currentTimerTick == 10)) {
                host->unloadPlugin(res.plugin);
                res = LoadResultPluginImpl{LoadResultSharedLibrary::FromError(SharedLibState::FILE_NOT_FOUND, "")};
                currentTimerTick = -1;
                numPluginsTested++;
            } else {
                host->onTick();
                res.plugin->updateFromMainThread();
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

extern volatile bool fatalError;
void on_terminate();

int main(int, char*[]) {
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);

    std::set_terminate(on_terminate);
#ifdef USE_WIN32_EXC_HOOKS
    setExceptionHandler();
#endif

#ifdef _WIN32
    dllFilesToTest.push_back(TestCaseEntry{ String("MISSING.dll"), SharedLibState::FILE_NOT_FOUND, SharedLibPluginType::UNKNOWN });
    std::vector<FileFound> files;
    findFilesWithExt(TEST_PATH("plugins-vst2-ok/"), PLATFORM_PLUGIN_EXT, true, files);
    for (const FileFound& file : files) {
        dllFilesToTest.push_back(TestCaseEntry{ String(file.path), SharedLibState::SUCCESS, SharedLibPluginType::VST2 });
    }
#endif

    int retVal     = 0;
    auto host = std::make_unique<DAW::Host::Host>();
    auto pluginMgr = host.get();
    DAW::Host::PluginManager::assignMasterCallback(pluginMgr);
    auto& tls = daw_tls::initNewTls();
    tls.host = host.get();
    tls.pluginManager = pluginMgr;
    host->setTls(tls);
    try {

#ifdef _WIN32
        createWin32Window();
        auto tmLastTick = getTimeMillis();
        bool quit         = false;
        while (!fatalError && !quit) {
            auto tmNow = getTimeMillis();
            if (tmNow - tmLastTick >= 25) {
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
        fatalError = true;
    }
    tls.host->unload();
    tls.host->destroy();
    return fatalError ? 0x5A5A5A5A : retVal;
}
