#include "gain-plugin.h"
#include "automation.h"
#include "dsp_util.h"
#include "event.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugincontrol.h"
#include "seq_time.h"
#include "str_util.h"
#include "gui/container/container.h"
#include "gui/controls/knoblabeled.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginctr.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "modules.h"
#include "host/mainctrl.h"
#include "host/plugin/internal_plugin.h"
#include "track.h"
#include "track_impl.h"
#include "audioblock.h"
#include "meter.h"
#include "snapshot.h"
#include "window.h"
#include "dsp_util.h"
#include <algorithm>
#include <vector>

namespace PluginGain {

    struct impl_data_t {
        std::vector<float> vecGain;
        std::vector<float> vecPanL;
        std::vector<float> vecPanR;
    };

    module_gain::module_gain(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("Gain", getModuleType(), _projectGlobalId, _hostCallback),
        impl(new impl_data_t)
    {
        struct effectgain_param_entry {
            int32_t id;
            String name;
            String unit;
            float val;
        };
        const std::array<effectgain_param_entry, 2> parameterTypes{ {
                { PARAM_GAIN, "Gain", "dB", dsp_util::gainToLinScaleWithRange(1.0f, MTR_CEIL, DBFS_MUTE_POS) },
                { PARAM_PAN,  "Pan",  "", 0.5f }
        } };
        for (const effectgain_param_entry& paramEntry : parameterTypes) {
            automatable_param_t* regparam = registerParam(paramEntry.id);
            regparam->defaultValue = paramEntry.val;
            regparam->value = paramEntry.val;
            regparam->name  = paramEntry.name;
            regparam->unit  = paramEntry.unit;
        }
    }
    module_gain::~module_gain() {
        delete impl;
    }

    void module_gain::process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert(in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);

        auto autParGain = getActiveAutomation(PARAM_GAIN);
        auto autParPan = getActiveAutomation(PARAM_PAN);
        out->clear();
        // fast path: no sample accurate automation
        if (!autParGain && !autParPan) {
            /* Calculate group gain level */
            float fGain = 1.0f;
            if (dsp_util::getGainLvlWithRange(getParamValue(PARAM_GAIN), MTR_CEIL, DBFS_MUTE_POS, fGain)) {
                // fast path: center pan
                if (math::abs(getParamValue(PARAM_PAN) - 0.5f) < 0.005f) {
                    out->addFromOp(in, AudioBlock::mix_op::ADD, fGain);
                } else {
                    DAW::Panning::MultiplyConstant(in, out, fGain * (1.0f/DAW::Panning::GetCenterGain()), getParamValue(PARAM_PAN));
                }
            } else {
                // fast path: fully muted
            }
            return;
        }
        impl->vecGain.resize(numSamples);
        impl->vecPanL.resize(numSamples);
        impl->vecPanR.resize(numSamples);
        const auto bpm100 = project_controller_t::get()->getCurrentTempo(); //TODO: use hostCallback or provide time info struct in process() parameter list
        const auto tickBegin = tick;
        const auto tickEnd = tickBegin + sampleToTickConvert<double, roundmode::none>(numSamples, bpm100, format.sampleRate);

        if (autParGain && autParGain->isActive()) {
            autParGain->sampleAutomation(tickBegin, tickEnd, numSamples, impl->vecGain.data());
        } else {
            std::fill(impl->vecGain.begin(), impl->vecGain.end(), getParamValue(PARAM_GAIN));
        }
        if (autParPan && autParPan->isActive()) {
            autParPan->sampleAutomation(tickBegin, tickEnd, numSamples, impl->vecPanL.data());
        } else {
            std::fill(impl->vecPanL.begin(), impl->vecPanL.end(), getParamValue(PARAM_PAN));
        }
        for (int32_t i = 0; i < numSamples; i++) {
            dsp_util::getGainLvlWithRange(impl->vecGain[i], MTR_CEIL, DBFS_MUTE_POS, impl->vecGain[i]);
            DAW::Panning::CalculatePanning<DAW::Panning::PanLaw::SIN_4_5DB>(impl->vecPanL[i], &impl->vecPanL[i], &impl->vecPanR[i]);
            impl->vecGain[i] *= 1.0f/DAW::Panning::GetCenterGain();
        }
        float* panningData[2] = { impl->vecPanL.data(), impl->vecPanR.data() };
        DAW::Panning::MultiplyAutomation(in, out, impl->vecGain.data(), panningData);

    }
    param_converted_t module_gain::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        if (idx == PARAM_GAIN) {
            if (fTextFieldVal <= DBFS_MUTE_POS + 1.0f)
                fTextFieldVal = 0.0f;
            if (fTextFieldVal > MTR_CEIL)
                fTextFieldVal = MTR_CEIL;
            float f_gain = pow(10.0f, fTextFieldVal / 20.0f);
            float f_linear = dsp_util::gainToLinScaleWithRange(f_gain, MTR_CEIL, DBFS_MUTE_POS);
            return {f_linear, true};
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }
    param_unit_t module_gain::convertParamValueToDisplay(int32_t idx, float value) {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->unit == "dB") {
            float fGain = 1.0f;
            if (dsp_util::getGainLvlWithRange(getParamValue(PARAM_GAIN), MTR_CEIL, DBFS_MUTE_POS, fGain)) {
                return {StringFormat("%.3f", dsp_util::dBFS(fGain)), param->unit};
            }
            return {"-INF", param->unit};
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }

    void module_gain::loadSnapshot(const plugin_snapshot_t& snapshot) {
        internalplugin::loadSnapshot(snapshot);
        if (snapshot.vendorVersion == 0) {
            /*  Convert old snapshot format */
            auto& params = snapshot.params;
            auto it = std::find_if(params.begin(), params.end(), [](const auto& p) {
                return p.idx == PARAM_GAIN;
            });
            if (it != params.end()) {
                float val = it->val;
                val = dsp_util::linScaleToGainWithRange(val, 6.0f, -81.0f);
                val = dsp_util::gainToLinScaleWithRange(val, MTR_CEIL, DBFS_MUTE_POS);
                setParamValue(PARAM_GAIN, val, FLG_PAR_UPDATE_INIT | FLG_PAR_UPDATE_NOSTORE);
            }
        }
    }

    void module_gain::makeSnapshot(plugin_snapshot_t& snapshot, const tracksnapshot_store_opts_t& opts) {
        internalplugin::makeSnapshot(snapshot, opts);
        snapshot.vendorVersion = 1;
    }

    std::shared_ptr<PluginViewContainers> module_gain::createViewCtrInternal() {
        return std::make_shared<SinglePluginViewContainers<guictr_vst2_simple, module_gain>>(this, 100, 150);
    }
} // namespace PluginGain

template<>
effectbase* makeInstance<PluginGain::module_gain>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginGain::module_gain(_projectGlobalId, _hostCallback);
}
