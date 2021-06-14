#include <algorithm>
#include "vst_plugin.h"

#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "logging.h"
#include "audioblock.h"
#include "snapshot.h"
#include "base_plugin.h"
#include "vst_plugin_handles.h"
#include "track.h"
#include "track_impl.h"
#include "../vst_host.h"
#include "../vst_window.h"
#include "../../gui/gui.h"
#include "../../gui/guiplugin.h"
#include "../../gui/pluginctr.h"
#include "../../gui/pluginviewcontainers.h"
#include "plugins/plugin-base.h"
#include "../mainctrl.h"
#include "../history.h"


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
float vst_getParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx);
void vst_setParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx, float value);

bool vstplugin::onResize(vst_window* window, ivec2 size) {
	return true;
}
bool vstplugin::updateWindow() {
	if (this->window != NULL) {
		this->window->updateWindow();
		return true;
	}
	return false;
}
ivec2 vstplugin::constrainSize(vst_window* window, ivec2& size) {
	ERect *prc = NULL;
	this->dispatch(effEditGetRect, 0, 0, (void*)&prc);
	if (prc)
	{
		if (size.x > (prc->right - prc->left)) {
			size.x = prc->right - prc->left;
		}
		if (size.y > (prc->bottom - prc->top)) {
			size.y = prc->bottom - prc->top;
		}
	}
	return size;
}

bool vstplugin::onClose() {
	if (this->window != NULL && bEditOpen) {
		this->dispatch(effEditClose);
	}
	bEditOpen = false;
	return true;
}
void vstplugin::onWindowDestroy() {
	this->window = NULL;
}
void vstplugin::resume() {
	if (!isInSuspend) {
		return;
	}
	isInSuspend = false;
	this->dispatch(effMainsChanged, 0, true);
	this->dispatch(effStartProcess);
}
void vstplugin::sleep() {
	if (isInSuspend) {
		return;
	}
	isInSuspend = true;
	this->dispatch(effStopProcess);
	this->dispatch(effMainsChanged, 0, false);
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
bool vstplugin::getNameString(char* szBuf) {
	if (this->dispatch(effGetProductString, 0, 0, (void*)szBuf) && szBuf[0] != 0) {
		return true;
	}
	if (this->dispatch(effGetEffectName, 0, 0, (void*)szBuf) && szBuf[0] != 0) {
		return true;
	}
	return false;
}
bool vstplugin::updateWindowSize() {
	if (this->window != NULL) {
		ERect *prc = NULL;
		this->dispatch(effEditGetRect, 0, 0, (void*)&prc);
		if (prc)
		{
			this->window->resize({ prc->right - prc->left, prc->bottom - prc->top });
			return true;
		}
	}
	return false;
}

bool vstplugin::onShow(vst_window* window) {
	if (this->window == window) {
		bEditOpen = true;
		this->dispatch(effEditOpen, 0, 0, (void*)window->getHWND());
		updateWindowSize();
		this->updateWindow();
	}
	return true;
}
void AppWndProc_disableBlockReentrant();
void AppWndProc_enableBlockReentrant();
void vstplugin::unload(vsthost* host, int flags) {
	effectbase::unload(host, flags);
	dbgassert(this->bIsSetup);
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
//		dbgassert(ap.ref);
//		ap.ref->onDstDelete();
//	}
	AppWndProc_enableBlockReentrant();
	this->dispatch(effClose);
	AppWndProc_disableBlockReentrant();
	if (this->window) {
		this->window->destroy();
	}
	this->bIsSetup = false;
	my_printf("UNLOAD %s\n", StringAsCStr(this->sName));
}

void vstplugin::load(vsthost* host) {
	effectbase::load(host);
	dbgassert(!this->bIsSetup);
	auto aeffect = handle->aeffect;
	dbgassert(aeffect->numOutputs > 0);
	this->blockInputs = new AudioBlock(math::max(2, aeffect->numInputs), format.blockSize);
	this->blockOutputs = new AudioBlock(math::max(2, aeffect->numOutputs), format.blockSize);
	aeffect->resvd2 = 0;
	this->vstVersion = dispatch(effGetVstVersion);
	this->uId = aeffect->uniqueID;

	long l0 = this->dispatch(effIdentify, 0, 0, 0, 0);
	long l5 = this->dispatch(effOpen);
	long l1 = this->dispatch(effStopProcess);
	long l2 = this->dispatch(effMainsChanged, 0, false);

	long l3 = this->dispatch(effSetSampleRate, 0, 0, NULL, (float)format.sampleRate);
	long l4 = this->dispatch(effSetBlockSize, 0, format.blockSize, 0, 0);
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
			outputNames.push_back(pin.label);
		} else {
			outputNames.push_back(StringFormat("Output %d", i));
		}
	}

	this->pluginCategory = this->dispatch(effGetPlugCategory);
	this->isSynth = (handle->aeffect->flags & effFlagsIsSynth) != 0;
	this->bCanReceiveMidi = this->isSynth || this->dispatch(effCanDo, 0, 0, (void*)PlugCanDos::canDoReceiveVstMidiEvent) > 0;
