#pragma once
#include "str_util.hpp"
#include "host/plugin/modules.hpp"
#include "host/plugin/internal/internal-plugin.hpp"
#include "plugins/plugin-ui.hpp"

namespace PluginMacros {
struct ui_layout_t {
    int32_t uiId = 0;
    int32_t numActive = 0;
};
struct snapshot_t {
    int32_t version = 0;
    std::vector<ui_layout_t> uiLayout;
};
class module_macros final : public internal_modulator {
    friend class ctxtmenu_macro_count;
    struct macro_impl_t;
    macro_impl_t* const impl;
public:
    explicit module_macros(int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~module_macros() override;
    void initModChannels() override;
    PluginType getPluginType() override { return PLUGIN_TYPE_MACROS; };
    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override;
    const automated_param_t* getModulationOutputData(const DAW::modulation_channel_ref& channel) override;
    std::shared_ptr<std::vector<std::byte>> storePresetData() override;
    bool loadPresetData(const std::shared_ptr<std::vector<std::byte>>& buf) override;
    void getUiSnapshot(snapshot_t& snapshot);
    void setUiSnapshot(snapshot_t& snapshot);
};
}
