#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include "assert_dbg.h"
#include "host/audiobuffer/audioblock.h"
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
#include "host/daw/mainctrl.h"
#include "host/plugin/internal/internal-plugin.h"
#include "logging.h"
#include "math/seq_math.h"
#include "host/meter/meter.h"
#include "host/plugin/modules.h"
#include "platform.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugin-window.h"
#include "plugins/plugin.h"
#include "plugins/plugincontrol.h"
#include "samplecrush-plugin.h"
#include "snapshot/snapshot.h"
#include "str_util.h"
#include "str_util.h"
#include "host/track/track_impl.h"
#include "host/track/track.h"
#include "window.h"

namespace PluginSampleCrush {
    int32_t convertToSamples(float value) {
        return math::clamp<int32_t>(math::floorfS32(value * (BITCRUSH_BITS_MAX - BITCRUSH_BITS_MIN) + BITCRUSH_BITS_MIN), BITCRUSH_BITS_MIN, BITCRUSH_BITS_MAX);
    }
    
    int32_t convertToBits(float value) {
        return math::clamp<int32_t>(math::floorfS32(value * BITCRUSH_BITS_MAX), BITCRUSH_BITS_MIN, BITCRUSH_BITS_MAX);
    }
    
    int32_t convertToMode(float value) {
        return math::clamp<int32_t>(math::floorfS32(value * (BITCRUSH_MODE_MAX - BITCRUSH_MODE_MIN) + BITCRUSH_MODE_MIN), BITCRUSH_MODE_MIN, BITCRUSH_MODE_MAX);
    }

    static void processSampleHardClip(float** inputs, float** outputs, VstInt32 sampleFrames, const int32_t sampleCrushLevel) {
        float* out1 = outputs[0];
        float* out2 = outputs[1];
        float* in1  = inputs[0];
        float* in2  = inputs[1];
        int steps   = 1 << (sampleCrushLevel);
        if (steps <= 1) {
            for (int a = 0; a < sampleFrames; ++a) {
                (*out1++) = (*in1++) < 0 ? -1 : 1;
                (*out2++) = (*in2++) < 0 ? -1 : 1;
            }
        } else {
            while (sampleFrames > 0) {
                float accL = 0;
                float accR = 0;
                int num = 0;
                while (num < steps && sampleFrames > 0) {
                    accL += (*in1++);
                    accR += (*in2++);
                    ++num;
                    --sampleFrames;
                }
                for (int b = 0; b < num; ++b) {
                    (*out1++) = (accL) < 0 ? -1 : 1;
                    (*out2++) = (accR) < 0 ? -1 : 1;
                }
            }
        }
    }

    static void processSampleSampleCrush(float** inputs, float** outputs, VstInt32 sampleFrames, const int32_t sampleCrushLevel) {
        float* out1 = outputs[0];
        float* out2 = outputs[1];
        float* in1  = inputs[0];
        float* in2  = inputs[1];
        int steps   = 1 << (sampleCrushLevel);
        if (steps <= 1) {
            for (int a = 0; a < sampleFrames; ++a) {
                (*out1++) = (*in1++);
                (*out2++) = (*in2++);
            }
        } else {
            while (sampleFrames > 0) {
                float accL = 0;
                float accR = 0;
                int num = 0;
                while (num < steps && sampleFrames > 0) {
                    accL += (*in1++);
                    accR += (*in2++);
                    ++num;
                    --sampleFrames;
                }
                accL /= num;
                accR /= num;
                for (int b = 0; b < num; ++b) {
                    *out1++ = accL;
                    *out2++ = accR;
                }
            }
        }
    }
    static void processSampleBitCrush(float** inputs, float** outputs, VstInt32 sampleFrames, const int32_t sampleCrushLevel) {
        //classic bitcrush algorithm (convert to int, quantize, then back to float)
        float* out1 = outputs[0];
        float* out2 = outputs[1];
        float* in1  = inputs[0];
        float* in2  = inputs[1];
        int bits = 1 << (sampleCrushLevel);
        for (int a = 0; a < sampleFrames; ++a) {
            (*out1++) = (float)((int)((*in1++) * (float)(bits - 1)) / (float)(bits - 1));
            (*out2++) = (float)((int)((*in2++) * (float)(bits - 1)) / (float)(bits - 1));
        }
    }

    static constexpr int32_t PARAM_NUM_SAMPLES = 1;
    static constexpr int32_t PARAM_CRUSH_MODE  = 2;

    module_samplecrush::module_samplecrush(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("Sample Crush", getModuleType(), _projectGlobalId, _hostCallback)
    {
        struct effectgain_param_entry {
            int32_t id;
            String name;
            String unit;
            float val;
        };
        const std::array<effectgain_param_entry, 2> parameterTypes{ {
            { PARAM_NUM_SAMPLES, "#Samples", "samples",  0.0f },
            { PARAM_CRUSH_MODE, "Mode", "",  0.0f }
        } };
        for (const auto& paramEntry : parameterTypes) {
            registerParam(paramEntry.id)->initValue(paramEntry);
        }
    }

    void module_samplecrush::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert( in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);
        out->clear();
        dbgassert(in->channels >= 2 && out->channels >= 2);
        int mode = convertToMode(getParamValue(PARAM_CRUSH_MODE));
        if (mode == 0) {
            processSampleSampleCrush(in->buf, out->buf, numSamples, convertToSamples(getParamValue(PARAM_NUM_SAMPLES)));
        } else if (mode == 1) {
            processSampleBitCrush(in->buf, out->buf, numSamples, convertToBits(getParamValue(PARAM_NUM_SAMPLES)));
        } else {
            processSampleHardClip(in->buf, out->buf, numSamples, convertToSamples(getParamValue(PARAM_NUM_SAMPLES)));
        }
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
            case PARAM_CRUSH_MODE: {
                int mode = math::roundfS32(fTextFieldVal);
                float fMode = mode / static_cast<float>(3);
                return {math::clamp(fMode, 0.0f, 1.0f), true};
            }
            default:
                break;
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }

    param_unit_t module_samplecrush::convertParamValueToDisplay(int32_t idx, float value) {
        auto param = getParam(idx);
        dbgassert(param);
        // if (param->idx == PARAM_NUM_SAMPLES) {
        //     auto nPow2 = (1 << convertToBits(value));
        //     return {StringFormat("%d", nPow2), param->unit};
        // }
        if (param->idx == PARAM_NUM_SAMPLES) {
            switch (convertToMode(getParamValue(PARAM_CRUSH_MODE))) {
                case 0:
                    return {StringFormat("%d", convertToSamples(value)), "Samples"};
                case 1:
                    return {StringFormat("%d", convertToSamples(value)), "Bits"};
                case 2:
                    return {StringFormat("%d", convertToSamples(value)), "Samples"};
                default:
                    break;
            }
        }
        if (param->idx == PARAM_CRUSH_MODE) {
            int mode = convertToMode(getParamValue(PARAM_CRUSH_MODE));
            switch (mode) {
                case 0:
                    return {"Sample Crush", param->unit};
                case 1:
                    return {"Bit Crush", param->unit};
                case 2:
                    return {"Hard Clip", param->unit};
                default:
                    break;
            }
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }

    std::shared_ptr<PluginViewContainers> module_samplecrush::createViewCtrInternal() {
        return std::make_shared<SinglePluginViewContainers<guictr_vst2_simple, module_samplecrush>>(this, 100, 150);
    }
} // namespace PluginSampleCrush

template<>
effectbase* makeInstance<PluginSampleCrush::module_samplecrush>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginSampleCrush::module_samplecrush(_projectGlobalId, _hostCallback);
}
