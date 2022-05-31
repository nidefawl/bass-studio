#include <cmath>
#include <algorithm>
#include <cstdio>
#include <memory>
#include "assert_dbg.h"
#include "config.h"
#include "logging.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "dsp_util.h"

#include "platform.h"

#include "../plugin.h"
#include "sampledelay-plugin.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"

#define PLUGIN_EFFECT_NAME "SampleDelay"
#define PLUGIN_UID "STWD"
#define PLUGIN_PRODUCT_NAME "sample delay VST2.x "


#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginSampleDelay::createPlugin(audioMaster);
}
#endif


namespace PluginSampleDelay {
    samplecount_t convertToSamples(float value) {
        return math::clamp<samplecount_t>(math::floorfS32(MIN_DELAY + value * (MAX_DELAY - MIN_DELAY)), MIN_DELAY, MAX_DELAY);
    }

    PluginVST2_SampleDelay::PluginVST2_SampleDelay(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs) {
        curProgram = 0;
        setInitialDelay(-MIN_DELAY);
        // setInitialDelay(0);
    }

    void PluginVST2_SampleDelay::setProgram(VstInt32 program) {
        if (program < 0 || program >= kNumPrograms)
            return;
        curProgram = program;
    }

    void PluginVST2_SampleDelay::setProgramName(char* name) {
    }

    void PluginVST2_SampleDelay::getProgramName(char* name) {
        if (name)
            name[0] = 0;
    }

    void PluginVST2_SampleDelay::getParameterLabel(VstInt32 index, char* label) {
        switch (index) {
            case kSampleDelay:
                vst_strncpy(label, "samples", kVstMaxParamStrLen);
                return;
            default:
                vst_strncpy(label, "", kVstMaxParamStrLen);
        }
    }

    void PluginVST2_SampleDelay::getParameterDisplay(VstInt32 index, char* text) {
        text[0] = 0;
        switch (index) {
            case kSampleDelay: {
                auto delaySamples = convertToSamples(current()->delay);
                snprintf(text, kVstMaxParamStrLen, "%zd", delaySamples);
                break;
            }
        }
    }

    void PluginVST2_SampleDelay::getParameterName(VstInt32 index, char* label) {
        switch (index) {
            case kSampleDelay:
                vst_strncpy(label, "Delay", kVstMaxParamStrLen);
                return;
        }
    }

    void PluginVST2_SampleDelay::setParameter(VstInt32 index, float value) {
        BaseVST2_ProgramSampleDelay* ap = current();
        switch (index) {
            case kSampleDelay:
                ap->delay = value;
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

    float PluginVST2_SampleDelay::getParameter(VstInt32 index) {
        BaseVST2_ProgramSampleDelay* ap = current();
        float value                     = 0;
        switch (index) {
            case kSampleDelay:
                value = ap->delay;
                break;
        }
        return value;
    }

    bool PluginVST2_SampleDelay::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) {
        if (index >= 0 && index < kNumPrograms) {
            vst_strncpy(text, "Default", kVstMaxProgNameLen);
            return true;
        }
        return false;
    }

    bool PluginVST2_SampleDelay::getEffectName(char* name) {
        vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
        return true;
    }

    bool PluginVST2_SampleDelay::getVendorString(char* text) {
        vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
        return true;
    }

    bool PluginVST2_SampleDelay::getProductString(char* text) {
        vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
        return true;
    }

    VstInt32 PluginVST2_SampleDelay::getVendorVersion() {
        return 1;
    }

    VstInt32 PluginVST2_SampleDelay::canDo(char* text) {
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

    void PluginVST2_SampleDelay::processStereo(float** inputs, AudioBlock& out, VstInt32 sampleFrames, const float filterCoeff, BaseVST2_ProgramSampleDelay& params, const BaseVST2_ProgramSampleDelay nextParams) {
        float* out1 = out.buf[0];
        float* out2 = out.buf[1];

        if (!this->delayLine) {
            this->delayLine = std::make_unique<DelayLine>();
        }
        AudioBlock inputBlock(inputs, 2, sampleFrames);
        constexpr auto DELAYLINE_SIZE = (MAX_DELAY - MIN_DELAY);
        delayLine->write(&inputBlock, DELAYLINE_SIZE * 2);
        auto& delayBlock = delayLine->getBlock();
        dbgassert(DELAYLINE_SIZE <= delayBlock.samples);
        const auto writeOffset = delayLine->getWriteOffset();
        for (samplecount_t smpPos = 0; smpPos < sampleFrames; smpPos++) {
            updateParam(params.delay, nextParams.delay, filterCoeff);
            auto delay = math::clamp<samplecount_t>(math::floorfS32(params.delay * DELAYLINE_SIZE), 0, DELAYLINE_SIZE-1);
            dbgassert(delay >= 0 && delay < DELAYLINE_SIZE);
            auto readPos = writeOffset - delay;
            readPos += smpPos;
            if (readPos < 0) {
                readPos += delayBlock.samples;
            }
            if (readPos >= delayBlock.samples) {
                readPos -= delayBlock.samples;
            }
            dbgassert(readPos >= 0);
            dbgassert(readPos + 1 <= delayBlock.samples);
            (*out1++)  = *(delayBlock.buf[0] + readPos);
            (*out2++)  = *(delayBlock.buf[1] + readPos);
        }
    }

    void PluginVST2_SampleDelay::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
        if (issetprogram)
            return;

        if (sampleFrames != blockSize) {
            return;
        }
        BaseVST2_ProgramSampleDelay* ap = current();
        if (this->getAeffect()->numOutputs == 2) {
            float fBlockFreq  = (sampleRate / blockSize) * 0.0015f;
            float filterCoeff = 1.0f - expf(-2.0f * M_PIf * (fBlockFreq / sampleRate));
            AudioBlock outputBlock(outputs, 2, sampleFrames);
            processStereo(inputs, outputBlock, sampleFrames, filterCoeff, paramsState, *ap);
        }
    }

    BaseVST2_ProgramSampleDelay::BaseVST2_ProgramSampleDelay() : ProgramParameters() {
        vst_strncpy(name, "Init", kVstMaxProgNameLen);
        delay  = 0.5f;
    }

    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
}// namespace PluginSampleDelay
