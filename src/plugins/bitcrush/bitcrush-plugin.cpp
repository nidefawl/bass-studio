#include <cmath>
#include <algorithm>
#include <cstdio>
#include <memory>
#include "config.h"
#include "math/seq_math.h"
#include "plugins/plugin-ui.h"
#include "str_util.h"
#include "dsp_util.h"

#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/container/container.h"

#include "basectrl.h"
#include "platform.h"
#include "../plugin.h"
#include "bitcrush-plugin.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "audioblock.h"

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginBitcrush::createPlugin(audioMaster);
}
#endif

namespace PluginBitcrush {
    const char* const PLUGIN_EFFECT_NAME = "Samplecrush";
    const char* const PLUGIN_UID = "SMPC";
    const char* const PLUGIN_PRODUCT_NAME = "Samplecrush plugin";

    PluginVST2_Bitcrush::PluginVST2_Bitcrush(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs) {
        setNewBitcrushLvl(current()->bitcrush);
    }

    void PluginVST2_Bitcrush::setProgram(VstInt32 program) {
        if (program < 0 || program >= kNumPrograms)
            return;
        curProgram = program;
    }

    void PluginVST2_Bitcrush::setProgramName(char* name) {
    }

    void PluginVST2_Bitcrush::getProgramName(char* name) {
        if (name)
            name[0] = 0;
    }

    void PluginVST2_Bitcrush::getParameterLabel(VstInt32 index, char* label) {
        switch (index) {
            case kSamples:
                vst_strncpy(label, "samples", PLUGIN_PARAM_STR_MAX_LEN);
                return;
            default:
                vst_strncpy(label, "", PLUGIN_PARAM_STR_MAX_LEN);
        }
    }

    void PluginVST2_Bitcrush::getParameterDisplay(VstInt32 index, char* text) {
        text[0] = 0;
        switch (index) {
            case kSamples: {
                int nPow2 = (1 << current()->bitcrush);
                snprintf(text, PLUGIN_PARAM_STR_MAX_LEN, "%d", nPow2);
                return;
            }
        }
        return BasePluginVST2::getParameterDisplay(index, text);
    }

    param_converted_t PluginVST2_Bitcrush::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        switch (idx) {
            case kSamples: {
                int nSamples = math::roundfS32(fTextFieldVal);
                int pow2 = math::roundfS32(std::log2f(nSamples));
                float fPow = pow2 / static_cast<float>(BITCRUSH_BITS_MAX - BITCRUSH_BITS_MIN);
                return {math::clamp(fPow, 0.0f, 1.0f), true};
            }
            default:
                break;
        }
        return BasePluginVST2::convertParamValueDisplay(idx, displayValue);
    }

    void PluginVST2_Bitcrush::getParameterName(VstInt32 index, char* label) {
        switch (index) {
            case kSamples:
                vst_strncpy(label, "Bitcrush", PLUGIN_PARAM_STR_MAX_LEN);
                return;
        }
    }

    void PluginVST2_Bitcrush::setParameter(VstInt32 index, float value) {
        Program* ap = current();
        switch (index) {
            case kSamples:
                ap->bitcrush = math::max(BITCRUSH_BITS_MIN, math::min(BITCRUSH_BITS_MAX, (int32_t) std::round(value * (BITCRUSH_BITS_MAX - BITCRUSH_BITS_MIN) + BITCRUSH_BITS_MIN)));
                setNewBitcrushLvl(ap->bitcrush);
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

    float PluginVST2_Bitcrush::getParameter(VstInt32 index) {
        Program* ap = current();
        float value = 0;
        switch (index) {
            case kSamples:
                value = std::max(0.0f, std::min(1.0f, (ap->bitcrush - BITCRUSH_BITS_MIN) / (float) (BITCRUSH_BITS_MAX - BITCRUSH_BITS_MIN)));
                break;
        }
        return value;
    }

    bool PluginVST2_Bitcrush::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) {
        if (index >= 0 && index < kNumPrograms) {
            vst_strncpy(text, "Default", PLUGIN_PROGRAM_STR_MAX_LEN);
            return true;
        }
        return false;
    }

    bool PluginVST2_Bitcrush::getEffectName(char* name) {
        vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
        return true;
    }

    bool PluginVST2_Bitcrush::getVendorString(char* text) {
        vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
        return true;
    }

    bool PluginVST2_Bitcrush::getProductString(char* text) {
        vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
        return true;
    }
    void PluginVST2_Bitcrush::setNewBitcrushLvl(int32_t nSamplesBitcrush) {
        this->newBitcrush        = nSamplesBitcrush;
        this->bitcrushLvlChanged = true;
    }

    VstInt32 PluginVST2_Bitcrush::getVendorVersion() {
        return 1;
    }

    VstInt32 PluginVST2_Bitcrush::canDo(char* text) {
        if (!strcmp(text, "receiveVstTimeInfo"))
            return 1;
        return -1;// explicitly can't do; 0 => don't know
    }

    static void processSampleCrush(float** inputs, float** outputs, VstInt32 sampleFrames, const int sampleCrushLevel) {
        float* out1 = outputs[0];
        float* out2 = outputs[1];
        float* in1  = inputs[0];
        float* in2  = inputs[1];
        int steps   = 1 << (sampleCrushLevel);
        if (steps <= 1) {
            for (int a = 0; a < sampleFrames; a++) {
                (*out1++) = (*in1++) < 0 ? -1 : 1;
                (*out2++) = (*in2++) < 0 ? -1 : 1;
            }
        } else {
            for (int a = 0; a < sampleFrames; a += steps) {
                float accL = 0;
                float accR = 0;

                for (int b = 0; b < steps; b++) {
                    accL += (*in1++);
                    accR += (*in2++);
                }
                for (int b = 0; b < steps; b++) {
                    (*out1++) = (accL) < 0 ? -1 : 1;
                    (*out2++) = (accR) < 0 ? -1 : 1;
                }
            }
        }
    }
    void PluginVST2_Bitcrush::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
        if (issetprogram)
            return;

        if (sampleFrames != blockSize) {
            return;
        }
        if (this->bitcrushLvlChanged) {
            this->bitcrushLvlChanged         = false;
            this->curBitcrush                = this->newBitcrush;
        }
        dbgassert(this->curBitcrush >= BITCRUSH_BITS_MIN && this->curBitcrush <= (BITCRUSH_BITS_MAX));

        if (this->getAeffect()->numOutputs == 2) {
            Program* ap = current();
            processSampleCrush(inputs, outputs, sampleFrames, ap->bitcrush);
        }
    }

    Program::Program() : ProgramParameters() {
        vst_strncpy(name, "Init", PLUGIN_PROGRAM_STR_MAX_LEN);
        bitcrush = BITCRUSH_BITS_MAX;
    }

    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new PluginVST2_Bitcrush(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> PluginVST2_Bitcrush::createView() {
        auto view = std::make_shared<SinglePluginViewContainers<guictr_vst2_simple, PluginVST2_Bitcrush>>(this);
        this->views.push_back(view);
        return view;
    }
}// namespace PluginBitcrush
