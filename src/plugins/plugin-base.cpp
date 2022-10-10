
#include "appconfig.h"
#include "tls.h"
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
#include "host/mainctrl.h"

#include "assert_dbg.h"

#include <GLFW/glfw3.h>
namespace MouseCursors {
    void initCursors();// mousecursor.cpp
}
#ifdef __linux__
#include  <link.h>
static int cb_(struct dl_phdr_info *Info, size_t Sz, void *Data)
{
    //printf("Name=%s\n",Info->dlpi_name);
    uintptr_t a = (uintptr_t)cb_;

    //for each elf-header, iterate thru program headers
    for(size_t i=0; i<Info->dlpi_phnum; i++){
        // [b,e) is the corresponding segment
        uintptr_t b = Info->dlpi_addr + Info->dlpi_phdr[i].p_paddr;
        uintptr_t e = b + Info->dlpi_phdr[i].p_memsz;
        if(a>=b && a<e){
            //if this is cb_'s segment, we're done
            printf("NAME=%s\n", Info->dlpi_name);
            *((String*)Data) = Info->dlpi_name;
            //nonzero signals end of iteration
            return 1;
        }

    }
    return 0;
}

String getModuleNameLinux() {
    //iterate thru elf-headers
    String s;
    dl_iterate_phdr(cb_, &s);
    return s;
}
#endif



bool isFirstPluginLoad = false;
#ifdef _WIN32
extern HMODULE hInstance;
String getModuleName(HMODULE module);//platform_win.cpp
#endif

pluginwindow* createPluginClientVst2Window(AudioEffectX* _effect, std::shared_ptr<PluginControl> _ctrl, int w, int h);

void BasePluginVST2::createEditorWindow(std::shared_ptr<PluginViewContainers> view) {
    try {
        auto tls = daw_tls::getTls();
        auto mainCtrl = tls.mainCtrl;
        std::shared_ptr<PluginControl> ctrl = std::make_shared<PluginControl>(mainCtrl, view);
        ctrl->initApp(std::vector<String>());
        if(mainCtrl) {
            ctrl->setDawCtrl(mainCtrl);
            ctrl->m_scale     = mainCtrl->m_scale;
            *ctrl->getTheme() = *mainCtrl->getTheme();
        }
        int32_t ctrlWidth = 0, ctrlHeight = 0;
        view->getFixedSize(&ctrlWidth, &ctrlHeight);

		char* ptr = static_cast<char*>(alloca(STR_GET_STACK_BUF_SIZE));
		*ptr = 0;
		this->getEffectName(ptr);
        ctrl->setWindowName(ptr);
        pluginwindow* pluginWindow = createPluginClientVst2Window(this, std::move(ctrl), ctrlWidth, ctrlHeight);
        setEditor(pluginWindow);
    } catch (std::exception& e) {
        ngui::showNotification(ngui::Style::Error, "Fatal error", e.what());
        throw;
    }
}
BasePluginVST2::BasePluginVST2(audioMasterCallback audioMaster,
                               uint32_t pluginUniqueID,
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
        setUniqueID(static_cast<VstInt32>(pluginUniqueID));
    }
    setProgram(0);

    suspend();
    dbgassert(this->curProgram == 0);
    dbgassert(this->numPrograms == numPrograms);
    dbgassert(this->numParams == numParams);
    dbgassert(cEffect.numInputs == numInputs);
    dbgassert(cEffect.numOutputs == numOutputs);
}
BasePluginVST2::BasePluginVST2(audioMasterCallback audioMaster,
                               uint32_t pluginUniqueID)
    : AudioEffectX(audioMaster, 0, 0)
{
    setInitialDelay(0);
    this->cEffect.version = 2;
    canProcessReplacing(true);
    noTail(false);
    isSynth(false);
    setUniqueID(static_cast<VstInt32>(pluginUniqueID));
    setProgram(0);
    suspend();
}

bool BasePluginVST2::getInputProperties(VstInt32 index, VstPinProperties* properties) {
    if (index == 0 || index == 1) {
        properties->flags = kVstPinIsActive | kVstPinIsStereo;
    }
    if (index == 0) {
        safe_strcpy(properties->label, "Stereo Input");
        safe_strcpy(properties->shortLabel, "Input");
        return true;
    }
    if (index == 1) {
        safe_strcpy(properties->label, "Stereo Input R");
        safe_strcpy(properties->shortLabel, "In R");
        return true;
    }
    return false;
}

