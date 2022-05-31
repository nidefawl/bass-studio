#include "glheaders.h"
#include "types.h"
#include "str_util.h"
#include "math/seq_math.h"
#include "color_util.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "plugins/plugin.h"
#include "sampledelay-plugin.h"

namespace PluginSampleDelay {

class guicontainer_sampledelay : public guictr_base {
    PluginVST2_SampleDelay* const plugin;
    guiknob_pluginparam knobDelay;

public:
    explicit guicontainer_sampledelay(PluginVST2_SampleDelay* plugin)
        : guictr_base(),
        plugin(plugin),
        knobDelay(PARAM_OFFSET_EXTERNAL + kSampleDelay, kSampleDelay)
    {
        setLayoutMode(LAYOUT_HORIZONTAL);
        setBackgroundRendered(true);
        padding = 4;
        margin  = 4;
        add(&knobDelay);
    }
    ~guicontainer_sampledelay() override {
        remove(&knobDelay);
    }

    void onGuiOpen() {
#if BUILD_VSTHOST
        knobDelay.setEffectInstance(plugin->getHostSideHandle());
#endif
#if BUILD_EXTERNAL_PLUGIN
        knobDelay.setAudioEffect(plugin);
#endif
    }

    void onGuiClose() {
#if BUILD_VSTHOST
        knobDelay.setEffectInstance(nullptr);
#endif
#if BUILD_EXTERNAL_PLUGIN
        knobDelay.setAudioEffect(nullptr);
#endif
    }

    guiknob_pluginparam* getKnobFromParameter(int32_t index) {
        switch (index) {
            case kSampleDelay:
                return &knobDelay;
        }
        return nullptr;
    }

    void onSetParameter(int32_t index, float value) {
#if BUILD_EXTERNAL_PLUGIN
        guiknob_pluginparam* knob = getKnobFromParameter(index);
        if (knob) {
            knob->setValueInit(value);
        }
#endif
    }
};

AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
    return new PluginVST2_SampleDelay(audioMaster);
}
std::shared_ptr<PluginViewContainers> PluginVST2_SampleDelay::createView() {
    auto view = std::make_shared<SinglePluginViewContainers<guicontainer_sampledelay, PluginVST2_SampleDelay>>(this);
    this->views.push_back(view);
    return view;
}

}// namespace PluginSampleDelay
