#pragma once
#include <memory>
#include <vector>
#include <cmath>
#include "host/automation/automation.hpp"
#include "config.hpp"
#include "gui/container/container.hpp"
#include "gui/controls/textfield.hpp"
#include "gui/controls/knobpluginparam.hpp"
#include "host/plugin/base/base-plugin.hpp"
#include "host/plugin/internal/internal-plugin.hpp"
#include "host/plugin/vst/vstplugin.hpp"
#include "plugin.hpp"
#include <vstsdk-plugin-2.4/audioeffectx.h>

class guictr_plugin_basic : public guictr_base {
    effectbase* const module;
protected:
    std::vector<guiknob_pluginparam*> knobs;
    gui_textfield editfield;
    void init() {
        setLayoutMode(LAYOUT_HORIZONTAL);
        editfield.setFlag(FLG_NO_LAYOUT, true);
        setBackgroundRendered(true);
        setCanMouseHit(true);
        padding = 4;
        margin  = 2;
        editfield.setVisible(false);
        editfield.setAlignment(gui_textfield::Alignment::Center);
        editfield.setReturnCommits(true);
    }
public:
    explicit guictr_plugin_basic(internalplugin* module) : guictr_base(),
        module(module)
    {
        init();
        std::vector<automatable_param_t*> paramsSorted;
        module->getSortedParams(paramsSorted);
        erase_if(paramsSorted, [](const automatable_param_t* p) {
            return p->idx == PARAM_ENABLE;
        });
        knobs.reserve(paramsSorted.size());
        for (automatable_param_t* param : paramsSorted) {
            knobs.push_back(new guiknob_pluginparam(param->idx, param->idx, guiknob::knobtype::SLIDER_LABELED));
            add(knobs.back());
        };
        add(&editfield);
    }
    explicit guictr_plugin_basic(BasePluginVST2* plugin) : guictr_base(),
        module(plugin->getHostSideHandle())
    {
        init();
        const int32_t numParams = plugin->getAeffect()->numParams;
        knobs.reserve(numParams);
        for (int32_t i = 0; i < numParams; ++i) {
            int32_t clientParamIdx = i;
            int32_t paramIdx = PARAM_OFFSET_EXTERNAL+i;
            knobs.push_back(new guiknob_pluginparam(paramIdx, clientParamIdx, guiknob::knobtype::SLIDER_LABELED));
            add(knobs.back());
        }
        add(&editfield);
    }
    std::vector<guiknob_pluginparam*>& getKnobs() {
        return knobs;
    }
    ~guictr_plugin_basic() override {
        removeGuis();
        for (guiknob_pluginparam* knob : knobs) {
            delete knob;
        }
    }
    void buttonClicked(guibase* button) override {
        auto param = dynamic_cast<guiknob_pluginparam*>(button);
        if (param && module) {
            auto paramIdx = param->getParamIdx();
            auto layout = param->getLayout();
            auto paramValue = module->getParamValueDisplay(paramIdx);
            editfield.mCallbackEnd = [this, param, paramValue, paramIdx](const std::string& str) {
                auto paramConverted = module->convertParamValueDisplay(param->getParamIdx(), param_unit_t{str, paramValue.unit});
                if (paramConverted.success) {
                    module->setParamValue(paramIdx, paramConverted.floatVal, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
                    if (param->fnValueEditChanged)
                        param->fnValueEditChanged(param->getValue(), paramConverted.floatVal);
                }
                editfield.setVisible(false);
                return true;
            };
            
            auto pValue = param->parent->toScreenSpace(layout.pValue);
            pValue = toControlsObjectSpace(pValue, editfield.parent);
            editfield.pos = pValue;
            editfield.size = layout.sValue;
            editfield.setVisible(true);
            editfield.layout();
            editfield.setValue(paramValue.value);
            editfield.setSelectionRange(-1, -1);
            editfield.setFontSize(layout.valueHeight*layout.fontScaleValue);
            parentCtrl->focusGui(&editfield);
            return;
        }
        guictr_base::buttonClicked(button);
    }

    virtual void onGuiOpen() {
        for (auto knob : knobs) {
            knob->setEffectInstance(module);
        }
    }

    virtual void onGuiClose() {
        for (auto knob : knobs) {
            knob->setEffectInstance(nullptr);
        }
    }

    guiknob_pluginparam* getKnobFromParameter(int32_t index) {
        auto it = std::find_if(knobs.begin(), knobs.end(), [index](guiknob_pluginparam* knob) {
            return knob->getParamIdx() == index;
        });
        return it != knobs.end() ? *it : nullptr;
    }

    void onSetParameter(int32_t index, float value) {
    }
    virtual void getSizeScale(int& w, int& h) const {
        w = 100*knobs.size();
        h = 300;
    }
};