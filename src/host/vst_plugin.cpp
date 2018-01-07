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
//	for (automated_param_t& ap : this->automatedParams) {
//		assert(ap.ref);
//		ap.ref->onDstDelete();
//	}
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

	VstParameterProperties properties{0};

	char buf[1024];
	vst_param_category fallbackCat={0, 0, "Parameters"};
	for (int i = 0; i < aeffect->numParams; i++) {
		vst_param param{0};
		param.idx = i;
		memset(buf, 0, sizeof(buf));
		this->dispatch(effGetParamName, i, 0, buf);
		String label = buf[0] ? buf : StringFormat("Parameter %d", i);
		param.label = param.shortLabel = label;
		if (this->dispatch(effGetParameterProperties, i, 0, &properties, 0)) {
			param.flags = properties.flags | (ParamIsAdvanced);
			param.label = properties.label;
			param.shortLabel = properties.shortLabel;
			if (properties.label[0]) {
				param.label = properties.label;
			}
			if (properties.shortLabel[0]) {
				param.shortLabel = properties.shortLabel;
			}
			if (param.flags & ParamUsesFloatStep) {
				param.min.valFloat = 0;
				param.max.valFloat = 0;
				param.step.valFloat = properties.stepFloat;
				param.stepSmall.valFloat = properties.smallStepFloat;
				param.stepLarge.valFloat = properties.largeStepFloat;
			}
			if (param.flags & ParamUsesIntStep) {
				param.min.valInt = std::numeric_limits<int32_t>::min();
				param.max.valInt = std::numeric_limits<int32_t>::max();
				param.step.valInt = properties.stepInteger;
				param.stepSmall.valInt = 1;
				param.stepLarge.valInt = properties.largeStepInteger;
			}
			if (param.flags & ParamUsesIntegerMinMax) {
				param.min.valInt = properties.minInteger;
				param.max.valInt = properties.maxInteger;
			}
			if (param.flags & ParamSupportsDisplayCategory) {
				param.category = properties.category + 1;
				if (getCategory(param.category) == 0 && properties.categoryLabel[0]) {
					vst_param_category paramCat = { param.category, properties.numParametersInCategory, properties.categoryLabel };
					paramsCategories.push_back(paramCat);
				}
			}
			if (param.flags & ParamSupportsDisplayIndex) {
				param.displayIndex = properties.displayIndex;
			}
		} else {
			param.flags = 0;
			fallbackCat.numParametersInCategory++;
		}
		params.push_back(param);
	}
	paramsCategories.push_back(fallbackCat);



	this->resume();
	this->sleep();
	this->dispatch(effSetBlockSize, 0, host->lBlockSize);
	this->resume();
	this->bIsSetup = true;
}
vst_param_category* vstplugin::getCategory(int idx) {
	if (idx >= 0 && idx < paramsCategories.size()) {
		return &paramsCategories[idx];
	}
	return NULL;
}
int32_t vstplugin::getNumParameters() {
	return params.size();
}
String vstplugin::getParamName(int32_t idx) {
	if (idx >= 0 && idx < params.size()) {
		return params[idx].label;
	}
	return "";
}
String vstplugin::getAutomatableName() {
	return this->sName;
}
float vstplugin::getParamValue(int32_t idx) {
	if (idx >= 0 && idx < params.size()) {
		return handle->aeffect->getParameter(handle->aeffect, idx);
	}
	return 0;
}
void vstplugin::setParamValue(int32_t idx, float val) {
	if (idx >= 0 && idx < params.size()) {
		handle->aeffect->setParameter(handle->aeffect, idx, val);
	}
}
automated_param_t* vstplugin::getRegisteredAutomation(int32_t idx) {
	auto it = std::find_if(automatedParams.begin(), automatedParams.end(), [idx](automated_param_t& ap) {
		return ap.paramIdx == idx;
	});
	if (it != automatedParams.end()) {
		return &(*it);
	}
	return NULL;
}
automationlane_snapshot_t vstplugin::toRef() {
	automationlane_snapshot_t ref;
	ref.type = 0;
	ref.refId = this->projectGlobalId;
	return ref;
}
void vstplugin::getAutomated(std::vector<int32_t>& targets) {
	for (automated_param_t t : automatedParams) {
		targets.push_back(t.paramIdx);
	}
}
void vstplugin::updateAutomatedParameters(tick_t pos) {
	for (automated_param_t& param : automatedParams) {
		float val = param.src->getValueAt(pos);
		setParamValue(param.paramIdx, val);
	}
}
automation_t* vstplugin::getAutomation(int32_t paramIdx) {
	if (!getParam(paramIdx)) {
		return NULL;
	}
	for (automated_param_t& param : automatedParams) {
		if (paramIdx == param.paramIdx) {
			return param.src;
		}
	}
	vstparam_automation_t* param = new vstparam_automation_t();
	param->plugin = this;
	param->paramIdx = paramIdx;
	param->dummy = 0.5f;
	automatedParams.push_back({paramIdx, param});
	return param;
}
vst_param* vstplugin::getParam(int idx) {
	if (idx >= 0 && idx < params.size()) {
		return &params[idx];
	}
	return NULL;
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
