#include <algorithm>
#include "seq_util.h"
#include "logging.h"
#include "audioblock.h"
#include "snapshot.h"
#include "base_plugin.h"
#include "internal_plugin.h"
#include "../vst_host.h"
#include "../vst_window.h"
#include "../../gui/plugin.h"
#include "leak_detect.h"
/*
bool internalplugin::onResize(vst_window* window, Size size) {
	return true;
}
Size internalplugin::constrainSize(vst_window* window, Size& size) {
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

bool internalplugin::onClose() {
	if (this->window != NULL) {
		this->dispatch(effEditClose);
	}
	bEditOpen = false;
	return true;
}
void internalplugin::onWindowDestroy() {
	this->window = NULL;
}*/
//bool internalplugin::resume() {
//	bool wasSleep = !this->bIsEnabled;
//	this->dispatch(effMainsChanged, 0, true);
//	this->dispatch(effStartProcess);
//	this->bIsEnabled = true;
//	return wasSleep;
//}
//bool internalplugin::sleep() {
//	bool wasSleep = !this->bIsEnabled;
//	this->dispatch(effStopProcess);
//	this->dispatch(effMainsChanged, 0, false);
//	this->bIsEnabled = false;
//	return !wasSleep;
//}
//void internalplugin::printNames() {
//	char buf[256];
//	printf("Name: %s\n", StringAsCStr(sName));
//	if (this->dispatch(effGetVendorString, 0, 0, (void*)buf) && buf[0] != 0) {
//		printf("effGetVendorString: %s\n", buf);
//	}
//	if (this->dispatch(effGetProductString, 0, 0, (void*)buf) && buf[0] != 0) {
//		printf("effGetProductString: %s\n", buf);
//	}
//	if (this->dispatch(effGetEffectName, 0, 0, (void*)buf) && buf[0] != 0) {
//		printf("effGetEffectName: %s\n", buf);
//	}
//}
//bool internalplugin::getNameString(const char* szBuf) {
//	if (this->dispatch(effGetProductString, 0, 0, (void*)szBuf) && szBuf[0] != 0) {
//		return true;
//	}
//	if (this->dispatch(effGetEffectName, 0, 0, (void*)szBuf) && szBuf[0] != 0) {
//		return true;
//	}
//	return false;
//}
//bool internalplugin::updateWindowSize() {
//	if (this->window != NULL) {
//		ERect *prc = NULL;
//		this->dispatch(effEditGetRect, 0, 0, (void*)&prc);
//		if (prc)
//		{
//			this->window->resize({ prc->right - prc->left, prc->bottom - prc->top });
//			return true;
//		}
//	}
//	return false;
//}
//bool internalplugin::onShow(vst_window* window) {
//	if (this->window == window) {
//		bEditOpen = true;
//		this->dispatch(effEditOpen, 0, 0, (void*)window->getHWND());
//		updateWindowSize();
//		this->updateDisplay();
//	}
//	return true;
//}
//void internalplugin::unload() {
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
//	if (this->window) {
//		this->window->destroy();
//	}
//	this->dispatch(effClose);
//	this->bIsSetup = false;
//	my_printf("UNLOAD %s\n", StringAsCStr(this->sName));
//}

