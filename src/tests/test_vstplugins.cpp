
#ifdef _WIN32
#include "str_util.h"
#include "test_common.h"
#include <windows.h>
#include "../host/vst_host.h"
#include "../host/plugin/vst_plugin.h"
#include "tls.h"
#include "project.h"

namespace {


static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const std::vector<String> files{"mdaLimiter.dll", "mdaPiano.dll"};
int rIdx = 0;
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
		return;
	}
	reentrantblocker block(reentrant);


	static int tick = 0;
	static int test = 0;
	auto* audiohost = vsthost::getInstance();
	if (res.plugin == NULL) {
		if (test > 122) {
            PostQuitMessage(0);
		}
		if (rIdx >= (int32_t)files.size()) {
            PostQuitMessage(0);
			return;
		}
	    String f = files[rIdx];
		res = audiohost->loadPlugin(f);
	    LOG("loadPlugin: %s %d\n", StringAsCStr(f), res.result);
	    if (res.result != 0) {
			res = vstpluginloadres(0, NULL);
	    }
		rIdx++;
	} else {
		if (tick == 0) {
			res.plugin->show();
		} else if (tick == 72) {
			res.plugin->close();
		} else if (tick == 92) {
			LOG("unloadPlugin");
			audiohost->unloadPlugin(res.plugin);
			res = vstpluginloadres(0, NULL);
			tick = -1;
			test++;
		} else {
			audiohost->onTick();
			res.plugin->updateWindow();
		}
		tick++;
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
}


int main(int argc, char* argv[]) {
    auto audiohost = std::make_unique<vsthost>();
	vsthost::assignMasterCallback(audiohost.get());
    daw_tls::tlsinstance& tls = daw_tls::getTls();
    tls.host = audiohost.get();
    MSG msg;
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
    assert(hwnd != NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    while  (GetMessage(&msg, NULL, 0, 0))
    {
        DispatchMessage(&msg);
    }
	vsthost::getInstance()->unload();
	vsthost::getInstance()->destroy();
}
#endif
