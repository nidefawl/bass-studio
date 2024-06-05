#pragma once
#include "host/shape/shape.h"
#include "str_util.h"
#include "host/plugin/modules.h"
#include "host/plugin/internal/internal-plugin.h"
#include "plugins/plugin-ui.h"
#include "lfo-snapshot.hpp"

namespace PluginLFO {

class module_lfo final : public internal_modulator {
    friend class guictr_module_lfo;
    struct lfo_impl_t;
    lfo_impl_t* const impl;
public:
    explicit module_lfo(int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~module_lfo() override;
    void initModChannels() override;
    PluginType getPluginType() override { return PLUGIN_TYPE_LFO; };
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override;
    const automated_param_t* getModulationOutputData(const DAW::modulation_channel_ref& channel) override;
    std::shared_ptr<std::vector<std::byte>> storePresetData() override;
    bool loadPresetData(const std::shared_ptr<std::vector<std::byte>>& buf) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;

    void getUiSnapshot(snapshot_t& snapshot);
    void setUiSnapshot(snapshot_t& snapshot);
    DAW::Shape::shape_t& getShape(int32_t idx);
    int32_t getSyncRatio(int32_t chIdx) const;
    bool isShapeMode(int32_t chIdx) const;
    void setSyncRatio(int32_t chIdx, int32_t ratio);
    void setShapeMode(int32_t chIdx);
    void setRandomMode(int32_t chIdx, int32_t mode = -1);
    int32_t getRandomMode(int32_t chIdx) const;
};
}
