#include "str_util.h"
#include "test_common.h"
#include "../host/vst_host.h"
#include "../host/plugin/vst_plugin.h"
#include "tls.h"
#include "project.h"
#include "appconfig.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
int32_t exitStatusCode = 1;

#if defined(_WIN32) 

static const std::vector<String> dllFilesToTest{"cpp-test-data/mdaLimiter.dll", "cpp-test-data/mdaPiano.dll"};
size_t currentFileIdx = 0;
HWND hwnd = NULL;

class reentrantblocker {
	bool& boolField;
public:
	reentrantblocker(bool& _boolField) : boolField(_boolField) {
		boolField = true;
	}
	~reentrantblocker() {
		boolField = false;
	}
	bool isReentrant() {
		return boolField;
	}
};
VOID CALLBACK TimerCallback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	static vstpluginloadres res(0, NULL);
	static bool reentrant = false;
	if (reentrant) {
    	exitStatusCode = 1;
	    printf("TimerCallback reentrant\n");
		return;
	}
	reentrantblocker block(reentrant);


	static int currentTimerTick = 0;
	static int numPluginsTested = 0;
	auto* host = vsthost::getInstance();
	if (res.plugin == NULL) {
		if (numPluginsTested > 122) {
            PostQuitMessage(0);
			return;
		}
		if (currentFileIdx >= dllFilesToTest.size()) {
            PostQuitMessage(0);
			return;
		}
	    String f = dllFilesToTest[currentFileIdx];
		res = host->loadPlugin(f, 0);
	    printf("loadPlugin: %s %d\n", StringAsCStr(f), res.result);
	    if (res.result != 0) {
	    	exitStatusCode = 1;
			res = vstpluginloadres(0, NULL);
            PostQuitMessage(0);
			return;
	    }
		currentFileIdx++;
	} else {
		if (currentTimerTick == 0) {
			res.plugin->show();
		} else if (currentTimerTick == 72) {
			res.plugin->close();
		} else if (currentTimerTick == 92) {
			printf("unloadPlugin\n");
			host->unloadPlugin(res.plugin);
			res = vstpluginloadres(0, NULL);
			currentTimerTick = -1;
			numPluginsTested++;
		} else {
			host->onTick();
			res.plugin->updateWindow();
		}
		currentTimerTick++;
	}
}
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
	case WM_CREATE:
		SetTimer(NULL, 0, 2, TimerCallback);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void createWin32Window() {
    WNDCLASS wc;

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpszClassName = "Window";
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpszMenuName  = NULL;
    wc.lpfnWndProc   = WndProc;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClass(&wc);
    hwnd = CreateWindow(wc.lpszClassName, "Window",
                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                100, 100, 350, 250, NULL, NULL, wc.hInstance, NULL);
    dbgassert(hwnd != NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

}
#endif

}


int main(int argc, char* argv[]) {
    auto audiohost = std::make_unique<vsthost>();
    try {
    	vsthost::assignMasterCallback(audiohost.get());
    	daw_tls::tlsinstance _tls;
    	_tls.tlsInitialized = true;
        _tls.config = new app_config_t{};
        _tls.host = audiohost.get();
    	daw_tls::setTls(_tls);

#if defined(_WIN32)
    	exitStatusCode = 0;
    	createWin32Window();
        MSG msg;
        while  (GetMessage(&msg, NULL, 0, 0))
        {
            DispatchMessage(&msg);
        }
#endif

    	vsthost::getInstance()->unload();
    	vsthost::getInstance()->destroy();
    	return exitStatusCode;
    } catch(std::exception& e) {
    	printf("std::exception: %s\n", e.what());
    	return 1;
    }
	return 0;
}
