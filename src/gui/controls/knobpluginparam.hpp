#pragma once
#include "assert_dbg.h"
#include "host/automation/automation.hpp"
#include "gui/controls/knob.hpp"
#include "logging.hpp"
#include "nanovg/nanovg_min.h"
#include <vector>
#include <cmath>
#include <memory>

#include "str_util.hpp"
#include "math/seq_math.hpp"
#include "color_util.hpp"
#include "gui/gui.hpp"
#include "knoblabeled.hpp"
#include "basectrl.hpp"

#include "plugins/plugin.hpp"
#include "plugins/plugin-base.hpp"
#include "host/plugin/base/base-plugin.hpp"
#include "host/plugin/vst/vstplugin.hpp"

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

    explicit guiknob_pluginparam(guiknob::knobtype _knobtype = guiknob::knobtype::KNOB_LABELED)
        : guiknob_labeled_base(_knobtype)
    {
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

    void setParamIdx(int _paramIdx) {
        paramIdx = _paramIdx;
        clientParamIdx = _paramIdx;
    }

    ~guiknob_pluginparam() override = default;

    void setEffectInstance(effectbase* _hostSidePlugin) {
        hostSidePlugin   = _hostSidePlugin;
        paramAutomatable = _hostSidePlugin;
        if (hostSidePlugin) {
            setKnobInternalHandlers();
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
