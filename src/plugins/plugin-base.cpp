
#include <cstdint>
#include "plugin-base.h"
#include <vstsdk-plugin-2.4/audioeffectx.h>
#include "config.h"
#include "str_util.h"
#include "logging.h"
#include "fileio.h"

#ifdef _WIN32
#include <windows.h>//this include SUCKS
#include <direct.h> //_getcwd
#endif

#include "plugins/plugin.h"
#include "plugins/plugincontrol.h"
#include "plugins/plugin-window.h"
#include "../gui/pluginviewcontainers.h"
#include "exceptions.h"
#include "handle-exceptions.h"
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
        ctrl->initApp(0, NULL);
        int32_t ctrlWidth = 0, ctrlHeight = 0;
        view->getFixedSize(&ctrlWidth, &ctrlHeight);
        pluginwindow* pluginWindow = createPluginWindow(this, ctrl, ctrlWidth, ctrlHeight);
        setEditor(pluginWindow);
    } catch (std::exception& e) {
        String excDesc = StringFormat("Fatal error: %s", e.what());
        ngui::show(StringAsCStr(excDesc), "Error", ngui::Style::Error, ngui::Buttons::OK);
        throw;
    } catch (...) {
        ngui::show("FATAL", "Error", ngui::Style::Error, ngui::Buttons::OK);
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
        strcpy(properties->label, "Left input");
        strcpy(properties->shortLabel, "L in");
        return true;
    } else if (index == 1) {
        strcpy(properties->label, "Right input");
        strcpy(properties->shortLabel, "R in");
        return true;
    }
    return false;
}

bool BasePluginVST2::getOutputProperties(VstInt32 index, VstPinProperties* properties) {
    if (index == 0 || index == 1) {
        properties->flags = kVstPinIsActive | kVstPinIsStereo;
    }
    if (index == 0) {
        strcpy(properties->label, "Left output");
        strcpy(properties->shortLabel, "L out");
        return true;
    } else if (index == 1) {
        strcpy(properties->label, "Right output");
        strcpy(properties->shortLabel, "R out");
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
    log_printf("open!\n", 0);
    if (editor) {
        log_printf("Editor already exists!\n", 0);
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
    ngui::show(description, "Error", ngui::Style::Error, ngui::Buttons::OK);
}

void initColor();// gui/gui.cpp
void onModuleLoad(HINSTANCE hInst) {
    isFirstPluginLoad = true;
    String moduleName = getModuleName(hInst);
    log_printf("moduleName %s\n", StringAsCStr(moduleName));
    String path = "";
    SplitPath(moduleName, &path, nullptr, nullptr, nullptr);
    String resPath = path + "/res/";
    log_printf("resPath %s\n", StringAsCStr(resPath));
    setResourcePath(resPath);

    String cwdPath = "";
    if (determineUserdataPath(cwdPath)) {
        setUserdataPath(cwdPath + "/daw/");
        log_printf("UserdataPath %s\n", StringAsCStr(toUserdataPath("")));
    }

    try {
        initColor();
        char pluginWindowClassName[32];
        sprintf_s(pluginWindowClassName, 32, "PLUGWND%I64X", (int64_t) &onModuleLoad);
        log_printf("window class name %s\n", pluginWindowClassName);
        glfwSetErrorCallback(glfw_plugin_error_callback);
        if (!glfwInit(pluginWindowClassName)) {
#ifdef _WIN32
            DWORD error    = GetLastError();
            String message = FormatErrorMessage(error, StringFormat("Couldn't initialize glfw (%d)", error));
            showerror(StringAsCStr(message));
#else
            showerror("Initialization failed. Couldn't initialize glfw");
#endif
            exit(EXIT_FAILURE);
        }

    EXC_CATCH_NO_THROW_DIALOG
}
void onModuleUnload() {
    try {
        glfwTerminate();
    EXC_CATCH_NO_THROW_DIALOG
}
#endif//BUILD_EXTERNAL_PLUGIN
