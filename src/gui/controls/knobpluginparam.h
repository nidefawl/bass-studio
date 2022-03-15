#pragma once
#include "nanovg/nanovg_min.h"
#include <vector>
#include <cmath>
#include <memory>

#include "str_util.h"
#include "math/seq_math.h"
#include "color_util.h"
#include "gui/gui.h"
#include "knoblabeled.h"
#include "basectrl.h"

#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "host/plugin/base_plugin.h"
#include "host/plugin/vst_plugin.h"

#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"

class guiknob_pluginparam : public guiknob_labeled_base {
    AudioEffect* curEffect    = nullptr;
    int32_t internalEffectIdx = 0;
    //#if BUILD_VSTHOST
    vstplugin* hostSidePlugin = nullptr;
    //#endif
public:
    guiknob_pluginparam(int _paramIdx, int _internalEffectIdx) : guiknob_labeled_base(false) {
        //#if BUILD_VSTHOST
        paramIdx = _paramIdx;
        //#endif
        internalEffectIdx  = _internalEffectIdx;
        fnValueEditChanged = [this](float preVal, float val) {
            if (curEffect) {
                curEffect->setParameterAutomated(internalEffectIdx, val);
                setDisplayValueFromEffect();
            }
        };
        //#if BUILD_VSTHOST
        setAutomationHandlers();
        //#endif
    }
    ~guiknob_pluginparam() override = default;
    //#if BUILD_VSTHOST
    void setEffectInstance(vstplugin* _hostSidePlugin) {
        hostSidePlugin   = _hostSidePlugin;
        paramAutomatable = _hostSidePlugin;
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (this->contains(mpos)) {
            if (evt.type != MouseHitType::MOUSE_RIGHT) {
                if (guiknob::mouseHitTest(mpos, evt)) {
                    return true;
                }
            }
            evt.requestFocus(this);
            return true;
        }
        return false;
    }
    //#endif
    void setAudioEffect(AudioEffect* eff) {
        this->curEffect = eff;
        if (eff) {
            setValueInit(eff->getParameter(internalEffectIdx));
            setLabel(eff->getParameterName(internalEffectIdx));
        }
        setDisplayValueFromEffect();
    }
    void setDisplayValueFromEffect() {
        if (this->curEffect) {
            String display     = curEffect->getParameterDisplay(internalEffectIdx);
            String displayUnit = curEffect->getParameterLabel(internalEffectIdx);
            this->valueDisplay = display + displayUnit;
        } else {

            this->valueDisplay = "???";
        }
    }
    void setDisplayValue(float f) override {
    }
};
