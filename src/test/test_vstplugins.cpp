
#include "str_util.h"
#include "tests.h"
#include <iomanip>
#include <windows.h>
#include "../host/vst_host.h"
#include "../host/vst_plugin.h"
#include "fileio.h"
#include "project.h"
namespace {

project_globals_t project;
static vsthost* audiohost = new vsthost(project);

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static std::vector<FileFound> files;
//static String vstPlugPath = "C:/VstPlugins/xfer/Serum_x64.dll";
static String vstPlugPath = "C:/PluginManager/configs/default/hosts/Ableton/categories/sonalksis/Sonalksis SV-517Mk2 Stereo EQ.dll";
bool singleInstanceTest = false;
int rIdx;

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
	if (res.plugin == NULL) {
		if (test > 122) {
            PostQuitMessage(0);
		}
	    FileFound& f = files[rIdx];
	    if (!singleInstanceTest) {
	    	vstPlugPath = f.path;
	    }
		res = audiohost->loadPlugin(vstPlugPath);
	    LOG("loadPlugin: %s %d\n", StringAsCStr(vstPlugPath), res.result);
	    if (res.result != 0) {
			res = vstpluginloadres(0, NULL);
	    }
		rIdx++;
		if (rIdx >= (int32_t)files.size()) {
			rIdx = 0;
		}
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
			res.plugin->updateDisplay();
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


void testVSTPlugins()
{
    findFilesWithExt("C:/PluginManager/configs/default/hosts/Ableton/categories", "dll", true, files);
    LOG("Found %u files", (uint32_t)files.size());
    srand(time(NULL));
    rIdx = rand()%(int32_t)files.size();
    FileFound& f = files[rIdx];
//    LOG("RAND: %s %s %s", StringAsCStr(f.path), StringAsCStr(f.name), StringAsCStr(f.ext));
    if (!singleInstanceTest)
    	vstPlugPath = f.path;
//    return;
    MSG msg;
    HWND hwnd;
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
}
