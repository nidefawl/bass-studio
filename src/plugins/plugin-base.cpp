
#include "types.h"
#include "plugin-base.h"
#include <vstsdk-plugin-2.4/audioeffectx.h>
#include "config.h"
#include "str_util.h"
#include "logging.h"
#include "fileio.h"

#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <windows.h>//this include SUCKS
#include <direct.h> //_getcwd
#include "str_win32.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "plugins/plugin.h"
#include "plugins/plugincontrol.h"
#include "plugins/plugin-window.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "exceptions.h"
#include "msgbox.h"
#include "platform.h"

#include "assert_dbg.h"

#if BUILD_EXTERNAL_PLUGIN
#include <GLFW/glfw3.h>
namespace MouseCursors {
    void initCursors();// mousecursor.cpp
}
#endif//BUILD_EXTERNAL_PLUGIN


#if BUILD_EXTERNAL_PLUGIN
bool isFirstPluginLoad = false;
extern HMODULE hInstance;
String getModuleName(HMODULE module);//platform_win.cpp
#endif //BUILD_EXTERNAL_PLUGIN

pluginwindow* createPluginWindow(AudioEffect* _effect, std::shared_ptr<PluginControl> _ctrl, int w, int h);

void BasePluginVST2::createEditorWindow(std::shared_ptr<PluginViewContainers> view) {
    try {
        std::shared_ptr<PluginControl> ctrl = std::make_shared<PluginControl>(view);
        ctrl->initApp(std::vector<String>());
        int32_t ctrlWidth = 0, ctrlHeight = 0;
        view->getFixedSize(&ctrlWidth, &ctrlHeight);
        pluginwindow* pluginWindow = createPluginWindow(this, ctrl, ctrlWidth, ctrlHeight);
        setEditor(pluginWindow);
    } catch (std::exception& e) {
        ngui::showNotification(ngui::Style::Error, "Fatal error", e.what());
        throw;
    }
}
BasePluginVST2::BasePluginVST2(audioMasterCallback audioMaster,
                               const char* pluginUIDStr,
                               VstInt32 numPrograms,
                               VstInt32 numParams,
                               VstInt32 numInputs,
                               VstInt32 numOutputs)
    : AudioEffectX(audioMaster, numPrograms, numParams)
{
    if (audioMaster) {
        setInitialDelay(0);
        this->cEffect.version = 2;
        setNumInputs(numInputs);
        setNumOutputs(numOutputs);
        canProcessReplacing(true);
        noTail(false);
        isSynth(false);
        int id = 0;
        memcpy(&id, pluginUIDStr, sizeof(VstInt32));
        setUniqueID(id);
    }
    setProgram(0);

    suspend();
    dbgassert(this->curProgram == 0);
    dbgassert(this->numPrograms == numPrograms);
    dbgassert(this->numParams == numParams);
    dbgassert(cEffect.numInputs == numInputs);
    dbgassert(cEffect.numOutputs == numOutputs);
}

bool BasePluginVST2::getInputProperties(VstInt32 index, VstPinProperties* properties) {
    if (index == 0 || index == 1) {
        properties->flags = kVstPinIsActive | kVstPinIsStereo;
    }
    if (index == 0) {
        strcpy(properties->label, "Stereo Input");
        strcpy(properties->shortLabel, "Input");
        return true;
    }
    if (index == 1) {
        strcpy(properties->label, "Stereo Input R");
        strcpy(properties->shortLabel, "In R");
        return true;
    }
    return false;
}

bool BasePluginVST2::getOutputProperties(VstInt32 index, VstPinProperties* properties) {
    if (index == 0 || index == 1) {
        properties->flags = kVstPinIsActive | kVstPinIsStereo;
    }
    if (index == 0) {
        strcpy(properties->label, "Stereo Output");
        strcpy(properties->shortLabel, "Output");
        return true;
    }
    if (index == 1) {
        strcpy(properties->label, "Stereo Output R");
        strcpy(properties->shortLabel, "Out R");
        return true;
    }
    return false;
}

bool BasePluginVST2::getVendorString(char* text) {
    vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
    return true;
}

void BasePluginVST2::open() {
#if BUILD_EXTERNAL_PLUGIN
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE* f;
    freopen_s(&f, "CON", "w", stdout);
    log_printf("open!\n");
    if (editor) {
        log_printf("Editor already exists!\n");
    }
#endif//BUILD_EXTERNAL_PLUGIN
    createEditorWindow(createView());
#if BUILD_EXTERNAL_PLUGIN
#ifdef _WIN32
    if (!isFirstPluginLoad) {
        return;
    }
#endif
    isFirstPluginLoad = false;
    MouseCursors::initCursors();//TODO: call MouseCursors::destroy() on exit of last instance

#endif//BUILD_EXTERNAL_PLUGIN
}

void BasePluginVST2::close() {
    if (editor) {
        delete editor;
        editor = nullptr;
    }
}

#if BUILD_EXTERNAL_PLUGIN

static void glfw_plugin_error_callback(int error, const char* description) {
    log_printf("glfw-error %d: %s\n", error, description);
    logStackTrace();
}

static void showerror(const char* description) {
    ngui::showNotification(ngui::Style::Error, "Error", description);
}

void initColor();// gui/gui.cpp
void onModuleLoad(HINSTANCE hInst) {
    isFirstPluginLoad = true;
    String moduleName = getModuleName(hInst);
    log_lf(Log::L_DEBUG, "moduleName %s\n", StringAsCStr(moduleName));
    String path = "";
    SplitPath(moduleName, &path, nullptr, nullptr, nullptr);
    App::Platform::initPlatformEnvironment("daw", path);
    log_lf(Log::L_DEBUG, "resPath %s\n", StringAsCStr(App::Platform::toResourcePath("")));
    try {
        initColor();
        glfwSetErrorCallback(glfw_plugin_error_callback);
#ifdef _WIN32
        static wchar_t pluginWindowClassName[128]{};
        int len = swprintf_s(pluginWindowClassName, 128, L"PLUGIN_WINDOW_%06X", (int) ( ((uint64_t) &onModuleLoad) & 0xFFFFFF ) );
        dbgassert(len > 0 && len <  128);
        if (len > 0 && len <  128) {
            glfwSetWin32WindowClassName(pluginWindowClassName);
        }
        
#endif

        if (!glfwInit()) {
#ifdef _WIN32
            DWORD error    = GetLastError();
            String message = FormatErrorMessage(error, StringFormat("Couldn't initialize glfw (%d)", error));
            showerror(StringAsCStr(message));
#else
            showerror("Initialization failed. Couldn't initialize glfw");
#endif
            //exit(EXIT_FAILURE);
        }
    } catch (std::exception& e) {
        ngui::showNotification(ngui::Style::Error, "Fatal error", e.what());
    }
}
void onModuleUnload() {
    try {
        glfwTerminate();
    } catch (std::exception& e) {
        ngui::showNotification(ngui::Style::Error, "Fatal error", e.what());
    }
}
#endif//BUILD_EXTERNAL_PLUGIN
