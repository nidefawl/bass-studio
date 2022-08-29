#pragma once
#include "automation.h"
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
#if BUILD_EXTERNAL_PLUGIN
    AudioEffectX* curEffect    = nullptr;
#endif
#if BUILD_VSTHOST
    effectbase* hostSidePlugin = nullptr;
#endif
public:
    explicit guiknob_pluginparam(int _paramIdx, int _internalEffectIdx = -1, guiknob::knobtype _knobtype = guiknob::knobtype::KNOB_LABELED)
        : guiknob_labeled_base(_knobtype)
    {
        paramIdx = _paramIdx;
        m_layout.inset = 6;
        m_layout.renderLabelBorder = false;
        setBackgroundRendered(true);
    }
    ~guiknob_pluginparam() override = default;
#if BUILD_VSTHOST
    void setToDefaultValue() override {
        if (hostSidePlugin) {
            hostSidePlugin->resetParamValue(paramIdx, FLG_PAR_UPDATE_USER);
        }
    }
#endif
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
            setValueInit(eff->getParameter(paramIdx));
            setLabel(eff->getParameterName(paramIdx));
        }
    }
    void setKnobVST2Handlers() {
        fnValueEditBegin = [this](float preVal, float val) {
            if (curEffect) {
                curEffect->beginEdit(paramIdx);
            }
        };
        fnValueEditChanged = [this](float preVal, float val) {
            if (curEffect) {
                curEffect->setParameterAutomated(paramIdx, val);
            }
        };
        fnValueEditFinish = [this](float preVal, float val) {
            if (curEffect) {
                curEffect->endEdit(paramIdx);
            }
        };
        fnGetDisplayValue = [this](float val) {
            String displayValCached = curEffect->getParameterDisplay(paramIdx);
            displayValCached += curEffect->getParameterLabel(paramIdx);
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
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
    effectbase* getEffectInstance() {
#if BUILD_VSTHOST
        return hostSidePlugin;
#endif
    }
};
