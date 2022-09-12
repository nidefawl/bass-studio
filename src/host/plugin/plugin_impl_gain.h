#pragma once
#include "str_util.h"
#include "modules.h"
#include "internal_plugin.h"
#include "host/vst_host.h"
#include "track_impl.h"

class guiplugin;
class vsthost;
struct audio_stage_t;

class module_gain : public internalplugin {
public:
    const float DBFS_MUTE_POS = -101.0f;
    const float MTR_CEIL      = 24.0f;
    explicit module_gain(int32_t _projectGlobalId);
    ~module_gain() override;

    float dispatchGetParameter(int32_t idx) override;
    void dispatchSetParameter(int32_t idx, float val) override;
    int getModuleType() override { return PLUGIN_TYPE_GAIN; };
    samplecount_t getPluginLatency() override;
    void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t getParamValueDisplay(int32_t idx) override;
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) override;
    std::shared_ptr<PluginViewContainers> createInternalView() override;
};
