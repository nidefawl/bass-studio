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
#include "msgbox.h"


#ifndef BUILD_BUILTIN_EFFECT
extern HMODULE hInstance;
String getModuleName(HMODULE module); //platform_win.cpp
#endif

AEffEditor* createPluginWindow(AudioEffect *_effect, std::shared_ptr<PluginControl> _ctrl, int w, int h);
namespace MouseCursors {
void init(); // mousecursor.cpp
}

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
	MouseCursors::init(); //TODO: call MouseCursors::destroy() on exit of last instance
#endif

}


