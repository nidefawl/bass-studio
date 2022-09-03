#pragma once
#include <memory>
#include <vector>
#include <cmath>
#include "config.h"
#include "gui/container/container.h"
#include "gui/controls/textfield.h"
#include "gui/controls/knobpluginparam.h"
#include "host/plugin/base_plugin.h"
#include "host/plugin/internal_plugin.h"
#include "host/plugin/vst_plugin.h"
#include "plugin.h"
#include <vstsdk-plugin-2.4/audioeffect.h>
#include <vstsdk-plugin-2.4/audioeffectx.h>

class guictr_vst2_simple : public guictr_base {
#if BUILD_EXTERNAL_PLUGIN
    BasePluginVST2* vst2Handle;
#endif
    effectbase* const module;
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
    explicit guictr_vst2_simple(internalplugin* module) : guictr_base(),
        module(module)
    {
        init();
        const int32_t numParams = module->getNumParameters();
        knobs.reserve(numParams);
        for (int32_t i = 1; i < numParams; ++i) {
            knobs.push_back(new guiknob_pluginparam(i, -1, guiknob::knobtype::SLIDER_LABELED));
            add(knobs.back());
        }
        add(&editfield);
    }
    explicit guictr_vst2_simple(BasePluginVST2* plugin) : guictr_base(),
#if BUILD_EXTERNAL_PLUGIN
        vst2Handle(plugin),
#endif
        module(plugin->getHostSideHandle())
    {
        init();
        const int32_t numParams = plugin->getAeffect()->numParams;
        knobs.reserve(numParams);
        for (int32_t i = 0; i < numParams; ++i) {
#if BUILD_EXTERNAL_PLUGIN
            int32_t paramIdx = i;
#else
            int32_t paramIdx = PARAM_OFFSET_EXTERNAL+i;
#endif
            knobs.push_back(new guiknob_pluginparam(paramIdx, -1, guiknob::knobtype::SLIDER_LABELED));
            add(knobs.back());
        }
        add(&editfield);
    }
    ~guictr_vst2_simple() override {
        removeGuis();
    }
    void layoutEntries(ivec2 dir) override {
        guictr_base::layoutEntries(dir);
    }
    void buttonClicked(guibase* button) override {
        auto param = dynamic_cast<guiknob_pluginparam*>(button);
        if (param && module) {
            auto paramIdx = param->getParamIdx();
            auto paramValue = module->getParamValueDisplay(paramIdx);
            editfield.mCallbackEnd = [this, param, paramValue, paramIdx](const std::string& str) {
                auto paramConverted = module->convertParamValueDisplay(param->getParamIdx(), param_unit_t{str, paramValue.unit});
                if (paramConverted.success) {
                    module->setParamValue(paramIdx, paramConverted.floatVal, FLG_PAR_UPDATE_USER);
                    if (param->fnValueEditChanged)
                        param->fnValueEditChanged(param->getValue(), paramConverted.floatVal);
                }
                editfield.setVisible(false);
                return true;
            };
            auto layout = param->getLayout();
            editfield.pos = layout.pValue;
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

    void onGuiOpen() {
        for (auto knob : knobs) {
#if BUILD_VSTHOST
            knob->setEffectInstance(module);
#endif
#if BUILD_EXTERNAL_PLUGIN
            knob->setAudioEffect(vst2Handle);
#endif
        }
    }

    void onGuiClose() {
        for (auto knob : knobs) {
#if BUILD_VSTHOST
            knob->setEffectInstance(nullptr);
#endif
#if BUILD_EXTERNAL_PLUGIN
            knob->setAudioEffect(nullptr);
#endif
        }
    }

    guiknob_pluginparam* getKnobFromParameter(int32_t index) {
        auto it = std::find_if(knobs.begin(), knobs.end(), [index](guiknob_pluginparam* knob) {
            return knob->getParamIdx() == index;
        });
        return it != knobs.end() ? *it : nullptr;
    }

    void onSetParameter(int32_t index, float value) {
#if BUILD_EXTERNAL_PLUGIN
        guiknob_pluginparam* knob = getKnobFromParameter(index);
        if (knob) {
            knob->setValueInit(value);
        }
#endif
    }
    void getSizeScale(int& w, int& h) const {
        w = 100*knobs.size();
        h = 300;
    }
};