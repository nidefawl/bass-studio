#ifdef __APPLE__
#endif
#include "types.h"
#include "math/vec.h"
#include "str_util.h"
#include "seq_time.h"

#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "au-plugin.h"

void auplugin::unload(DAW::Host::PluginManager* host) {}
void auplugin::load(DAW::Host::PluginManager* host) {}

String auplugin::getAutomatableName() { return "AU_PLUGIN"; }

void auplugin::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {}
void auplugin::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {}
guiplugin* auplugin::makeGui() { return nullptr; }
guiplugin* auplugin::getGui() { return nullptr; }
void auplugin::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
}
samplecount_t auplugin::getPluginLatency() { return 0; }

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
