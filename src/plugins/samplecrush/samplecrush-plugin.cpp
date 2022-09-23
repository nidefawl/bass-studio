#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include "assert_dbg.h"
#include "audioblock.h"
#include "config.h"
#include "dsp_util.h"
#include "dsp_util.h"
#include "event.h"
#include "gui/container/container.h"
#include "gui/controls/knoblabeled.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginctr.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "host/mainctrl.h"
#include "host/plugin/internal_plugin.h"
#include "logging.h"
#include "math/seq_math.h"
#include "meter.h"
#include "modules.h"
#include "platform.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugin-window.h"
#include "plugins/plugin.h"
#include "plugins/plugincontrol.h"
#include "samplecrush-plugin.h"
#include "snapshot.h"
#include "str_util.h"
#include "str_util.h"
#include "track_impl.h"
#include "track.h"
#include "window.h"

namespace PluginSampleCrush {
    int32_t convertToBits(float value) {
        return math::clamp<int32_t>(math::floorfS32(value * (BITCRUSH_BITS_MAX - BITCRUSH_BITS_MIN) + BITCRUSH_BITS_MIN), BITCRUSH_BITS_MIN, BITCRUSH_BITS_MAX);
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



    static void processSampleCrush(float** inputs, float** outputs, VstInt32 sampleFrames, const int32_t sampleCrushLevel) {
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

    static constexpr int32_t PARAM_NUM_SAMPLES = 1;

    module_samplecrush::module_samplecrush(int32_t _projectGlobalId, i_host_callback* _hostCallback)
        : internalplugin("Sample Crush", getModuleType(), _projectGlobalId, _hostCallback)
    {
        struct effectgain_param_entry {
            int32_t id;
            String name;
            String unit;
            float val;
        };
        const std::array<effectgain_param_entry, 1> parameterTypes{ {
            { PARAM_NUM_SAMPLES, "#Samples", "samples",  0.0f }
        } };
        for (const effectgain_param_entry& paramEntry : parameterTypes) {
            automatable_param_t* regparam = registerParam(paramEntry.id);
            regparam->defaultValue = paramEntry.val;
            regparam->value = paramEntry.val;
            regparam->name  = paramEntry.name;
            regparam->unit  = paramEntry.unit;
        }
    }

    module_samplecrush::~module_samplecrush() {
        delete blockInputs;
        delete blockOutputs;
    }

    void module_samplecrush::postSetParameter(int32_t idx, float preVal, float val, int flags) {
        switch (idx) {
            case PARAM_NUM_SAMPLES:
                // setNewLatency(math::clamp(math::roundfS32(val * MAX_LATENCY), 0, MAX_LATENCY));
                break;
        }
        internalplugin::postSetParameter(idx, preVal, val, flags);
    }

    void module_samplecrush::process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert( in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);
        out->clear();
        dbgassert(in->channels >= 2 && out->channels >= 2);
        processSampleCrush(in->buf, out->buf, numSamples, convertToBits(getParamValue(PARAM_NUM_SAMPLES)));
    }

    param_converted_t module_samplecrush::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        switch (idx) {
            case PARAM_NUM_SAMPLES: {
                int nSamples = math::roundfS32(fTextFieldVal);
                int pow2 = math::roundfS32(std::log2f(nSamples));
                float fPow = pow2 / static_cast<float>(BITCRUSH_BITS_MAX - BITCRUSH_BITS_MIN);
                return {math::clamp(fPow, 0.0f, 1.0f), true};
            }
            default:
                break;
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }

    param_unit_t module_samplecrush::getParamValueDisplay(int32_t idx) {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->idx == PARAM_NUM_SAMPLES) {
            auto nPow2 = (1 << convertToBits(param->value));
            return {StringFormat("%d", nPow2), param->unit};
        }
        return internalplugin::getParamValueDisplay(idx);
    }

    void module_samplecrush::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
        meterIn.update(this->blockInputs, 1.0f);
        meter.update(out, 1.0f);
    }

    void module_samplecrush::loadSnapshot(const plugin_snapshot_t& snapshot) {
        internalplugin::loadSnapshot(snapshot);
    }

    void module_samplecrush::makeSnapshot(plugin_snapshot_t& snapshot, const tracksnapshot_store_opts_t& opts) {
        internalplugin::makeSnapshot(snapshot, opts);
        snapshot.vendorVersion = 1;
    }

    std::shared_ptr<PluginViewContainers> module_samplecrush::createInternalView() {
        if (!views.empty()) {
            for (auto& existingView : views) {
                if (!existingView->isInUse()) {
                    existingView->setUsed();
                    return existingView;
                }
            }
        }
        auto v = std::make_shared<SinglePluginViewContainers<guictr_vst2_simple, module_samplecrush>>(this, 100, 150);
        this->views.push_back(v);
        return v;
    }

    samplecount_t module_samplecrush::getPluginLatency() {
        return 0;
    }

    void module_samplecrush::onEnable() {
    }

} // namespace PluginSampleCrush

template<>
effectbase* makeInstance<PluginSampleCrush::module_samplecrush>(int32_t _projectGlobalId, i_host_callback* _hostCallback) {
    return new PluginSampleCrush::module_samplecrush(_projectGlobalId, _hostCallback);
}

#if 0
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
#include "audioblock.h"
#include <vstsdk-plugin-2.4/audioeffectx.h>

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginSampleCrush::createPlugin(audioMaster);
}
#endif

namespace PluginSampleCrush {
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
}// namespace PluginSampleCrush
#endif