bool BasePluginVST2::getOutputProperties(VstInt32 index, VstPinProperties* properties) {
    if (index == 0 || index == 1) {
        properties->flags = kVstPinIsActive | kVstPinIsStereo;
    }
    if (index == 0) {
        safe_strcpy(properties->label, "Stereo Output");
        safe_strcpy(properties->shortLabel, "Output");
        return true;
    }
    if (index == 1) {
        safe_strcpy(properties->label, "Stereo Output R");
        safe_strcpy(properties->shortLabel, "Out R");
        return true;
    }
    return false;
}

bool BasePluginVST2::getVendorString(char* text) {
    vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
    return true;
}

// internal API
std::shared_ptr<PluginViewContainers> BasePluginVST2::openViewCtrVst2(int32_t uiId) {
    for (auto& existingView : views) {
        if (!existingView->isInUse() && existingView->getUiId() == uiId) {
            existingView->setUsed();
            return existingView;
        }
    }
    auto newView = createViewCtrVst2();
    if (newView && newView->isViewSupported(uiId)) {
        newView->setUiId(uiId);
        newView->setUsed();
        views.push_back(newView);
    }
    return newView;
}
std::shared_ptr<PluginViewContainers> BasePluginVST2::getViewCtrVst2(int32_t uiId) {
    for (auto& existingView : views) {
        if (existingView->getUiId() == uiId) {
            return existingView;
        }
    }
    return nullptr;
}

param_converted_t BasePluginVST2::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
    //TODO: use std::from_chars when floating point version arrives in libc++
    auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
    return {fTextFieldVal, false};
}
void BasePluginVST2::addPropertiesParameterTooltip(Table::tbl& table, int idx) {
    
}
void BasePluginVST2::onWindowResize(ivec2 size) {
    if (editor) {
        pluginwindow* pluginWindow = static_cast<pluginwindow*>(editor);
        pluginWindow->onHostWindowResize(size.x, size.y);
    }
}

void BasePluginVST2::open() {
    if (this->bIsExternalInstance) {
#ifdef _WIN32
        AllocConsole();
        AttachConsole(GetCurrentProcessId());
        FILE* f;
        freopen_s(&f, "CON", "w", stdout);
#endif
        log_printf("open!\n");
        if (editor) {
            log_printf("Editor already exists!\n");
    }
    }
    createEditorWindow(openViewCtrVst2(UID_VIEW_CTR_WINDOW));
    if (this->bIsExternalInstance) {
#ifdef _WIN32
        if (!isFirstPluginLoad) {
            return;
        }
#endif
        isFirstPluginLoad = false;
        MouseCursors::initCursors();//TODO: call MouseCursors::destroy() on exit of last instance
    }
}

void BasePluginVST2::close() {
    if (editor) {
        delete editor;
        editor = nullptr;
    }
}

static void glfw_plugin_error_callback(int error, const char* description) {
    log_printf("glfw-error %d: %s\n", error, description);
    logStackTrace();
}

static void showerror(const char* description) {
    ngui::showNotification(ngui::Style::Error, "Error", description);
}

void initColor();// gui/gui.cpp
#ifdef _WIN32
void onModuleLoad(HINSTANCE hInst) {
    String moduleName = getModuleName(hInst);
#else
void onModuleLoad() {
    String moduleName = getModuleNameLinux();
#endif
    if (!daw_tls::isTlsInitialized()) {
        daw_tls::initNewTls();
    }
    log_lf(Log::L_DEBUG, "moduleName %s\n", StringAsCStr(moduleName));
    String path = "";
    SplitPath(moduleName, &path, nullptr, nullptr, nullptr);
    App::Platform::initPlatformEnvironment("daw", path);
    isFirstPluginLoad = true;
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
#ifndef NDEBUG
            daw_tls::getTls().runtime->printWindowFps = true;
#endif
#ifdef _WIN32
            DWORD error    = GetLastError();
            String message = FormatErrorMessage(error, StringFormat("Couldn't initialize glfw (%lu)", error));
            showerror(StringAsCStr(message));
#else
            showerror("Initialization failed. Couldn't initialize glfw");
#endif
            //exit(EXIT_FAILURE);
        }
        DAW::UI::InitKeynames();
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
