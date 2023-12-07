#pragma once
#include "host/shape/shape.h"
#include "str_util.h"
#include "host/plugin/modules.h"
#include "host/plugin/internal/internal-plugin.h"
#include "plugins/plugin-ui.h"

namespace PluginLFO {

struct impl_channel_snapshot_t;

struct ui_layout_t {
    int32_t uiId = 0;
    int32_t numActive = 0;
};

struct snapshot_t {
    int32_t version = 0;
    std::vector<ui_layout_t> uiLayout;
    std::vector<impl_channel_snapshot_t> channels;
};

class module_lfo final : public internal_modulator {
    friend class guictr_module_lfo;
    struct lfo_impl_t;
    lfo_impl_t* const impl;
public:
    explicit module_lfo(int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~module_lfo() override;
    void initModChannels() override;
    int getModuleType() override { return PLUGIN_TYPE_LFO; };
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override;
    const automated_param_t* getModulationOutputData(const DAW::modulation_channel_ref& channel) override;
    std::shared_ptr<std::vector<std::byte>> storePresetData() override;
    bool loadPresetData(const std::shared_ptr<std::vector<std::byte>>& buf) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;

    void getUiSnapshot(snapshot_t& snapshot);
    void setUiSnapshot(snapshot_t& snapshot);
    DAW::Shape::shape_t& getShape(int idx);
    int32_t getSyncRatio(int chIdx) const;
    void setSyncRatio(int chIdx, int32_t ratio);
};
}
