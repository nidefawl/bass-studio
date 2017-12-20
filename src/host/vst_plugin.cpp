#include "vst_plugin.h"
#include "vst_host.h"
#include "vst_window.h"
#include "seq_util.h"
#include "vst_plugin_handles.h"
#include "logging.h"
#include "audioblock.h"
#include "../gui/plugin.h"
#include <algorithm>

const char* plug_features_array[] = {
	PlugCanDos::canDoSendVstEvents,
	PlugCanDos::canDoSendVstMidiEvent,
	PlugCanDos::canDoReceiveVstEvents,
	PlugCanDos::canDoReceiveVstMidiEvent,
	PlugCanDos::canDoReceiveVstTimeInfo,
	PlugCanDos::canDoOffline,
	PlugCanDos::canDoMidiProgramNames,
	PlugCanDos::canDoBypass,
};
long vstplugin::dispatch(
	long opcode,
	long index,
	long value,
	void *ptr,
	float opt) {
	return handle->aeffect->dispatcher(handle->aeffect, opcode, index, value, ptr, opt);
}

bool vstplugin::onResize(vst_window* window, Size size) {
	return true;
}
Size vstplugin::constrainSize(vst_window* window, Size& size) {
	ERect *prc = NULL;
	this->dispatch(effEditGetRect, 0, 0, (void*)&prc);
	if (prc)
	{
		if (size.width > (prc->right - prc->left)) {
			size.width = prc->right - prc->left;
		}
		if (size.height > (prc->bottom - prc->top)) {
			size.height = prc->bottom - prc->top;
		}
	}
	return size;
}

bool vstplugin::onClose() {
	if (this->window != NULL) {
		this->dispatch(effEditClose);
	}
	bEditOpen = false;
	this->window = NULL;
	return true;
}
bool vstplugin::resume() {
	bool wasSleep = !this->bIsEnabled;
	this->dispatch(effMainsChanged, 0, true);
	this->bIsEnabled = true;
	return wasSleep;
}
bool vstplugin::sleep() {
	bool wasSleep = !this->bIsEnabled;
	this->dispatch(effMainsChanged, 0, false);
	this->bIsEnabled = false;
	return !wasSleep;
}
void vstplugin::printNames() {
	char buf[256];
	printf("Name: %s\n", StringAsCStr(sName));
	if (this->dispatch(effGetVendorString, 0, 0, (void*)buf) && buf[0] != 0) {
		printf("effGetVendorString: %s\n", buf);
	}
	if (this->dispatch(effGetProductString, 0, 0, (void*)buf) && buf[0] != 0) {
		printf("effGetProductString: %s\n", buf);
	}
	if (this->dispatch(effGetEffectName, 0, 0, (void*)buf) && buf[0] != 0) {
		printf("effGetEffectName: %s\n", buf);
	}
}
bool vstplugin::getNameString(const char* szBuf) {
	if (this->dispatch(effGetProductString, 0, 0, (void*)szBuf) && szBuf[0] != 0) {
		return true;
	}
	if (this->dispatch(effGetEffectName, 0, 0, (void*)szBuf) && szBuf[0] != 0) {
		return true;
	}
	return false;
}
bool vstplugin::onShow(vst_window* window) {
	this->window = window;
	if (this->window != NULL) {
		bEditOpen = true;
		this->dispatch(effEditOpen, 0, 0, this->window->getHWND());

		ERect *prc = NULL;
		this->dispatch(effEditGetRect, 0, 0, (void*)&prc);
		if (prc)
		{
			this->window->resize({ prc->right - prc->left, prc->bottom - prc->top });
		}
		this->updateDisplay();
	}
	return true;
}
void vstplugin::unload() {
//	if (handle->aeffect != NULL) {
//		float** pluginBufIn = blockInputs->buf;
//		float** pluginBufOut = blockOutputs->buf;
//		if (handle->aeffect->flags & effFlagsCanReplacing) {
//				handle->aeffect->processReplacing(handle->aeffect, pluginBufIn, pluginBufOut, blockInputs->samples);
//		} else {
//			handle->aeffect->process(handle->aeffect, pluginBufIn, pluginBufOut, blockInputs->samples);
//		}
//	}
//	this->sleep();
//	if (this->bEditOpen) {
//
//		this->dispatch(effEditIdle);
//		this->dispatch(effEditSleep);
//		this->dispatch(effEditClose);
//	}
//	this->dispatch(effMainsChanged, 0, 0);
//	this->dispatch(effSetBypass, 0, 1);
	this->dispatch(effClose);
	this->bIsSetup = false;
	my_printf("UNLOAD %s\n", StringAsCStr(this->sName));
}

