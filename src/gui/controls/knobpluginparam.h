#pragma once
#include "assert_dbg.h"
#include "host/automation/automation.h"
#include "gui/controls/knob.h"
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
#include "host/plugin/base/base-plugin.h"
#include "host/plugin/vst/vstplugin.h"

#include <vstsdk-plugin-2.4/audioeffectx.h>

class guiknob_pluginparam : public guiknob_labeled_base {
    AudioEffectX* curEffect    = nullptr;
    int clientParamIdx = -1;
    effectbase* hostSidePlugin = nullptr;
public:
    explicit guiknob_pluginparam(int _paramIdx, int _clientParamIdx = -1, guiknob::knobtype _knobtype = guiknob::knobtype::KNOB_LABELED)
        : guiknob_labeled_base(_knobtype)
    {
        paramIdx = _paramIdx;
        clientParamIdx = _clientParamIdx;
        m_layout.inset = 6;
        m_layout.renderLabelBorder = false;
        if (_knobtype == guiknob::knobtype::KNOB_LABELED) {
            setLabelsFontScale(0.9f, 0.9f);
            setLabelsScale(0.2f, 0.2f);
        }
        if (_knobtype == guiknob::knobtype::SLIDER_LABELED) {
            setLabelsFontScale(0.7f, 0.8f);
            setLabelsScale(0.1f, 0.1f);
        }
        setBackgroundRendered(true);
    }
    ~guiknob_pluginparam() override = default;

    float getQuantizationStep() const override {
        if (paramAutomatable) {
            auto p = paramAutomatable->getParam(paramIdx);
            if (assert_expr(p)) {
                return p->quantizationSteps ? 1.0f / p->quantizationSteps : 0.0f;
            }
        }
        return 0.0f;
    }
    void setEffectInstance(effectbase* _hostSidePlugin) {
        hostSidePlugin   = _hostSidePlugin;
        paramAutomatable = _hostSidePlugin;
        if (hostSidePlugin) {
            setKnobInternalHandlers();
            fnValueEditChanged = [this](float preVal, float val) {
                setValueInit(val);
            };
            fnValueEditBegin = [this](float preVal, float val) {
                hostSidePlugin->getHostCallback()->onParametersChanged(hostSidePlugin, paramIdx, val, 0, 0);
            };
            fnValueEditChanged = [this](float preVal, float val) {
                setValueInit(val);
                hostSidePlugin->getHostCallback()->onParametersChanged(hostSidePlugin, paramIdx, val, 0, 1);
            };
            fnValueEditFinish = [this](float preVal, float val) {
                hostSidePlugin->getHostCallback()->onParametersChanged(hostSidePlugin, paramIdx, val, 0, 2);
                paramAutomatable->postSetParameter(paramIdx, preVal, getValue(), FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
            };
            setValueInit(paramAutomatable->getParam(paramIdx)->getValue());
            setLabel(hostSidePlugin->getParamName(paramIdx));
        }
    }
    int32_t getParamIdxInternal() const { return clientParamIdx; }
    void setAudioEffect(AudioEffectX* eff) {
        this->curEffect = eff;
        if (eff) {
            setKnobVST2Handlers();
            setValueInit(eff->getParameter(clientParamIdx));
            setLabel(eff->getParameterName(clientParamIdx));
        }
    }
    void setKnobVST2Handlers() {
        fnValueEditBegin = [this](float preVal, float val) {
            if (curEffect) {
                curEffect->beginEdit(clientParamIdx);
            }
        };
        fnValueEditChanged = [this](float preVal, float val) {
            if (curEffect) {
                curEffect->setParameterAutomated(clientParamIdx, val);
            }
        };
        fnValueEditFinish = [this](float preVal, float val) {
            if (curEffect) {
                curEffect->endEdit(clientParamIdx);
            }
        };
        fnOverrideGetDisplay = [this](float val) {
            String displayVal = curEffect->getParameterDisplay(clientParamIdx);
            displayVal += curEffect->getParameterLabel(clientParamIdx);
            return displayVal;
        };
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        return guiknob_labeled_base::mouseHitTest(mpos, evt);
    }
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
    effectbase* getEffectInstance() {
        return hostSidePlugin;
    }
};