//    char szBuf[256] = "";
//    szBuf[0] = 0;
//    if (this->dispatch(effGetEffectName, 0, 0, (void*)szBuf) && szBuf[0] != 0) {
//        setProductName(szBuf);
//    }
//    else {
//        szBuf[0] = 0;
//        if (this->dispatch(effGetProductString, 0, 0, (void*)szBuf) && szBuf[0] != 0) {
//            setProductName(szBuf);
//        }
//    }
	VstParameterProperties properties = {};

	char buf[1024];
	vst_param_category fallbackCat={0, 0, "Parameters"};
	for (int i = 0; i < aeffect->numParams; i++) {
		int32_t paramIdentifier = PARAM_OFFSET_EXTERNAL + i;
		automatable_param_t* param = registerParam(paramIdentifier);
		param->internalIdx = i;
		memset(buf, 0, sizeof(buf));
		this->dispatch(effGetParamName, i, 0, buf);
		String label = buf[0] ? buf : StringFormat("Parameter %d", i);
		param->label = param->shortLabel = label;
		if (this->dispatch(effGetParameterProperties, i, 0, &properties, 0)) {
			param->flags = properties.flags | (ParamIsAdvanced);
			param->label = properties.label;
			param->shortLabel = properties.shortLabel;
			if (properties.label[0]) {
				param->label = properties.label;
			}
			if (properties.shortLabel[0]) {
				param->shortLabel = properties.shortLabel;
			}
			if (param->flags & ParamUsesFloatStep) {
//				param.min.valFloat = 0.0f;
//				param.max.valFloat = 1.0f;
				param->step.valFloat = properties.stepFloat;
				param->stepSmall.valFloat = properties.smallStepFloat;
				param->stepLarge.valFloat = properties.largeStepFloat;
			}
			if (param->flags & ParamUsesIntStep) {
				param->min.valInt = std::numeric_limits<int32_t>::min();
				param->max.valInt = std::numeric_limits<int32_t>::max();
				param->step.valInt = properties.stepInteger;
				param->stepSmall.valInt = 1;
				param->stepLarge.valInt = properties.largeStepInteger;
			}
			if (param->flags & ParamUsesIntegerMinMax) {
				param->min.valInt = properties.minInteger;
				param->max.valInt = properties.maxInteger;
			}
			if (param->flags & ParamSupportsDisplayCategory) {
				param->category = properties.category + 1;
				if (getCategory(param->category) == 0 && properties.categoryLabel[0]) {
					vst_param_category paramCat = { param->category, properties.numParametersInCategory, properties.categoryLabel };
					paramsCategories.push_back(paramCat);
				}
			}
			if (param->flags & ParamSupportsDisplayIndex) {
				param->displayIndex = properties.displayIndex;
			}
		} else {
			param->flags = 0;
			fallbackCat.numParametersInCategory++;
		}
		//TODO: wrap getParameter call in exception handler
		param->value = handle->aeffect->getParameter(handle->aeffect, param->internalIdx);
	}
	paramsCategories.push_back(fallbackCat);


	for (int i = 0; i < aeffect->numPrograms; i++) {
		memset(buf, 0, sizeof(buf));
		if (this->dispatch(effGetProgramNameIndexed, i, 0, &buf, 0)) {
			buf[sizeof(buf)-1]=0;
			this->programNames.push_back(buf);
		}
	}


	bIsEnabled = this->getParamValue(PARAM_ENABLE) > 0.5;
//	if (bIsEnabled) {
//		this->resume();
//	} else {
//		this->sleep();
//	}
//	this->dispatch(effSetBlockSize, 0, format.blockSize);
//	this->resume();


	this->bIsSetup = true;
    if (aeffect->numParams)
		getRegisteredAutomation(65536);
}

