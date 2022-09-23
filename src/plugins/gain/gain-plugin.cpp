#include "gain-plugin.h"
#include "dsp_util.h"
#include "event.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugincontrol.h"
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
#include <algorithm>

module_gain::module_gain(int32_t _projectGlobalId, i_host_callback* _hostCallback)
    : internalplugin("Gain", getModuleType(), _projectGlobalId, _hostCallback)
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

void module_gain::process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
    dbgassert(in->samples == format.blockSize
              && out->samples == format.blockSize
              && format.blockSize > 0
              && format.sampleRate > 0);

    out->clear();
    /* Calculate group gain level */
    float fGain = 1.0f;
    if (dsp_util::getGainLvlWithRange(getParamValue(PARAM_GAIN), MTR_CEIL, DBFS_MUTE_POS, fGain)) {
        out->addFromOp(in, AudioBlock::mix_op::ADD, fGain);
    }
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
param_unit_t module_gain::getParamValueDisplay(int32_t idx) {
    auto param = getParam(idx);
    dbgassert(param);
    if (param->unit == "dB") {
        float fGain = 1.0f;
        if (dsp_util::getGainLvlWithRange(getParamValue(PARAM_GAIN), MTR_CEIL, DBFS_MUTE_POS, fGain)) {
            return {StringFormat("%.3f", dsp_util::dBFS(fGain)), param->unit};
        }
        return {"-INF", param->unit};
    }
    return internalplugin::getParamValueDisplay(idx);
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

std::shared_ptr<PluginViewContainers> module_gain::createInternalView() {
    if (!views.empty()) {
        for (auto& existingView : views) {
            if (!existingView->isInUse()) {
                existingView->setUsed();
                return existingView;
            }
        }
    }
    auto v = std::make_shared<SinglePluginViewContainers<guictr_vst2_simple, module_gain>>(this, 100, 150);
    this->views.push_back(v);
    return v;
}

template<>
effectbase* makeInstance<module_gain>(int32_t _projectGlobalId, i_host_callback* _hostCallback) {
    return new module_gain(_projectGlobalId, _hostCallback);
}
