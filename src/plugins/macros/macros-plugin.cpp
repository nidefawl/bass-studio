#include "macros-plugin.h"

namespace PluginMacros {
    static constexpr int32_t PARAM_NUM_MACROS = 32;
    static constexpr int32_t PARAM_MACROS_FIRST = 16;
    class guictr_macros : public guictr_vst2_simple {
        int32_t numKnobs = 4;
        public:
        guictr_macros(internalplugin* module) : guictr_vst2_simple(module) {
            setNumKnobs(numKnobs);
        }
        void setNumKnobs(int32_t num) {
            numKnobs = num;
            int32_t idx = 0;
            for (auto& knob : getKnobs()) {
                knob->setVisible(idx++ < num);
            }
        }
        void getSizeScale(int& w, int& h) const override {
            w = 100*numKnobs;
            h = 300;
        }
    };
    module_macros::module_macros(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internal_automator("Macros", getModuleType(), _projectGlobalId, _hostCallback)
    {
        for (int32_t i = 0; i < PARAM_NUM_MACROS; ++i) {
            auto paramIdx = PARAM_MACROS_FIRST + i;
            automatable_param_t* regparam = registerParam(paramIdx);
            regparam->defaultValue = 0.0f;
            regparam->value = 0.0f;
            regparam->name  = StringFormat("Macro %d", i);
            regparam->shortLabel  = StringFormat("Macro %d", i);
            regparam->unit  = "%";
        }
    }
    std::shared_ptr<PluginViewContainers> module_macros::createViewCtrInternal() {
        return std::make_shared<SinglePluginViewContainers<guictr_macros, module_macros>>(this, 100, 150);
    }
}// namespace PluginMacros

template<>
effectbase* makeInstance<PluginMacros::module_macros>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginMacros::module_macros(_projectGlobalId, _hostCallback);
}