//void internalplugin::load(vsthost* host) {
//	if (this->bIsSetup) {
//		unload();
//	}
//	auto aeffect = handle->aeffect;
//	assert(aeffect->numOutputs > 0);
//	this->blockInputs = new AudioBlock(std::max(2, aeffect->numInputs), host->lBlockSize);
//	this->blockOutputs = new AudioBlock(std::max(2, aeffect->numOutputs), host->lBlockSize);
//	aeffect->resvd2 = 0;
//	this->vstVersion = dispatch(effGetVstVersion);
//	this->uId = aeffect->uniqueID;
//	this->dispatch(effIdentify, 0, 0, 0, 0);
//	this->dispatch(effSetSampleRate, 0, 0, NULL, (float) host->lSampleRate);
//	this->dispatch(effSetBlockSize, 0, host->lBlockSize, 0, 0);
//	this->dispatch(effOpen);
//
//	VstPinProperties pin;
//	for (int32_t i = 0; i < aeffect->numInputs; i++) {
//		if (this->dispatch(effGetInputProperties, i, 0, &pin)) {
//			inputNames.push_back(pin.label);
//		} else {
//			inputNames.push_back(StringFormat("Input %d", i));
//		}
//	}
//	for (int32_t i = 0; i < aeffect->numOutputs; i++) {
//		if (this->dispatch(effGetOutputProperties, i, 0, &pin)) {
//			inputNames.push_back(pin.label);
//		} else {
//			inputNames.push_back(StringFormat("Output %d", i));
//		}
//	}
//
//	this->pluginCategory = this->dispatch(effGetPlugCategory) > 0;
//	this->isSynth = (handle->aeffect->flags & effFlagsIsSynth) != 0;
//	this->bCanReceiveMidi = this->isSynth || this->dispatch(effCanDo, 0, 0, (void*)PlugCanDos::canDoReceiveVstMidiEvent) > 0;
//
//	VstParameterProperties properties{0};
//
//	char buf[1024];
//	vst_param_category fallbackCat={0, 0, "Parameters"};
//	for (int i = 0; i < aeffect->numParams; i++) {
//		vst_param param{0};
//		param.idx = i;
//		memset(buf, 0, sizeof(buf));
//		this->dispatch(effGetParamName, i, 0, buf);
//		String label = buf[0] ? buf : StringFormat("Parameter %d", i);
//		param.label = param.shortLabel = label;
//		if (this->dispatch(effGetParameterProperties, i, 0, &properties, 0)) {
//			param.flags = properties.flags | (ParamIsAdvanced);
//			param.label = properties.label;
//			param.shortLabel = properties.shortLabel;
//			if (properties.label[0]) {
//				param.label = properties.label;
//			}
//			if (properties.shortLabel[0]) {
//				param.shortLabel = properties.shortLabel;
//			}
//			if (param.flags & ParamUsesFloatStep) {
//				param.min.valFloat = 0;
//				param.max.valFloat = 0;
//				param.step.valFloat = properties.stepFloat;
//				param.stepSmall.valFloat = properties.smallStepFloat;
//				param.stepLarge.valFloat = properties.largeStepFloat;
//			}
//			if (param.flags & ParamUsesIntStep) {
//				param.min.valInt = std::numeric_limits<int32_t>::min();
//				param.max.valInt = std::numeric_limits<int32_t>::max();
//				param.step.valInt = properties.stepInteger;
//				param.stepSmall.valInt = 1;
//				param.stepLarge.valInt = properties.largeStepInteger;
//			}
//			if (param.flags & ParamUsesIntegerMinMax) {
//				param.min.valInt = properties.minInteger;
//				param.max.valInt = properties.maxInteger;
//			}
//			if (param.flags & ParamSupportsDisplayCategory) {
//				param.category = properties.category + 1;
//				if (getCategory(param.category) == 0 && properties.categoryLabel[0]) {
//					vst_param_category paramCat = { param.category, properties.numParametersInCategory, properties.categoryLabel };
//					paramsCategories.push_back(paramCat);
//				}
//			}
//			if (param.flags & ParamSupportsDisplayIndex) {
//				param.displayIndex = properties.displayIndex;
//			}
//		} else {
//			param.flags = 0;
//			fallbackCat.numParametersInCategory++;
//		}
//		param.value = handle->aeffect->getParameter(handle->aeffect, param.idx);
//		params.push_back(param);
//	}
//	paramsCategories.push_back(fallbackCat);
//
//
//
////	this->resume();
//	this->sleep();
//	this->dispatch(effSetBlockSize, 0, host->lBlockSize);
////	this->resume();
//
//
//	this->bIsSetup = true;
//}
//void createSnapshot(plugin_snapshot_t& ps, vstplugin* plugin, bool storePluginChunks) {
//	ps.present = true;
//	ps.slot = 0;
//	ps.projectGlobalId = plugin->projectGlobalId;
//	ps.enabled = plugin->bIsEnabled;
//	ps.uId = plugin->uId;
//	ps.name = plugin->sName;
//	if (storePluginChunks) {
//		void* pluginData;
//		int32_t pluginDataSize = plugin->dispatch(effGetChunk, 0, 0, &pluginData, 0);
//		if (pluginDataSize > 0 && pluginData) {
//			uint8_t* ptrData = reinterpret_cast<uint8_t*>(pluginData);
//			ps.dataChunk.reserve(pluginDataSize);
//			ps.dataChunk.assign(ptrData, ptrData + pluginDataSize);
//			my_printf("Plugin %s: Save data1[%d]\n", StringAsCStr(plugin->sName), pluginDataSize);
//
//		}
//		void* pluginData2;
//		int32_t pluginDataSize2 = plugin->dispatch(effGetChunk, 1, 0, &pluginData2, 0);
//		if (pluginDataSize2 > 0 && pluginData2) {
//			uint8_t* ptrData = reinterpret_cast<uint8_t*>(pluginData2);
//			ps.dataChunk2.reserve(pluginDataSize2);
//			ps.dataChunk2.assign(ptrData, ptrData + pluginDataSize2);
//			my_printf("Plugin %s: Save data2[%d]\n", StringAsCStr(plugin->sName), pluginDataSize2);
//		}
//	}
//	ps.params.reserve(plugin->params.size());
//	for (vst_param& param : plugin->params) {
//		float val = plugin->getParamValue(param.idx);
//		param_snapshot_t t{param.idx, val};
//		ps.params.push_back(t);
//	}
//	for (automated_param_t& automatedParam : plugin->automatedParams) {
//		if (automatedParam.src.points.empty()) {
//			continue;
//		}
//		automation_view_t automation;
//		automation.targetParam = automatedParam.paramIdx;
//		automation.points = automatedParam.src.points;
//		automation.active = automatedParam.src.active;
//		ps.automatedParams.push_back(automation);
//	}
//}
void internalplugin::makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) {
//	createSnapshot(ps, this, storePluginChunks);
//	ps.slot = handle->slot;
}
guibase* internalplugin::makeGui() {
//	if (!handle->gui) {
//		handle->gui = std::make_unique<guiplugin>(this);
//		handle->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
//	}
//	return handle->gui.get();
	return nullptr;
}
void internalplugin::setSlot(int32_t i) {
	slot = i;
}
int32_t internalplugin::getSlot() {
	return slot;
}
void internalplugin::breakTrackLink() {
	trackImpl = nullptr;
}
track_impl_t* internalplugin::getTrackLink() {
	return trackImpl;
}
void internalplugin::setTrackLink(track_impl_t* trImpl) {
	trackImpl = trImpl;
}
int32_t internalplugin::getNumParameters() {
	return params.size();
}
String internalplugin::getParamName(int32_t idx) {
	if (idx >= 0 && idx < (int32_t)params.size()) {
		return params[idx].label;
	}
	return "";
}
String internalplugin::getAutomatableName() {
	return this->sName;
}
float internalplugin::getParamValue(int32_t idx) {
	if (idx >= 0 && idx < (int32_t)params.size()) {
		auto& param = params[idx];
		param.value = dispatchGetParameter(param.idx);
		return param.value;
	}
	return 0;
}
void internalplugin::setParamValue(int32_t idx, float val) {
	if (idx >= 0 && idx < (int32_t)params.size()) {
		auto& param = params[idx];
		param.value = val;
		dispatchSetParameter(idx, val);
//		my_printf("set %s[%d] = %f\n", StringAsCStr(this->sName), idx, val);
	}
}
void internalplugin::recvPluginEditParamUpdate(int32_t idx) {
	if (idx >= 0 && idx < (int32_t)params.size()) {
		auto& param = params[idx];
		param.value = dispatchGetParameter(param.idx);
	}
}
automated_param_t* internalplugin::getRegisteredAutomation(int32_t idx) {
	auto it = std::find_if(automatedParams.begin(), automatedParams.end(), [idx](automated_param_t& ap) {
		return ap.paramIdx == idx;
	});
	if (it != automatedParams.end()) {
		automated_param_t* ap = &(*it);
		if (ap->src.isAutomated())
			return ap;
	}
	return NULL;
}
automationlane_snapshot_t internalplugin::toRef() {
	automationlane_snapshot_t ref;
	ref.type = 0;
	ref.refId = this->projectGlobalId;
	return ref;
}
void internalplugin::getAutomated(std::vector<int32_t>& targets) {
	for (automated_param_t t : automatedParams) {
		if (t.src.isAutomated())
			targets.push_back(t.paramIdx);
	}
}
void internalplugin::updateAutomatedParameters(tick_t pos) {
	for (automated_param_t& param : automatedParams) {
		if (param.src.isActive()) {
			float val = param.src.getValueAt(pos);
			setParamValue(param.paramIdx, val);
		}
	}
}
automation_t* internalplugin::getAutomation(int32_t paramIdx) {
	if (!hasParam(paramIdx)) {
		return NULL;
	}
	for (automated_param_t& param : automatedParams) {
		if (paramIdx == param.paramIdx) {
			return &param.src;
		}
	}
	automatedParams.emplace_back(paramIdx);
	return &automatedParams.back().src;
}
void internalplugin::deactivateAutomation(int32_t paramIdx) {
	for (automated_param_t& param : automatedParams) {
		if (paramIdx == param.paramIdx) {
			param.src.active = false;
			return;
		}
	}
}
bool internalplugin::hasParam(int idx) {
	if (idx >= 0 && idx < (int)params.size()) {
		return true;
	}
	return false;
}

bool internalplugin::close() {
//	if (this->window != NULL) {
//		this->window->close();
//	}
	return true;
}
bool internalplugin::show() {
//	if (this->window == NULL && (handle->aeffect->flags & effFlagsHasEditor)) {
//		ERect *prc = NULL;
//		this->dispatch(effEditGetRect, 0, 0, (void*)&prc);
//		Size size = { 160, 120 };
//		if (prc)
//		{
//			size = { prc->right - prc->left, prc->bottom - prc->top };
//		}
//		if (size.width <= 0) size.width = 160;
//		if (size.height <= 0) size.height = 120;
//		this->window = vst_window::make(this, this->sName, size, false);
//	}
//	if (this->window != NULL) {
//		this->window->show();
//	}
	return false;
}
