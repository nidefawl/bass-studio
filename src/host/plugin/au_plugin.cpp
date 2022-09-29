#ifdef __APPLE__
#endif
#include "types.h"
#include "math/vec.h"
#include "str_util.h"
#include "seq_time.h"

#include "track.h"
#include "track_impl.h"
#include "au_plugin.h"

void auplugin::unload(DAW::Host::PluginManager* host, int flags) {}
void auplugin::load(DAW::Host::PluginManager* host) {}

String auplugin::getAutomatableName() { return "AU_PLUGIN"; }

void auplugin::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {}
void auplugin::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {}
guiplugin* auplugin::makeGui() { return nullptr; }
guiplugin* auplugin::getGui() { return nullptr; }
void auplugin::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
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
param_unit_t auplugin::convertParamValueToDisplay(int32_t idx, float value) {
    auto* param = getParamUnchecked(idx);
    dbgassert(param);
    if (param->internalIdx >= 0) {
        char buf[1024];
        memset(buf, 0, sizeof(buf));
        //this->dispatch(effGetParamDisplay, param->internalIdx, 0, buf);
        return {StringFormat("%s", buf), ""};
    }
    return effectbase::convertParamValueToDisplay(idx, value);
}

automatable_param_ref_t auplugin::toRef() const {
    automatable_param_ref_t ref;
    ref.type  = AUTOMATABLE_EFFECT;
    ref.refId = this->projectGlobalId;
    return ref;
}

void auplugin::onEnable() {
}

void auplugin::onDisable() {
}