namespace {

void createSnapshot(plugin_snapshot_t& ps, vstplugin* plugin, bool storePluginChunks) {
	ps.present = true;
	ps.slot = 0;
	ps.projectGlobalId = plugin->projectGlobalId;
	ps.enabled = plugin->bIsEnabled;
	if (plugin->internalModuleId >= 0) {
		ps.pluginType = PLUGIN_TYPE_INTERNAL_EFFECT;
		ps.uId = plugin->internalModuleId;
	} else {
		ps.pluginType = PLUGIN_TYPE_VST;
		ps.uId = plugin->uId;
	}
	ps.name = plugin->sName;

	bool usesBinaryChunks = plugin->getFlagsVST() & effFlagsProgramChunks;
	if (storePluginChunks && (usesBinaryChunks)) {
		{
			void* pluginData = nullptr;
			int32_t pluginDataSize = plugin->dispatch(effGetChunk, 0, 0, &pluginData, 0);
			if (pluginDataSize > 0 && pluginData) {
				uint8_t* ptrData = reinterpret_cast<uint8_t*>(pluginData);
				ps.dataChunk.reserve(pluginDataSize);
				ps.dataChunk.assign(ptrData, ptrData + pluginDataSize);
				my_printf("Plugin %s: Save data1[%d]\n", StringAsCStr(plugin->sName), pluginDataSize);

			}
		}
		if (storePluginPresetWithSnapshot) {
			void* pluginData2 = nullptr;
			int32_t pluginDataSize2 = plugin->dispatch(effGetChunk, 1, 0, &pluginData2, 0);
			if (pluginDataSize2 > 0 && pluginData2) {
				uint8_t* ptrData = reinterpret_cast<uint8_t*>(pluginData2);
				ps.dataChunk2.reserve(pluginDataSize2);
				ps.dataChunk2.assign(ptrData, ptrData + pluginDataSize2);
				my_printf("Plugin %s: Save data2[%d]\n", StringAsCStr(plugin->sName), pluginDataSize2);
			}
		}
	}
	if (storePluginChunks) {
		int32_t numParamsReserve = math::min<int32_t>(150, plugin->getNumParameters());
		ps.params.reserve(numParamsReserve);
        plugin->visitParams([&ps, usesBinaryChunks](auto& mapEntry) {

                automatable_param_t& param = mapEntry.second;
			if (param.internalIdx < 0 || param.internalIdx < 128) {
    			if (param.inUse || !usesBinaryChunks) {
    				int paramFlags = param.inUse?1:0;
    				ps.params.push_back(param_snapshot_t{ param.idx, param.value, paramFlags });
                } else {
                }
        	}
		});
		storeAutomation(ps.automatedParams, plugin);
	}
	if (plugin->programNames.size()>1) {
		uint32_t curProgramNr = 0;
		plugin->getCurrentProgram(curProgramNr);
		ps.currentProgram = curProgramNr;
	}
}
}
void vstplugin::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {
	if (pluginSnapshot.currentProgram != -1 && pluginSnapshot.currentProgram < programNames.size()) {
		setCurrentProgram(pluginSnapshot.currentProgram);
	}
	if ((this->getFlagsVST() & effFlagsProgramChunks) != 0) {
		if (pluginSnapshot.dataChunk.size() > 0) {
			auto& localMem = this->handle->dataChunkLocalMemory;
			localMem = pluginSnapshot.dataChunk;
			my_printf("Plugin %s: Load data1[%d]\n", StringAsCStr(this->sName), localMem.size());
//			localMem.resize(localMem.size()*4);
			this->dispatch(effSetChunk, 0, localMem.size(), (void*)localMem.data());
		}
		if (loadPluginPresetWithSnapshot && pluginSnapshot.dataChunk2.size() > 0) {
			my_printf("Plugin %s: Load data2[%d]\n", StringAsCStr(this->sName), pluginSnapshot.dataChunk2.size());
			this->dispatch(effSetChunk, 1, pluginSnapshot.dataChunk2.size(), (void*)pluginSnapshot.dataChunk2.data());
		}
	}
}
void vstplugin::makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) {
	createSnapshot(ps, this, storePluginChunks);
	ps.slot = this->slot;
}
guiplugin* vstplugin::makeGui() {

	dbgassert(handle->hmodule || handle->axEffect);
	if (!handle->gui) {
		handle->gui = std::make_unique<guivstplugin>(this);
		handle->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
		if (handle->axEffect) { //only provided by internal vst2 instance (not a DLL)
			guiplugin* pGuiPlugin = handle->gui.get();
			guivstplugin* pGuiVstPlugin = dynamic_cast<guivstplugin*>(pGuiPlugin);
			dbgassert(pGuiVstPlugin);
			BasePluginVST2* baseVst2 = dynamic_cast<BasePluginVST2*>(handle->axEffect);
			dbgassert(baseVst2);
			auto viewCtr = baseVst2->createView();
			if (viewCtr) {
				pGuiVstPlugin->viewCtr = viewCtr;
				viewCtr->addTo(pGuiVstPlugin->viewCtrs);
				viewCtr->onGuiOpen(handle->axEffect);
				viewCtr->setVSTPlugin(this);
			}
		}
	}

	return handle->gui.get();
}

