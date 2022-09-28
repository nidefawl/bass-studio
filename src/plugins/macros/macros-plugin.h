#pragma once
#include "str_util.h"
#include "modules.h"
#include "host/plugin/internal_plugin.h"
#include "plugins/plugin-ui.h"

namespace PluginMacros {
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
    const automated_param_t* getModulationOutputData(int32_t channel) const override;
};
}
