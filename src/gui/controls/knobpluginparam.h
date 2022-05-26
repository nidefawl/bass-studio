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
    AudioEffectX* curEffect    = nullptr;
    int32_t internalEffectIdx = -1;
    effectbase* hostSidePlugin = nullptr;
public:
    explicit guiknob_pluginparam(int _paramIdx, int _internalEffectIdx = -1) 
        : guiknob_labeled_base(false)
    {
        paramIdx = _paramIdx;
        internalEffectIdx  = _internalEffectIdx;
    }
    ~guiknob_pluginparam() override = default;
    void setEffectInstance(effectbase* _hostSidePlugin) {
        hostSidePlugin   = _hostSidePlugin;
        paramAutomatable = _hostSidePlugin;
        if (hostSidePlugin) {
            setKnobInternalHandlers();
            fnValueEditChanged = [this](float preVal, float val) {
                if (paramAutomatable) {
                    setValueInit(val);
                    setDisplayValueFromEffect();
                }
            };
            setValueInit(hostSidePlugin->getParamValue(paramIdx));
            setLabel(hostSidePlugin->getParamName(paramIdx));
        }
        setDisplayValueFromEffect();
    }
#if BUILD_EXTERNAL_PLUGIN
    void setAudioEffect(AudioEffectX* eff) {
        this->curEffect = eff;
        if (eff) {
            setKnobVST2Handlers();
            setValueInit(eff->getParameter(internalEffectIdx));
            setLabel(eff->getParameterName(internalEffectIdx));
        }
        setDisplayValueFromEffect();
    }
    void setKnobVST2Handlers() {
        log_lf(Log::L_DEBUG, "guiknob_pluginparam::setKnobVST2Handlers()\n");
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
        // fnGetValue = [this]() {
        //     if (curEffect) {
        //         return paramAutomatable->getParamValue(paramIdx);
        //     }
        //     return value;
        // };
        // fnSetValue = [this](float f, int flags) {
        //     if (paramAutomatable) {
        //         ThreadLock lock     = MainCtrl::getPlayThread()->lockThread();
        //         automation_t* param = paramAutomatable->getRegisteredAutomation(paramIdx);
        //         if (param) {
        //             param->active = false;
        //         }
        //         paramAutomatable->setParamValue(paramIdx, f, flags);
        //     }
        // };
        // fnValueEditFinish = [this](float preVal, float val) {
        //     if (paramAutomatable) {
        //         paramAutomatable->postSetParameter(paramIdx, preVal, val, FLG_PAR_UPDATE_USER);
        //     }
        // };
        // fnFocus = [this](MouseHitEvt& evt, bool focused) {
        //     if (paramAutomatable) {
        //         auto* track = paramAutomatable->getTrack();
        //         if (!track)
        //             return;
        //         auto* guiTrackCtr   = dawCtrl->getTrackContainer();
        //         track_gui_entry_t* entry{};
        //         if (!guiTrackCtr->getPointerEntry(track, &entry))
        //             return;
        //         guiTrackCtr->showAutomationLane(entry, paramAutomatable, paramIdx);
        //         dawCtrl->updateVisibleTrackContents();
        //         guiTrackCtr->scrollTo(entry->content);
        //     }
        // };
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
    void setDisplayValueFromEffect() {
        if (this->curEffect) {
            this->valueDisplay  = curEffect->getParameterDisplay(internalEffectIdx);
            this->valueDisplay += curEffect->getParameterLabel(internalEffectIdx);
        } else if (this->hostSidePlugin) {
            this->valueDisplay = hostSidePlugin->formatDisplayValue(paramIdx);
        } else {

            this->valueDisplay = "???";
        }
    }
};
