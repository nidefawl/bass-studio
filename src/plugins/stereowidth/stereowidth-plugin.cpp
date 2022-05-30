#include <cmath>
#include <algorithm>
#include <cstdio>
#include <memory>
#include "config.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "dsp_util.h"

#include "platform.h"

#include "../plugin.h"
#include "stereowidth-plugin.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"

#define PLUGIN_EFFECT_NAME "StereoWidth"
#define PLUGIN_UID "STWD"
#define PLUGIN_PRODUCT_NAME "stereo width VST2.x "


#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginStereoWidth::createPlugin(audioMaster);
}
#endif


namespace PluginStereoWidth {

    PluginVST2_StereoWidth::PluginVST2_StereoWidth(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs) {
        curProgram = 0;
    }

    void PluginVST2_StereoWidth::setProgram(VstInt32 program) {
        if (program < 0 || program >= kNumPrograms)
            return;
        curProgram = program;
    }

    void PluginVST2_StereoWidth::setProgramName(char* name) {
    }

    void PluginVST2_StereoWidth::getProgramName(char* name) {
        if (name)
            name[0] = 0;
    }

    void PluginVST2_StereoWidth::getParameterLabel(VstInt32 index, char* label) {
        switch (index) {
            case kStereoWidth:
                label[0] = '%';
                label[1] = 0;
                return;
            case kGain:
                vst_strncpy(label, "dB", kVstMaxParamStrLen);
                return;
            default:
                vst_strncpy(label, "", kVstMaxParamStrLen);
        }
    }

    void PluginVST2_StereoWidth::getParameterDisplay(VstInt32 index, char* text) {
        text[0] = 0;
        switch (index) {
            case kStereoWidth: {
                snprintf(text, kVstMaxParamStrLen, "%.0f", current()->width * 200.0f);
                break;
            }
            case kGain: {
                dB2string(current()->gain * 2.0f, text, kVstMaxParamStrLen);
                break;
            }
        }
    }

    void PluginVST2_StereoWidth::getParameterName(VstInt32 index, char* label) {
        switch (index) {
            case kStereoWidth:
                vst_strncpy(label, "Width", kVstMaxParamStrLen);
                return;
            case kGain:
                vst_strncpy(label, "Gain", kVstMaxParamStrLen);
                return;
        }
    }

    void PluginVST2_StereoWidth::setParameter(VstInt32 index, float value) {
        BaseVST2_ProgramStereoWidth* ap = current();
        switch (index) {
            case kStereoWidth:
                ap->width = value;
                break;
            case kGain:
                ap->gain = value;
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

    float PluginVST2_StereoWidth::getParameter(VstInt32 index) {
        BaseVST2_ProgramStereoWidth* ap = current();
        float value                     = 0;
        switch (index) {
            case kStereoWidth:
                value = ap->width;
                break;
            case kGain:
                value = ap->gain;
                break;
        }
        return value;
    }

    bool PluginVST2_StereoWidth::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) {
        if (index >= 0 && index < kNumPrograms) {
            vst_strncpy(text, "Default", kVstMaxProgNameLen);
            return true;
        }
        return false;
    }

    bool PluginVST2_StereoWidth::getEffectName(char* name) {
        vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
        return true;
    }

    bool PluginVST2_StereoWidth::getVendorString(char* text) {
        vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
        return true;
    }

    bool PluginVST2_StereoWidth::getProductString(char* text) {
        vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
        return true;
    }

    VstInt32 PluginVST2_StereoWidth::getVendorVersion() {
        return 1;
    }

    VstInt32 PluginVST2_StereoWidth::canDo(char* text) {
        if (!strcmp(text, "receiveVstTimeInfo"))
            return 1;
        return -1;// explicitly can't do; 0 => don't know
    }

    template<typename T>
    inline void updateParam(T& cur, const T& next, const T filterCoeff) {
        T delta = next - cur;
        if (math::abs(delta) < math::F_MIN) {
            cur = next;
        } else {
            cur += filterCoeff * delta;
        }
    }

    static void processStereo(float** inputs, float** outputs, VstInt32 sampleFrames, const float filterCoeff, BaseVST2_ProgramStereoWidth& params, const BaseVST2_ProgramStereoWidth nextParams) {
        float* out1 = outputs[0];
        float* out2 = outputs[1];
        float* in1  = inputs[0];
        float* in2  = inputs[1];
        for (int a = 0; a < sampleFrames; a++) {
            updateParam(params.gain, nextParams.gain, filterCoeff);
            updateParam(params.width, nextParams.width, filterCoeff);
            float gain        = params.gain * 2.0f;
            float width       = params.width;
            float scaleMono   = 1.0f - math::max(0.0f, (width - 0.5f) * 2.0f);
            float scaleStereo = math::min(1.0f, width * 2.0f);
            float channelL    = (*in1++);
            float channelR    = (*in2++);
            float stereo      = (channelL - channelR) / 2.0f;
            float mono        = (channelL + channelR) / 2.0f;
            stereo *= scaleStereo;
            mono *= scaleMono;
            float outL = mono + stereo;
            float outR = mono - stereo;
            (*out1++)  = outL * gain;
            (*out2++)  = outR * gain;
        }
    }

    void PluginVST2_StereoWidth::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
        if (issetprogram)
            return;

        if (sampleFrames != blockSize) {
            return;
        }
        BaseVST2_ProgramStereoWidth* ap = current();
        if (this->getAeffect()->numOutputs == 2) {
            float fBlockFreq  = (sampleRate / blockSize) * 0.45f;
            float filterCoeff = 1.0f - expf(-2.0f * M_PI * (fBlockFreq / sampleRate));
            //filterCoeff = 1.0f;
            processStereo(inputs, outputs, sampleFrames, filterCoeff, paramsState, *ap);
        }
    }

    BaseVST2_ProgramStereoWidth::BaseVST2_ProgramStereoWidth() : ProgramParameters() {
        vst_strncpy(name, "Init", kVstMaxProgNameLen);
        gain  = 1.0f;
        width = 0.5f;
    }

    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
}// namespace PluginStereoWidth
