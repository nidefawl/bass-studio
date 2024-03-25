#include "gain-plugin.h"
#include "host/automation/automation.h"
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
#include "host/plugin/modules.h"
#include "host/daw/mainctrl.h"
#include "host/plugin/internal/internal-plugin.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "host/audiobuffer/audioblock.h"
#include "host/meter/meter.h"
#include "snapshot/snapshot.h"
#include "window.h"
#include "dsp_util.h"
#include <algorithm>
#include <vector>

namespace PluginGain {

    struct impl_data_t {
        DAW::Host::process_scratch_buf_t buf;
    };

    module_gain::module_gain(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("Gain", _projectGlobalId, _hostCallback),
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
        for (const auto& paramEntry : parameterTypes) {
            registerParam(paramEntry.id)->initValue(paramEntry);
        }
        getParam(PARAM_TRACK_PAN)->isBiPolar = true;
    }
    module_gain::~module_gain() {
        delete impl;
    }

    void module_gain::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert(in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);
        const auto autParGain = DAW::GetParameterModulationFromRouting(pluginMgr, DAW::GetRoutingFromDestinationParam(this, PARAM_GAIN));
        const auto autParPan = DAW::GetParameterModulationFromRouting(pluginMgr, DAW::GetRoutingFromDestinationParam(this, PARAM_PAN));
        out->clear();
        /* fast path: no sample accurate automation */
        if (autParGain.type <= DAW::automation_routing_type::ROUTING_CONSTANT 
            && (autParPan.type <= DAW::automation_routing_type::ROUTING_NONE 
                ||  (autParPan.type <= DAW::automation_routing_type::ROUTING_CONSTANT && autParPan.atl->getParamValue(autParPan.paramIdx) == 0.5f))) {
            float fGain = 1.0f;
            bool bIsNotMuted = true;
            if (autParGain.type != DAW::automation_routing_type::ROUTING_NONE) {
                bIsNotMuted = dsp_util::getGainLvlWithRange(autParGain.atl->getParamValue(autParGain.paramIdx), MTR_CEIL, DBFS_MUTE_POS, fGain);
            }
            if (bIsNotMuted) {
                float fPanTrack = 0.5f;
                if (autParPan.type != DAW::automation_routing_type::ROUTING_NONE) {
                    fPanTrack = autParPan.atl->getParamValue(autParPan.paramIdx);
                }
                /* fast path: center pan */
                if (math::abs(fPanTrack - 0.5f) < 0.005f) {
                    out->addFromOp(in, AudioBlock::mix_op::ADD, fGain);
                } else {
                    DAW::Panning::MultiplyConstant(in, out, fGain * (1.0f/DAW::Panning::GetCenterGain()), fPanTrack);
                }
            } else {
                /* fast path: fully muted */
            }
            return;
        }
        const auto tickBegin = tick;
        const auto tickEnd = tickBegin + host->getAudioStreamProperties().ticksPerBlock;
        DAW::Host::MixWithGainAndPanAutomation(host, impl->buf, in, out, autParGain, autParPan, tickBegin, tickEnd, state, MTR_CEIL, DBFS_MUTE_POS);
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
            if (dsp_util::getGainLvlWithRange(value, MTR_CEIL, DBFS_MUTE_POS, fGain)) {
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

    std::shared_ptr<PluginViewContainer> module_gain::createViewCtrInternal() {
        return std::make_shared<PluginViewContainerBasic<guictr_plugin_basic, module_gain>>(this, 100, 150);
    }
} // namespace PluginGain

template<>
effectbase* makeInstance<PluginGain::module_gain>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginGain::module_gain(_projectGlobalId, _hostCallback);
}
