#include <cmath>
#include <algorithm>
#include <cstdio>
#include <memory>
#include "config.h"
#include "str_util.h"
#include "dsp_util.h"

#include "platform.h"

#include "../plugin.h"
#include "adv-plugin.h"
#include "../../gui/pluginviewcontainers.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"

#define PLUGIN_EFFECT_NAME "NoneAdv"
#define PLUGIN_VENDOR_NAME "MichaelH"
#define PLUGIN_UID "AGTP"//advanced gui test plugin
#define PLUGIN_PRODUCT_NAME "advanced gui test plugin VST2.x "

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginTestAdv::createPlugin(audioMaster);
}
#endif

namespace PluginTestAdv {

    GuiAdvPluginVST2::GuiAdvPluginVST2(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs) {
        programs[0].latency       = 0.3f;
        programs[0].noiseVolume   = 0.1f;
        programs[0].reportLatency = true;

        curProgram = 0;
    }

    void GuiAdvPluginVST2::setProgram(VstInt32 program) {
        if (program < 0 || program >= kNumPrograms)
            return;
        curProgram = program;
    }

    void GuiAdvPluginVST2::setProgramName(char* name) {
    }

    void GuiAdvPluginVST2::getProgramName(char* name) {
        if (name != NULL && curProgram >= 0)
            vst_strncpy(name, programs[curProgram].name, kVstMaxProgNameLen);
    }

    void GuiAdvPluginVST2::getParameterLabel(VstInt32 index, char* label) {
        vst_strncpy(label, "", kVstMaxParamStrLen);
    }

    void GuiAdvPluginVST2::getParameterDisplay(VstInt32 index, char* text) {
        text[0] = 0;
        switch (index) {
            case kNoiseVolume: {
                snprintf(text, kVstMaxParamStrLen, "%.2f", current()->noiseVolume);
            }
        }
    }

    void GuiAdvPluginVST2::getParameterName(VstInt32 index, char* label) {
        switch (index) {
            case kNoiseVolume:
                vst_strncpy(label, "Noise level", kVstMaxParamStrLen);
                return;
        }
    }

    void GuiAdvPluginVST2::setParameter(VstInt32 index, float value) {
        BaseVST2_Program* ap = current();
        switch (index) {
            case kNoiseVolume:
                ap->noiseVolume = value;
                break;
        }
    }

    float GuiAdvPluginVST2::getParameter(VstInt32 index) {
        BaseVST2_Program* ap = current();
        float value          = 0;
        switch (index) {
            case kNoiseVolume:
                value = ap->noiseVolume;
                break;
        }
        return value;
    }

    bool GuiAdvPluginVST2::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) {
        if (index >= 0 && index < kNumPrograms) {
            vst_strncpy(text, programs[index].name, kVstMaxProgNameLen);
            return true;
        }
        return false;
    }

    bool GuiAdvPluginVST2::getEffectName(char* name) {
        vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
        return true;
    }

    bool GuiAdvPluginVST2::getVendorString(char* text) {
        vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
        return true;
    }

    bool GuiAdvPluginVST2::getProductString(char* text) {
        vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
        return true;
    }

    VstInt32 GuiAdvPluginVST2::getVendorVersion() {
        return 2;
    }

    VstInt32 GuiAdvPluginVST2::canDo(char* text) {
        if (!strcmp(text, "receiveVstEvents"))
            return 1;
        if (!strcmp(text, "receiveVstTimeInfo"))
            return 1;
        return -1;// explicitly can't do; 0 => don't know
    }

    static void fillNoiseMono(float** inputs, float** outputs, VstInt32 sampleFrames, float noiseGain) {
        float* out1 = outputs[0];
        float* in1  = inputs[0];
        for (int a = 0; a < sampleFrames; a++) {
            float random = (float) (rand() - rand());
            random       = random / 32767.f * noiseGain;
            (*out1++)    = (*in1++) + random;
        }
    }
    static void fillNoiseStereo(float** inputs, float** outputs, VstInt32 sampleFrames, float noiseGain) {
        float* out1 = outputs[0];
        float* out2 = outputs[1];
        float* in1  = inputs[0];
        float* in2  = inputs[1];
        for (int a = 0; a < sampleFrames; a++) {
            float random = (float) (rand() - rand());
            random       = random / 32767.f * noiseGain;
            (*out1++)    = (*in1++) + random;
            (*out2++)    = (*in2++) + random;
        }
    }
    void GuiAdvPluginVST2::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
        if (issetprogram)
            return;

        if (sampleFrames != blockSize) {
            return;
        }
        BaseVST2_Program* ap = current();
        float noiseGain      = dsp_util::clampGain(ap->noiseVolume);
        if (this->getAeffect()->numOutputs == 1) {
            fillNoiseMono(inputs, outputs, sampleFrames, noiseGain);
        } else if (this->getAeffect()->numOutputs == 2) {
            fillNoiseStereo(inputs, outputs, sampleFrames, noiseGain);
        } else {
            //log_printf("unsupported this->getAeffect()->numOutputs");
        }
    }


    BaseVST2_Program::BaseVST2_Program() : ProgramParameters() {
        vst_strncpy(name, "Init", kVstMaxProgNameLen);
        latency       = 0.3f;
        noiseVolume   = 0.1f;
        reportLatency = true;
    }


    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
}