vstplugin::~vstplugin() {
	if (blockInputs) {
		delete blockInputs;
	}
	if (blockOutputs) {
		delete blockOutputs;
	}
	delete handle;
}

guiplugin* vstplugin::getGui() {
	return handle->gui.get();
}
int32_t vstplugin::getDelay() {
	return handle && handle->aeffect ? handle->aeffect->initialDelay : 0;
}
int32_t vstplugin::getFlagsVST() {
	return handle && handle->aeffect ? handle->aeffect->flags : 0;
}
VstTimeInfo* vstplugin::getLocalTimeInfoPtr() {
	return handle ? &handle->localTimeInfo : nullptr;
}
int32_t vstplugin::getLocalCurrentUniqueId() {
	return handle ? handle->localCurrentUniqueId : 0;
}

vst_param_category* vstplugin::getCategory(int idx) {
	if (idx >= 0 && idx < (int)paramsCategories.size()) {
		return &paramsCategories[idx];
	}
	return NULL;
}
String vstplugin::getAutomatableName() {
	return this->sName;
}
float vstplugin::getParamValue(int32_t idx) {
	automatable_param_t* param = getParam(idx);
	dbgassert(param);
	if (param->internalIdx >= 0) {
		param->value = vst_getParameter(this, handle->aeffect, param->internalIdx);
	}
	return param->value;
}
String vstplugin::getParamValueDisplay(int32_t idx) {
	automatable_param_t* param = getParam(idx);
	dbgassert(param);
	if (param->internalIdx >= 0) {
		char buf[1024];
		memset(buf, 0, sizeof(buf));
		this->dispatch(effGetParamDisplay, param->internalIdx, 0, buf);
		return StringFormat("%s", buf);
	}
	return effectbase::getParamValueDisplay(idx);
}
void vstplugin::setParamValue(int32_t idx, float val, int flags) {
	automatable_param_t* param = getParam(idx);
	dbgassert(param);
	param->value = val;
	if (param->idx == PARAM_ENABLE) {
		updateOnEnableParam(param, this->bIsEnabled, val > 0, flags);
	} else {
		if (!(flags&FLG_PAR_UPDATE_NOSTORE) && !(flags&FLG_PAR_UPDATE_AUTOMATED)) {
			param->inUse = true;
		}
		if (param->internalIdx >= 0) {
			vst_setParameter(this, handle->aeffect, param->internalIdx, val);
		}
	}
}
void vstplugin::postSetParameter(int32_t idx, float preVal, float val, int flags) {
	if (!(flags&FLG_PAR_UPDATE_USER)) {
		return;
	}
	dbgassert(MainCtrl::get()); // this code path is called by user edit, which is only supported on MainCtrl as of now (2020-02-09)
	dbgassert(this->trackImpl->getTrack());
	track_t* track = this->trackImpl->getTrack();
	automationlane_snapshot_t ref = toRef();
	parameter_ref_t p = {track->projectIdx,  ref.type, this->projectGlobalId, idx};
	DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
}


bool vstplugin::setCurrentProgram(uint32_t idx) {
	if (idx < this->programNames.size()) {
		return dispatch(effSetProgram, 0, idx, 0, 0) > 0;
	}
	return false;
}
bool vstplugin::getCurrentProgram(uint32_t& idx) {
	long curProgram = dispatch(effGetProgram, 0, 0, 0, 0);
	dbgassert(curProgram >= 0);
	idx = (uint32_t)curProgram;
	return true;
}
bool vstplugin::getNumberOfPrograms(uint32_t& numPrograms) {
    numPrograms = this->programNames.size();
	return true;
}
bool vstplugin::getCurrentProgramName(String& out) {
	char buf[1024];
	memset(buf, 0, sizeof(buf));
	dispatch(effGetProgramName, 0, 0, buf, 0);
	if (buf[0]) {
		out = String(buf);
		return true;
	}
	long curProgram = dispatch(effGetProgram, 0, 0, 0, 0);
	if (curProgram >= 0 && curProgram < programNames.size()) {
		out = programNames[curProgram];
		return true;
	}
	return "";
}

