#include "plugin_impl_gain.h"
#include "event.h"
#include "plugins/plugin-ui.h"
#include "str_util.h"
#include "gui/container/container.h"
#include "gui/controls/knoblabeled.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginctr.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "modules.h"
#include "internal_plugin.h"
#include "host/mainctrl.h"
#include "track.h"
#include "track_impl.h"
#include "audioblock.h"
#include "meter.h"
#include "snapshot.h"

class guimodule_gain : public guictr_base {
    std::vector<guiknob_pluginparam*> knobs;
    internalplugin* const module;
    guiknob_pluginparam knobgain;
    void addKnob(guiknob_pluginparam* knob) {
        knobs.push_back(knob);
        add(knob);
    }
public:
    explicit guimodule_gain(module_gain* _eff);
    ~guimodule_gain() override {
        removeGuis();
    }

    void onSetParameter(int32_t index, float value) {
        guiknob_pluginparam* knob = getKnobFromParameter(index);
        if (knob) {
            knob->setValueInit(value);
        }
    }
    guiknob_pluginparam* getKnobFromParameter(int32_t index);

    void onGuiOpen() {
        knobgain.setEffectInstance(module);
    }
    void onGuiClose() {
    }
};

guimodule_gain::guimodule_gain(module_gain* _eff)
    : module(_eff), knobgain(PARAM_GAIN) {
    setLayoutMode(LAYOUT_HORIZONTAL);
    setBackgroundRendered(true);
    padding = 4;
    margin  = 4;
    addKnob(&knobgain);
}

guiknob_pluginparam* guimodule_gain::getKnobFromParameter(int32_t index) {
    switch (index) {
        case PARAM_GAIN:
            return &knobgain;
    }
    return nullptr;
}

module_gain::module_gain(int32_t _projectGlobalId)
    : internalplugin("Gain", PLUGIN_TYPE_GAIN, _projectGlobalId)
{
    struct effectgain_param_entry {
        int32_t id;
        String name;
        String unit;
        float val;
    };
    const std::array<effectgain_param_entry, 2> parameterTypes{ {
            { PARAM_GAIN, "Gain", "dB", dsp_util::gainToLinScale(1.0f) },
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
    delete blockInputs;
    delete blockOutputs;
}

float module_gain::dispatchGetParameter(int32_t idx) {
    return 0;
}

void module_gain::dispatchSetParameter(int32_t idx, float val) {
}

void module_gain::postSetParameter(int32_t idx, float preVal, float val, int flags) {
    internalplugin::postSetParameter(idx, preVal, val, flags);
    for (auto& pviewctr : this->views) {
        if (pviewctr->isInUse()) {
            pviewctr->onSetParameter(idx, val);
        }
    }
}

samplecount_t module_gain::getPluginLatency() {
    return 0;
}

String module_gain::getInfo(std::vector<String>& list) {
    return "";
}

void module_gain::onTick(double since) {
    meter.onTick(since);
    meterIn.onTick(since);
}

void module_gain::process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
    dbgassert(getTrackLink()->sampleFormat == this->format
              && in->samples == format.blockSize
              && out->samples == format.blockSize
              && format.blockSize > 0
              && format.sampleRate > 0);

    out->clear();
    /* Calculate group gain level */
    float fGain = 1.0f;
    if (dsp_util::getGainLvl(getParamValue(PARAM_GAIN), fGain)) {
        out->addFromOp(in, AudioBlock::mix_op::ADD, math::clamp(fGain, 0.0f, 2.0f));
    }
}

void module_gain::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
    meterIn.update(this->blockInputs, 1.0f);
    meter.update(out, 1.0f);
}

void module_gain::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {
    internalplugin::loadSnapshot(pluginSnapshot);
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
    auto v = std::make_shared<SinglePluginViewContainers<guictr_vst2_simple, module_gain>>(this, 320, 320);
    this->views.push_back(v);
    return v;
}

template<>
effectbase* makeInstance<module_gain>(int32_t _projectGlobalId) {
    return new module_gain(_projectGlobalId);
}
