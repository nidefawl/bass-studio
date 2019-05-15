#include <stdint.h>
#include "plugin-base.h"
#include "../vstsdk-plugin-2.4/audioeffectx.h"
#include "str_util.h"
#include "logging.h"
#include "fileio.h"
#include <windows.h> //this include SUCKS
#include <direct.h> //_getcwd

#include "plugins/plugin.h"
#include "plugins/plugincontrol.h"
#include "../gui/pluginviewcontainers.h"
#include "exceptions.h"
#include "handle-exceptions.h"
#include "msgbox.h"
#include "assert_dbg.h"

#ifndef BUILD_BUILTIN_EFFECT
#include <GLFW/glfw3.h>
namespace MouseCursors {
void initCursors(); // mousecursor.cpp
}
#endif



#ifndef BUILD_BUILTIN_EFFECT
bool isFirstPluginLoad = false;
extern HMODULE hInstance;
String getModuleName(HMODULE module); //platform_win.cpp
#endif

AEffEditor* createPluginWindow(AudioEffect *_effect, std::shared_ptr<PluginControl> _ctrl, int w, int h);

void BasePluginVST2::createEditorWindow(PluginViewContainersImpl* view) {
	try {
		std::shared_ptr<PluginControl> ctrl = std::make_shared<PluginControl>(view);
		ctrl->initApp(0, NULL);
		editor = createPluginWindow(this, ctrl, view->width, view->height);
	} catch (std::exception& e) {
		String excDesc = StringFormat("Fatal error: %s", e.what());
		ngui::show(StringAsCStr(excDesc), "Error", ngui::Style::Error, ngui::Buttons::OK);
//			exit(1);
		throw;
	} catch (...) {
		ngui::show("FATAL", "Error", ngui::Style::Error, ngui::Buttons::OK);
//			exit(1);
		throw;
	}

}
BasePluginVST2::BasePluginVST2(audioMasterCallback audioMaster,
		const char* pluginUIDStr,
		VstInt32 numPrograms,
		VstInt32 numParams,
		VstInt32 numInputs,
		VstInt32 numOutputs)
	: AudioEffectX(audioMaster, numPrograms, numParams) {

	if (audioMaster)
	{
		setInitialDelay(0);
		this->cEffect.version = 2;
		setNumInputs(numInputs);
		setNumOutputs(numOutputs);
		canProcessReplacing(true);
		noTail(false);
		isSynth(false);
		int id = 0;
		memcpy(&id, pluginUIDStr, sizeof(VstInt32));
		setUniqueID (id);
	}
	setProgram(0);

	suspend ();
	dbgassert(this->curProgram == 0);
	dbgassert(this->numPrograms == numPrograms);
	dbgassert(this->numParams == numParams);
	dbgassert(cEffect.numInputs == numInputs);
	dbgassert(cEffect.numOutputs == numOutputs);
}

BasePluginVST2::~BasePluginVST2() {

}



bool BasePluginVST2::getInputProperties (VstInt32 index, VstPinProperties* properties)
{
	if (index == 0 || index == 1)
	{
		properties->flags = kVstPinIsActive | kVstPinIsStereo;
	}
	if (index == 0)
	{
		strcpy(properties->label,	   "Left input");
		strcpy(properties->shortLabel, "L in");
		return true;
	}
	else if (index == 1)
	{
		strcpy(properties->label,	   "Right input");
		strcpy(properties->shortLabel, "R in");
		return true;
	}
	return false;
}
bool BasePluginVST2::getOutputProperties (VstInt32 index, VstPinProperties* properties)
{
	if (index == 0 || index == 1)
	{
		properties->flags = kVstPinIsActive | kVstPinIsStereo;
	}
	if (index == 0)
	{
		strcpy(properties->label,	   "Left output");
		strcpy(properties->shortLabel, "L out");
		return true;
	}
	else if (index == 1)
	{
		strcpy(properties->label,	   "Right output");
		strcpy(properties->shortLabel, "R out");
		return true;
	}
	return false;
}

bool BasePluginVST2::getVendorString (char* text)
{
	vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
	return true;
}

void BasePluginVST2::open () {
#ifndef BUILD_BUILTIN_EFFECT
#ifdef _WIN32
	if (!isFirstPluginLoad) {
		return;
	}
	isFirstPluginLoad = false;
#endif
	//this is the first point where we have the right currentworkingdirectory set
	//no file io before this point!
	const int bufLen = 512*4;
	char* cwdbuf = _getcwd(NULL, bufLen);
	String strPath = "??";
	if (cwdbuf) {
		strPath = cwdbuf;
		free(cwdbuf);
		replaceString(strPath, "\\", "/");
		my_printf("getcwd: %s\n", StringAsCStr(strPath));
	}
	if (!FileExists("res")) { //TODO: make sure its a directory not a file
		String moduleName = getModuleName(hInstance);
		String modulePath = "";
		SplitPath(moduleName, &modulePath, nullptr, nullptr, nullptr);
		setCWDPath(modulePath);
		my_printf("setCWDPath: %s\n", StringAsCStr(moduleName));
	} else {
		setCWDPath(strPath);// remember cwd, it _will_ change
		my_printf("setCWDPath: %s\n", StringAsCStr(strPath));
	}
	isFirstPluginLoad = false;
	MouseCursors::initCursors(); //TODO: call MouseCursors::destroy() on exit of last instance


#endif

}
#ifndef BUILD_BUILTIN_EFFECT
static void glfw_plugin_error_callback(int error, const char* description) {
	char errorCodeStr[1024] = { 0 };
	_snprintf_s(errorCodeStr, 1024 - 1, _TRUNCATE, "Error %d: %s", error, description);
	my_printf("%s\n", errorCodeStr);
//	ngui::show(errorCodeStr, "Error", ngui::Style::Error, ngui::Buttons::OK);
}
static void showerror(const char* description) {
	ngui::show(description, "Error", ngui::Style::Error, ngui::Buttons::OK);
}
void initColor(); // gui/gui.cpp
void onModuleLoad() {
	isFirstPluginLoad = true;
	try {
	initColor();
	char pluginWindowClassName[32];
	sprintf_s(pluginWindowClassName, 32, "PLUGWND%I64X", (int64_t)&onModuleLoad);
	my_printf("window class name %s\n", pluginWindowClassName);
	glfwSetErrorCallback(glfw_plugin_error_callback);
	if (!glfwInit(pluginWindowClassName)) {
#ifdef _WIN32
		DWORD error = GetLastError();
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
#endif


