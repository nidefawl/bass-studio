#ifdef __APPLE__
#endif
#include "types.h"
#include "math/vec.h"
#include "str_util.h"
#include "seq_time.h"

#include "track.h"
#include "track_impl.h"
#include "au_plugin.h"

String auplugin::getInfo(std::vector<String>& list) { return "NOT IMPLEMENTED"; }

bool auplugin::getNameString(char* szBuf) {
    szBuf[0] = 0;
    return false;
}
void auplugin::printNames() {}
bool auplugin::onClose() { return true; }
void auplugin::onWindowDestroy() {}
bool auplugin::onShow(vst_window* window) { return false; }
bool auplugin::updateWindowSize() { return false; }
bool auplugin::onResize(vst_window* window, ivec2 size) { return false; }
ivec2 auplugin::constrainSize(vst_window* window, ivec2& size) { return size; }
bool auplugin::show() { return false; }
bool auplugin::close() { return false; }
void auplugin::unload(vsthost* host, int flags) {}
void auplugin::load(vsthost* host) {}

String auplugin::getAutomatableName() { return "AU_PLUGIN"; }

void auplugin::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {}
void auplugin::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {}
guiplugin* auplugin::makeGui() { return nullptr; }
guiplugin* auplugin::getGui() { return nullptr; }
void auplugin::process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
}
samplecount_t auplugin::getPluginLatency() { return 0; }

float auplugin::getParamValue(int32_t idx) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    if (param->internalIdx >= 0) {
        param->value = 0;// vst_getParameter(this, handle->aeffect, param->internalIdx);
    }
    return param->value;
}
param_unit_t auplugin::getParamValueDisplay(int32_t idx) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    if (param->internalIdx >= 0) {
        char buf[1024];
        memset(buf, 0, sizeof(buf));
        //this->dispatch(effGetParamDisplay, param->internalIdx, 0, buf);
        return {StringFormat("%s", buf), ""};
    }
    return effectbase::getParamValueDisplay(idx);
}
void auplugin::setParamValue(int32_t idx, float val, int flags) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    param->value = val;
    if (param->idx == PARAM_ENABLE) {
        updateOnEnableParam(param, this->bIsEnabled, val > 0, flags);
    } else {
        if (!(flags & FLG_PAR_UPDATE_NOSTORE) && !(flags & FLG_PAR_UPDATE_AUTOMATED)) {
            param->inUse = true;
        }
        if (param->internalIdx >= 0) {
            //dispatch update to plugin
        }
    }
}
void auplugin::postSetParameter(int32_t idx, float preVal, float val, int flags) {
    if (flags != 2) {
        return;
    }
}

automationlane_snapshot_t auplugin::toRef() const {
    automationlane_snapshot_t ref;
    ref.type  = AUTOMATABLE_EFFECT;
    ref.refId = this->projectGlobalId;
    return ref;
}

void auplugin::onEnable() {
}

void auplugin::onDisable() {
}