void vstplugin::recvPluginEditParamUpdate(int32_t internalIdx) {
	automatable_param_t* param = getEffectParam(internalIdx);
	dbgassert(param && param->internalIdx >= 0);
	param->value = vst_getParameter(this, handle->aeffect, param->internalIdx);
	param->inUse = true;
}
automationlane_snapshot_t vstplugin::toRef() {
	automationlane_snapshot_t ref;
	ref.type = AUTOMATABLE_EFFECT;
	ref.refId = this->projectGlobalId;
	return ref;
}

void vstplugin::onEnable() {
	//TODO: check current thread, check if playthread is locked
	resume();
}
void vstplugin::onDisable() {
	//TODO: check current thread, check if playthread is locked
	sleep();
	vsthost::getInstance()->sendNotesOff(this);
}
bool vstplugin::close() {
	if (this->window != NULL) {
		this->window->close();
	}
	return true;
}
bool vstplugin::show() {
	if (this->window == NULL && (handle->aeffect->flags & effFlagsHasEditor)) {
		ERect *prc = NULL;
		this->dispatch(effEditGetRect, 0, 0, (void*)&prc);
		ivec2 size = { 160, 120 };
		if (prc)
		{
			size = { prc->right - prc->left, prc->bottom - prc->top };
		}
		if (size.x <= 0) size.x = 160;
		if (size.y <= 0) size.y = 120;
		this->window = vst_window::make(this, this->sName, size, false);
	}
	if (this->window != NULL) {
		this->window->show();
	}
	return false;
}
String vstplugin::getInfo(std::vector<String>& list) {
	String out;

	char szBuf[256] = "";

	list.push_back(StringFormat("Filename %s", StringAsCStr(this->sName)));

	if (this->getNameString(szBuf)) {
		list.push_back(StringFormat("Name %s", szBuf));

	}
	list.push_back(StringFormat("Dir %s", StringAsCStr(this->sDir)));

	AEffect* handle = this->handle->aeffect;
	char sUID[5];
	int i;
	for (i = 0; i < 4; i++)
	{
		sUID[i] = ((char *)&handle->uniqueID)[3 - i];
		if (!sUID[i])
			sUID[i] = ' ';
	}
	sUID[i] = '\0';
	list.push_back(StringFormat("VstID: '%s' (%08lXH)", sUID, handle->uniqueID));
	list.push_back(StringFormat("Version %d", handle->version));
	list.push_back(StringFormat("initialDelay: %d", handle->initialDelay));

	list.push_back(StringFormat("%d outputs", handle->numOutputs));
	list.push_back(StringFormat("%d inputs", handle->numInputs));
	list.push_back(StringFormat("%d programs", handle->numPrograms));
	list.push_back(StringFormat("%d parameters", handle->numParams));

	list.push_back(StringFormat("Flags: %08lXH", handle->flags));
	if (handle->flags & effFlagsNoSoundInStop)
		out += "effFlagsNoSoundInStop\n";
	if (handle->flags & effFlagsIsSynth)
		out += "effFlagsIsSynth\n";
	if (handle->flags & effFlagsProgramChunks)
		out += "effFlagsProgramChunks\n";
	if (handle->flags & effFlagsCanReplacing)
		out += "effFlagsCanReplacing\n";
	if (handle->flags & effFlagsHasEditor)
		out += "effFlagsHasEditor\n";
	if (handle->flags & effFlagsCanDoubleReplacing)
		out += "effFlagsCanDoubleReplacing\n";





#define ARR_SIZE(x) (sizeof(x)/sizeof(x[0]))
	for (size_t j = 0; j < ARR_SIZE(plug_features_array); j++)
	{
		if (this->dispatch(effCanDo, 0, 0, (void *)plug_features_array[j])) {
			list.push_back(StringFormat("effCanDo %s", plug_features_array[j]));

		}
	}

	return out;
}

handles_t::~handles_t() {
	hmodule = NULL; // we no longer own
}