void vstplugin::load(vsthost* host) {
	if (this->bIsSetup) {
		unload();
	}
	auto aeffect = handle->aeffect;
	assert(aeffect->numOutputs > 0);
	this->blockInputs = new AudioBlock(std::max(2, aeffect->numInputs), host->lBlockSize);
	this->blockOutputs = new AudioBlock(std::max(2, aeffect->numOutputs), host->lBlockSize);
	aeffect->resvd2 = 0;
	this->vstVersion = dispatch(effGetVstVersion);
	this->uId = aeffect->uniqueID;
	this->dispatch(effIdentify, 0, 0, 0, 0);
	this->dispatch(effSetSampleRate, 0, 0, NULL, (float) host->lSampleRate);
	this->dispatch(effSetBlockSize, 0, host->lBlockSize, 0, 0);
	this->dispatch(effOpen);

	VstPinProperties pin;
	for (int32_t i = 0; i < aeffect->numInputs; i++) {
		if (this->dispatch(effGetInputProperties, i, 0, &pin)) {
			inputNames.push_back(pin.label);
		} else {
			inputNames.push_back(StringFormat("Input %d", i));
		}
	}
	for (int32_t i = 0; i < aeffect->numOutputs; i++) {
		if (this->dispatch(effGetOutputProperties, i, 0, &pin)) {
			inputNames.push_back(pin.label);
		} else {
			inputNames.push_back(StringFormat("Output %d", i));
		}
	}

	this->pluginCategory = this->dispatch(effGetPlugCategory) > 0;
	this->isSynth = (handle->aeffect->flags & effFlagsIsSynth) != 0;
	this->bCanReceiveMidi = this->isSynth || this->dispatch(effCanDo, 0, 0, (void*)PlugCanDos::canDoReceiveVstMidiEvent) > 0;
	this->resume();
	this->sleep();
	this->dispatch(effSetBlockSize, 0, host->lBlockSize);
	this->resume();
	this->bIsSetup = true;
}

bool vstplugin::close() {
	if (this->window != NULL) {
		this->window->close();
	}
	return true;
}
bool vstplugin::show() {
	if (this->window != NULL) {
		this->window->close();
	}
	if (handle->aeffect->flags & effFlagsHasEditor) {
		ERect *prc = NULL;
		this->dispatch(effEditGetRect, 0, 0, (void*)&prc);
		Size size = { 0, 0 };
		if (prc)
		{
			size = { prc->right - prc->left, prc->bottom - prc->top };
		}
		this->window = vst_window::make(this, this->sName, size, false, GetModuleHandle(NULL));
		this->window->show();
	}
	return false;
}
String vstplugin::getInfo() {
	String out;

	char szBuf[256] = "";
	const char *sep = "\n";

	out += StringFormat("Filename %s", StringAsCStr(this->sName));
	out += sep;
	if (this->getNameString(szBuf)) {
		out += StringFormat("Name %s", szBuf);
		out += sep;
	}
	out += StringFormat("Dir %s", StringAsCStr(this->sDir));
	out += sep;

	AEffect* handle = this->handle->aeffect;
	out += StringFormat("%d programs", handle->numPrograms);
	out += sep;
	out += StringFormat("%d parameters", handle->numParams);
	out += sep;
	out += StringFormat("%d inputs", handle->numInputs);
	out += sep;
	out += StringFormat("%d outputs", handle->numOutputs);
	out += sep;
	out += StringFormat("Flags: %08lXH", handle->flags);


	if (handle->flags & effFlagsHasEditor)
		out += "Has Editor\n";
	if (handle->flags & effFlagsCanReplacing)
		out += "Supports in place output\n";
	if (handle->flags & effFlagsProgramChunks)
		out += "Program data are handled in formatless chunks\n";
	if (handle->flags & effFlagsIsSynth)
		out += "Is a synth\n";
	if (handle->flags & effFlagsNoSoundInStop)
		out += "Does not produce sound when input is all silence\n";
	if (handle->flags & effFlagsCanDoubleReplacing)
		out += "Supports in place double-precision output\n";

	if (handle->initialDelay)
	{
		out += StringFormat("Initial Delay: %d", handle->initialDelay);
		out += sep;
	}

	char sUID[5];
	int i;
	for (i = 0; i < 4; i++)
	{
		sUID[i] = ((char *)&handle->uniqueID)[3 - i];
		if (!sUID[i])
			sUID[i] = ' ';
	}
	sUID[i] = '\0';
	out += StringFormat("Unique ID: '%s' (%08lXH)", sUID, handle->uniqueID);
	out += sep;
	out += StringFormat("Version %d", handle->version);
	out += sep;


#define ARR_SIZE(x) (sizeof(x)/sizeof(x[0]))
	for (size_t j = 0; j < ARR_SIZE(plug_features_array); j++)
	{
		if (this->dispatch(effCanDo, 0, 0, (void *)plug_features_array[j])) {
			out += StringFormat("Supports %s", plug_features_array[j]);
			out += sep;
		}
	}

	return out;
}

handles_t::~handles_t() {
	hmodule = NULL; // we no longer own
}
