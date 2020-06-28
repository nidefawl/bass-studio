#ifdef __APPLE__
#endif
#include <stdint.h>
#include <stdbool.h>
#include "math/vec.h"
#include "str_util.h"
#include "seq_time.h"

#include "track.h"
#include "track_impl.h"
#include "au_plugin.h"

auplugin::~auplugin(){ }
void auplugin::resume(){ }
void auplugin::sleep(){ }

//	bool updateWindow();
String auplugin::getInfo(std::vector<String>& list) { return "NOT IMPLEMENTED"; }
//	long dispatch(
//		long opcode = 0,
//		long index = 0,
//		long value = 0,
//		void *ptr = 0,
//		float opt = 0);
bool auplugin::getNameString(char* szBuf){ szBuf[0] = 0; return false; }
void auplugin::printNames(){ }
bool auplugin::onClose(){ return true; }
void auplugin::onWindowDestroy(){ }
bool auplugin::onShow(vst_window* window){return false; }
bool auplugin::updateWindowSize(){ return false; }
bool auplugin::onResize(vst_window* window, ivec2 size){ return false; }
ivec2 auplugin::constrainSize(vst_window* window, ivec2& size){return size;}
bool auplugin::show(){ return false; }
bool auplugin::close(){ return false; }
void auplugin::unload(vsthost* host){ }
void auplugin::load(vsthost* host){ }
//	vst_param_category* getCategory(int idx);
//	void recvPluginEditParamUpdate(int32_t idx);

//automatable_t
String auplugin::getAutomatableName(){ return "AU_PLUGIN"; }

void auplugin::makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks){ }
void auplugin::loadSnapshot(const plugin_snapshot_t& pluginSnapshot){ }
guiplugin* auplugin::makeGui(){ return nullptr; }
guiplugin* auplugin::getGui(){ return nullptr; }
void auplugin::process(AudioBlock* in, AudioBlock* out, int32_t samplePos, int32_t numSamples, playback_state state){

}
int32_t auplugin::getDelay() { return 0; }

float auplugin::getParamValue(int32_t idx) {
	automatable_param_t* param = getParam(idx);
	dbgassert(param);
	if (param->internalIdx >= 0) {
		param->value = 0;// vst_getParameter(this, handle->aeffect, param->internalIdx);
	}
	return param->value;
}
String auplugin::getParamValueDisplay(int32_t idx) {
	automatable_param_t* param = getParam(idx);
	dbgassert(param);
	if (param->internalIdx >= 0) {
		char buf[1024];
		memset(buf, 0, sizeof(buf));
//		this->dispatch(effGetParamDisplay, param->internalIdx, 0, buf);
		return StringFormat("%s", buf);
	}
	return effectbase::getParamValueDisplay(idx);
}
void auplugin::setParamValue(int32_t idx, float val, int flags) {
	automatable_param_t* param = getParam(idx);
	dbgassert(param);
	param->value = val;
	if (param->idx == PARAM_ENABLE) {
		bool wasEnable = this->bIsEnabled;
		this->bIsEnabled = val > 0;
		if (this->bIsEnabled != wasEnable) {
			if (this->bIsEnabled) {
				onEnable();
			} else {
				onDisable();
			}
		}
	} else {
		if (param->internalIdx >= 0) {
//			vst_setParameter(this, handle->aeffect, param->internalIdx, val);
		}
	}
}
void auplugin::postSetParameter(int32_t idx, float preVal, float val, int flags) {
	if (flags != 2) {
		return;
	}
//	dbgassert(this->trackImpl->getTrack());
//	track_t* track = this->trackImpl->getTrack();
//	automationlane_snapshot_t ref = toRef();
//	parameter_ref_t p = {track->idx,  ref.type, this->projectGlobalId, idx};
//	DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
}
//void auplugin::recvPluginEditParamUpdate(int32_t internalIdx) {
//	automatable_param_t* param = getEffectParam(internalIdx);
//	dbgassert(param && param->internalIdx >= 0);
//	param->value = vst_getParameter(this, handle->aeffect, param->internalIdx);
//}
automationlane_snapshot_t auplugin::toRef() {
	automationlane_snapshot_t ref;
	ref.type = AUTOMATABLE_EFFECT;
	ref.refId = this->projectGlobalId;
	return ref;
}

void auplugin::onEnable() {
	//TODO: check current thread, check if playthread is locked
	resume();
}
void auplugin::onDisable() {
	//TODO: check current thread, check if playthread is locked
	sleep();
//	vsthost::getInstance()->sendNotesOff(this);
}
