#include <algorithm>
#include "seq_util.h"
#include "logging.h"
#include "audioblock.h"
#include "snapshot.h"
#include "base_plugin.h"
#include "internal_plugin.h"
#include "track.h"
#include "track_impl.h"
#include "../vst_host.h"
#include "../vst_window.h"
#include "../../gui/pluginctr.h"
#include "../mainctrl.h"

#include "../../gui/guiplugin.h"
#include "../history.h"
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
//bool internalplugin::getNameString(char* szBuf) {
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
//		dbgassert(ap.ref);
//		ap.ref->onDstDelete();
//	}
//	if (this->window) {
//		this->window->destroy();
//	}
//	this->dispatch(effClose);
//	this->bIsSetup = false;
//	my_printf("UNLOAD %s\n", StringAsCStr(this->sName));
//}

namespace {

void createSnapshot(plugin_snapshot_t& ps, internalplugin* plugin, bool storePluginChunks) {
	ps.present = true;
	ps.slot = 0;
	ps.projectGlobalId = plugin->projectGlobalId;
	ps.enabled = plugin->bIsEnabled;
	ps.uId = plugin->uId;
	ps.pluginType = plugin->pluginType;
	ps.name = plugin->sName;
	if (storePluginChunks) {
		ps.params.reserve(plugin->getNumParameters());
		plugin->visitParams([&ps](auto& mapEntry) {
			automatable_param_t& param = mapEntry.second;
			if (param.inUse) {
				ps.params.push_back(param_snapshot_t{ param.idx, param.value, param.inUse?1:0 });
			}
		});
	}
	storeAutomation(ps.automatedParams, plugin);

}
}
void internalplugin::makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) {
//	createSnapshot(ps, this, storePluginChunks);
//	ps.slot = handle->slot;
	createSnapshot(ps, this, storePluginChunks);
	ps.slot = this->slot;
//	this->makePresetSnapshot(ps, this);
}
void internalplugin::loadSnapshot(const plugin_snapshot_t& ps)  {
	//dbgassert(ps.slot == this->slot);
}
String internalplugin::getAutomatableName() {
	return this->sName;
}
float internalplugin::getParamValue(int32_t idx) {
	automatable_param_t* param = getParamUnchecked(idx);
	dbgassert(param);
	if (param->internalIdx >= 0) {
		param->value = dispatchGetParameter(param->internalIdx);
	}
	return param->value;
}
void internalplugin::setParamValue(int32_t idx, float val, int flags) {
	automatable_param_t* param = getParamUnchecked(idx);
	dbgassert(param);
	param->value = val;
	if (param->idx == PARAM_ENABLE) {
		bool wasEnable = this->bIsEnabled;
		bool isEnabled = val > 0;
		updateOnEnableParam(param, wasEnable, isEnabled, flags);
	} else {
		if (!(flags&FLG_PAR_UPDATE_NOSTORE) && !(flags&FLG_PAR_UPDATE_AUTOMATED)) {
			param->inUse = true;
		}
		if (param->internalIdx >= 0) {
			dispatchSetParameter(param->internalIdx, val);
		}
	}
}
void internalplugin::postSetParameter(int32_t idx, float preVal, float val, int flags) {
	if (flags != 2) {
		return;
	}
	dbgassert(this->trackImpl->getTrack());
	track_t* track = this->trackImpl->getTrack();
	automationlane_snapshot_t ref = toRef();
	parameter_ref_t p = {track->projectIdx,  ref.type, this->projectGlobalId, idx};
	DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
}
void internalplugin::recvPluginEditParamUpdate(int32_t internalIdx) {
	automatable_param_t* param = getEffectParam(internalIdx);
	dbgassert(param && param->internalIdx >= 0);
	param->value = dispatchGetParameter(param->internalIdx);
}
automationlane_snapshot_t internalplugin::toRef() const {
	automationlane_snapshot_t ref;
	ref.type = AUTOMATABLE_EFFECT;
	ref.refId = this->projectGlobalId;
	return ref;
}
void internalplugin::onEnable() {
	resume();
}
void internalplugin::onDisable() {
	sleep();
//	vsthost::getInstance()->sendNotesOff(this);
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


struct internalplugin::internalplugin_handles_t {
	std::unique_ptr<guiinternalpluginview> gui;
};
internalplugin::internalplugin(String _sName, int32_t _pluginType, int32_t _projectGlobalId)
	: effectbase(_sName, _pluginType, _projectGlobalId),
	  handlesIntPlugin(new internalplugin_handles_t{})
{
}

internalplugin::~internalplugin() {
	delete handlesIntPlugin;
}
guiplugin* internalplugin::makeGui() {
	if (!handlesIntPlugin->gui) {
		handlesIntPlugin->gui = std::make_unique<guiinternalpluginview>(this);
		handlesIntPlugin->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
	}
	return handlesIntPlugin->gui.get();
//	return handle->gui;
}
guiplugin* internalplugin::getGui() {
	return handlesIntPlugin->gui.get();
//	return handle->gui;
}
