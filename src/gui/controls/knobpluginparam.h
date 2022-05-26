#pragma once
#include "logging.h"
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
    int32_t internalEffectIdx;
#if BUILD_EXTERNAL_PLUGIN
    AudioEffectX* curEffect    = nullptr;
#endif
#if BUILD_VSTHOST
    effectbase* hostSidePlugin = nullptr;
#endif
public:
    explicit guiknob_pluginparam(int _paramIdx, int _internalEffectIdx = -1) 
        : guiknob_labeled_base(false), internalEffectIdx(_internalEffectIdx)
    {
        (void)internalEffectIdx;
        paramIdx = _paramIdx;
    }
    ~guiknob_pluginparam() override = default;
    void setEffectInstance(effectbase* _hostSidePlugin) {
#if BUILD_VSTHOST
        hostSidePlugin   = _hostSidePlugin;
        paramAutomatable = _hostSidePlugin;
        if (hostSidePlugin) {
            setKnobInternalHandlers();
            fnValueEditChanged = [this](float preVal, float val) {
                setValueInit(val);
            };
            fnGetDisplayValue = [this](float val) {
                auto paramDisplay = hostSidePlugin->getParamValueDisplay(paramIdx);
                if (paramDisplay.unit.empty())
                    return paramDisplay.value;
                return paramDisplay.value + " " + paramDisplay.unit;
            };
            setValueInit(hostSidePlugin->getParamValue(paramIdx));
            setLabel(hostSidePlugin->getParamName(paramIdx));
        }
#endif
    }
#if BUILD_EXTERNAL_PLUGIN
    void setAudioEffect(AudioEffectX* eff) {
        this->curEffect = eff;
        if (eff) {
            setKnobVST2Handlers();
            setValueInit(eff->getParameter(internalEffectIdx));
            setLabel(eff->getParameterName(internalEffectIdx));
        }
    }
    void setKnobVST2Handlers() {
        fnValueEditBegin = [this](float preVal, float val) {
            if (curEffect) {
                curEffect->beginEdit(internalEffectIdx);
            }
        };
        fnValueEditChanged = [this](float preVal, float val) {
            if (curEffect) {
                curEffect->setParameterAutomated(internalEffectIdx, val);
            }
        };
        fnValueEditFinish = [this](float preVal, float val) {
            if (curEffect) {
                curEffect->endEdit(internalEffectIdx);
            }
        };
        fnGetDisplayValue = [this](float val) {
            String displayValCached = curEffect->getParameterDisplay(internalEffectIdx);
            displayValCached += curEffect->getParameterLabel(internalEffectIdx);
            return displayValCached;
        };
    }
#endif
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
};
