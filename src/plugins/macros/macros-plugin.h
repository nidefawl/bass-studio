#pragma once
#include "str_util.h"
#include "modules.h"
#include "host/plugin/internal_plugin.h"
#include "plugins/plugin-ui.h"

namespace PluginMacros {
struct ui_layout_t {
    int32_t uiId = 0;
    int32_t numActive = 0;
};
struct snapshot_t {
    int32_t version = 0;
    std::vector<ui_layout_t> uiLayout;
};
class module_macros : public internal_automator {
    struct macro_impl_t;
    macro_impl_t* const impl;
public:
    explicit module_macros(int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~module_macros() override;
    int getModuleType() override { return PLUGIN_TYPE_MACROS; };
    bool hasAutomationModulationOutput() const override {
        return true;
    }
    std::shared_ptr<PluginViewContainers> createViewCtrInternal() override;
    const automated_param_t* getModulationOutputData(const DAW::automation_channel_ref& channel) override;
    std::shared_ptr<std::vector<std::byte>> storePresetData() override;
    bool loadPresetData(const std::shared_ptr<std::vector<std::byte>>& buf) override;
    void getUiSnapshot(snapshot_t& snapshot);
    void setUiSnapshot(snapshot_t& snapshot);
};
}
