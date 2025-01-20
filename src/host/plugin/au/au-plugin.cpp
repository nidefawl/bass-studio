#ifdef __APPLE__
#endif
#include "types.hpp"
#include "math/vec.hpp"
#include "str_util.hpp"
#include "seq_time.hpp"

#include "host/track/track.hpp"
#include "host/track/track_impl.hpp"
#include "au-plugin.hpp"

void auplugin::unload(DAW::Host::PluginManager* host) {}
void auplugin::load(DAW::Host::PluginManager* host) {}

String auplugin::getAutomatableName() { return "AU_PLUGIN"; }

void auplugin::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {}
void auplugin::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {}
std::shared_ptr<guiplugin> auplugin::createGuiPlugin(int32_t uuid) { return nullptr; }
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
