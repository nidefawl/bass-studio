#include <cmath>
#include <algorithm>
#include <cstdio>
#include <memory>
#include "audioblock.h"
#include "config.h"
#include "str_util.h"
#include "dsp_util.h"
#include "platform.h"
#include "../plugin.h"
#include "adv-plugin.h"
#include "gui/plugin/pluginviewcontainers.h"
#include <vstsdk-plugin-2.4/audioeffectx.h>

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginTestAdv::createPlugin(audioMaster);
}
#endif

namespace PluginTestAdv {
    const char* const PLUGIN_EFFECT_NAME = "NoneAdv";
    const char* const PLUGIN_UID = "AGTP";//advanced gui test plugin
    const char* const PLUGIN_PRODUCT_NAME = "advanced gui test plugin VST2.x";

    GuiAdvPluginVST2::GuiAdvPluginVST2(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs) {
        programs[0].latency       = 0.3f;
        programs[0].noiseVolume   = 0.1f;
        programs[0].reportLatency = true;

        curProgram = 0;
        rnd.rng_seed(87654323);
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
            vst_strncpy(name, programs[curProgram].name, PLUGIN_PROGRAM_STR_MAX_LEN);
    }

    void GuiAdvPluginVST2::getParameterLabel(VstInt32 index, char* label) {
        vst_strncpy(label, "", PLUGIN_PARAM_STR_MAX_LEN);
    }

    void GuiAdvPluginVST2::getParameterDisplay(VstInt32 index, char* text) {
        text[0] = 0;
        switch (index) {
            case kNoiseVolume: {
                snprintf(text, PLUGIN_PARAM_STR_MAX_LEN, "%.2f", current()->noiseVolume);
            }
        }
    }

    void GuiAdvPluginVST2::getParameterName(VstInt32 index, char* label) {
        switch (index) {
            case kNoiseVolume:
                vst_strncpy(label, "Noise level", PLUGIN_PARAM_STR_MAX_LEN);
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
            vst_strncpy(text, programs[index].name, PLUGIN_PROGRAM_STR_MAX_LEN);
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

    void GuiAdvPluginVST2::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
        if (issetprogram)
            return;

        if (sampleFrames != blockSize) {
            return;
        }
        BaseVST2_Program* ap = current();
        float noiseGain      = dsp_util::clampGain(ap->noiseVolume);
        AudioBlock block(outputs, this->getAeffect()->numOutputs, sampleFrames);
        block.fillNoise(rnd, noiseGain);
    }


    BaseVST2_Program::BaseVST2_Program() : ProgramParameters() {
        vst_strncpy(name, "Init", PLUGIN_PROGRAM_STR_MAX_LEN);
        latency       = 0.3f;
        noiseVolume   = 0.1f;
        reportLatency = true;
    }


    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
}
