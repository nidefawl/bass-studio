#include <cmath>
#include <algorithm>
#include <cstdio>
#include <memory>
#include "config.h"
#include "math/seq_math.h"
#include "plugins/plugin-ui.h"
#include "str_util.h"
#include "dsp_util.h"
#include "color_util.h"

#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/controls/button.h"
#include "gui/controls/knob.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_daw.h"

#include "basectrl.h"

#include "platform.h"

#include "latency-plugin.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "types.h"
#include "audioblock.h"
#include <vstsdk-plugin-2.4/audioeffectx.h>


#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginLatency::createPlugin(audioMaster);
}
#endif

namespace PluginLatency {
    const char* const PLUGIN_EFFECT_NAME = "Latency";
    const char* const PLUGIN_UID = "LTCY";
    const char* const PLUGIN_PRODUCT_NAME = "Latency introducing plugin";
    static constexpr int32_t MAX_LATENCY = 16384;

    PluginVST2_Latency::PluginVST2_Latency(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs) {
        setNewLatency(current()->latency);
    }

    void PluginVST2_Latency::setProgram(VstInt32 program) {
        if (program < 0 || program >= kNumPrograms)
            return;
        curProgram = program;
    }

    void PluginVST2_Latency::setProgramName(char* name) {
    }

    void PluginVST2_Latency::getProgramName(char* name) {
        if (name)
            name[0] = 0;
    }

    void PluginVST2_Latency::getParameterLabel(VstInt32 index, char* label) {
        switch (index) {
            case kLatency:
                vst_strncpy(label, "samples", PLUGIN_PARAM_STR_MAX_LEN);
                return;
            default:
                vst_strncpy(label, "", PLUGIN_PARAM_STR_MAX_LEN);
        }
    }

    void PluginVST2_Latency::getParameterDisplay(VstInt32 index, char* text) {
        text[0] = 0;
        switch (index) {
            case kLatency: {
                snprintf(text, PLUGIN_PARAM_STR_MAX_LEN, "%d", current()->latency);
                return;
            }
        }
        return BasePluginVST2::getParameterDisplay(index, text);
    }

    param_converted_t PluginVST2_Latency::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        switch (idx) {
            case kLatency: {
                return {math::clamp(math::roundfS64(fTextFieldVal)/static_cast<float>(MAX_LATENCY), 0.0f, 1.0f), true};
            }
            default:
                break;
        }
        return BasePluginVST2::convertParamValueDisplay(idx, displayValue);
    }

    void PluginVST2_Latency::getParameterName(VstInt32 index, char* label) {
        switch (index) {
            case kLatency:
                vst_strncpy(label, "Latency", PLUGIN_PARAM_STR_MAX_LEN);
                return;
        }
    }

    void PluginVST2_Latency::setParameter(VstInt32 index, float value) {
        Program* ap = current();
        switch (index) {
            case kLatency:
                ap->latency = math::max(0, math::min(MAX_LATENCY, math::roundfS32(value * MAX_LATENCY)));
                setNewLatency(ap->latency);
                break;
        }
#if BUILD_VSTHOST
        for (auto& pviewctr : this->views) {
            if (pviewctr->isInUse()) {
                pviewctr->onSetParameter(index, value);
            }
        }
#else
        if (this->editor) {
            static_cast<pluginwindow*>(this->editor)->onSetParameter(index, value);
        }
#endif
    }

    float PluginVST2_Latency::getParameter(VstInt32 index) {
        Program* ap = current();
        float value = 0;
        switch (index) {
            case kLatency:
                value = std::max(0.0f, std::min(1.0f, ap->latency / static_cast<float>(MAX_LATENCY)));
                break;
        }
        return value;
    }

    bool PluginVST2_Latency::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) {
        if (index >= 0 && index < kNumPrograms) {
            vst_strncpy(text, "Default", PLUGIN_PROGRAM_STR_MAX_LEN);
            return true;
        }
        return false;
    }

    bool PluginVST2_Latency::getEffectName(char* name) {
        vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
        return true;
    }

    bool PluginVST2_Latency::getVendorString(char* text) {
        vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
        return true;
    }

    bool PluginVST2_Latency::getProductString(char* text) {
        vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
        return true;
    }

    void PluginVST2_Latency::setNewLatency(int32_t nSamplesLatency) {
        this->newLatency     = nSamplesLatency;
        this->latencyChanged = true;
    }

    VstInt32 PluginVST2_Latency::getVendorVersion() {
        return 1;
    }

    VstInt32 PluginVST2_Latency::canDo(char* text) {
        if (!strcmp(text, "receiveVstTimeInfo"))
            return 1;
        return -1;// explicitly can't do; 0 => don't know
    }

    void PluginVST2_Latency::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
        if (issetprogram)
            return;

        if (sampleFrames != blockSize) {
            return;
        }
        if (this->latencyChanged) {
            this->latencyChanged             = false;
            this->curLatency                 = this->newLatency;
            this->getAeffect()->initialDelay = this->curLatency;
        }
        dbgassert(this->curLatency >= 0 && this->curLatency <= (1 << 20));
        int32_t nChannels = this->getAeffect()->numOutputs;
        if (!this->delayLine) {
            this->delayLine = std::make_unique<DelayLine>();
        }
        AudioBlock inputBlock(inputs, nChannels, sampleFrames);
        AudioBlock outputBlock(outputs, nChannels, sampleFrames);
        delayAudio(this->delayLine.get(), &inputBlock, &outputBlock, this->curLatency);
    }


    Program::Program() : ProgramParameters() {
        vst_strncpy(name, "Init", PLUGIN_PROGRAM_STR_MAX_LEN);
        latency = 1024;
    }

    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new PluginVST2_Latency(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> PluginVST2_Latency::createView() {
        auto view = std::make_shared<SinglePluginViewContainers<guictr_vst2_simple, PluginVST2_Latency>>(this, 50, 150);
        this->views.push_back(view);
        return view;
    }
}
