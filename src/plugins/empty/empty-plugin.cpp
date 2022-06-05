#include <cmath>
#include <algorithm>
#include <cstdio>
#include <memory>
#include "config.h"
#include "str_util.h"
#include "dsp_util.h"

#include "platform.h"

#include "../plugin.h"
#include "empty-plugin.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "logging.h"
#ifdef _MSC_VER
#include <windows.h>
#endif

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginEmptyVST2::createPlugin(audioMaster);
}
#endif

namespace PluginEmptyVST2 {
#define PLUGIN_BUILD_CRASHVERSION

#if defined(PLUGIN_BUILD_CRASHVERSION)
    const char* const PLUGIN_EFFECT_NAME = "CrashVST2x";
#else
    const char* const PLUGIN_EFFECT_NAME = "Empty";
#endif
    const char* const PLUGIN_UID = "EMPT";
    const char* const PLUGIN_PRODUCT_NAME = "empty test plugin VST2.x";
    EmptyPluginVST2::EmptyPluginVST2(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs) {
        curProgram = 0;
    }

    void EmptyPluginVST2::setProgram(VstInt32 program) {
        if (program < 0 || program >= kNumPrograms)
            return;
        curProgram = program;
    }

    void EmptyPluginVST2::setProgramName(char* name) {
    }

    void EmptyPluginVST2::getProgramName(char* name) {
        if (name && curProgram >= 0)
            vst_strncpy(name, programs[curProgram].name, PLUGIN_PROGRAM_STR_MAX_LEN);
    }

    void EmptyPluginVST2::getParameterLabel(VstInt32 index, char* label) {
        vst_strncpy(label, "", PLUGIN_PARAM_STR_MAX_LEN);
    }

    void EmptyPluginVST2::getParameterDisplay(VstInt32 index, char* text) {
        text[0] = 0;
    }

    void EmptyPluginVST2::getParameterName(VstInt32 index, char* label) {
    }

    void EmptyPluginVST2::setParameter(VstInt32 index, float value) {
    }

    float EmptyPluginVST2::getParameter(VstInt32 index) {
        return 0;
    }

    bool EmptyPluginVST2::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) {
        if (index >= 0 && index < kNumPrograms) {
            vst_strncpy(text, programs[index].name, PLUGIN_PROGRAM_STR_MAX_LEN);
            return true;
        }
        return false;
    }

    bool EmptyPluginVST2::getEffectName(char* name) {
        vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
        return true;
    }

    bool EmptyPluginVST2::getVendorString(char* text) {
        vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
        return true;
    }

    bool EmptyPluginVST2::getProductString(char* text) {
        vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
        return true;
    }

    VstInt32 EmptyPluginVST2::getVendorVersion() {
        return 2;
    }

    VstInt32 EmptyPluginVST2::canDo(char* text) {
        if (!strcmp(text, "receiveVstEvents"))
            return 1;
        if (!strcmp(text, "receiveVstTimeInfo"))
            return 1;
        return -1;// explicitly can't do; 0 => don't know
    }

    void EmptyPluginVST2::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
        numCalls++;
        if (issetprogram)
            return;

        if (sampleFrames != blockSize) {
            return;
        }
        if (this->getAeffect()->numOutputs == 1) {
            if (inputs)
                memset(inputs[0], 0, sizeof(float) * sampleFrames);
            memset(outputs[0], 0, sizeof(float) * sampleFrames);
        } else if (this->getAeffect()->numOutputs == 2) {
            if (inputs)
                dsp_util::fillChannels(inputs, this->getAeffect()->numInputs, sampleFrames, 0.0f);
            dsp_util::fillChannels(outputs, this->getAeffect()->numOutputs, sampleFrames, 0.0f);
#if defined(PLUGIN_BUILD_CRASHVERSION)
            //log_printf("producing segfault\n");
            int64_t* ptr = nullptr;
            ptr          = static_cast<int64_t*>((void*) 0xBAADF00D);
            int64_t val  = *ptr;
            log_printf("val = %zd WTF\n", val);
#endif
        }
        numCalls2++;
    }


    BaseVST2_Program::BaseVST2_Program() : ProgramParameters() {
        vst_strncpy(name, "Init", PLUGIN_PROGRAM_STR_MAX_LEN);
    }


    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
}// namespace PluginEmptyVST2
