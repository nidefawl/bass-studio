#pragma once
#include "str_util.h"
#include "modules.h"
#include "host/plugin/internal_plugin.h"

class guiplugin;
class vsthost;
struct audio_stage_t;

class module_gain : public internalplugin {
public:
    const float DBFS_MUTE_POS = -101.0f;
    const float MTR_CEIL      = 24.0f;
    explicit module_gain(int32_t _projectGlobalId, i_host_callback* _hostCallback);

    int getModuleType() override { return PLUGIN_TYPE_GAIN; };
    void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t getParamValueDisplay(int32_t idx) override;
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    std::shared_ptr<PluginViewContainers> createViewCtrInternal() override;
};
