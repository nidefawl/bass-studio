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
#include "stereowidth-plugin.h"
#include "stereowidth-pluginctr.h"

using namespace PluginStereoWidth;


class guicontainer_stereowidth : public guictr_base {
    PluginVST2_StereoWidth* const plugin;
    guiknob_pluginparam knobgain;
    guiknob_pluginparam knobwidth;

public:
    explicit guicontainer_stereowidth(PluginVST2_StereoWidth* plugin)
        : guictr_base(),
        plugin(plugin),
        knobgain(PARAM_OFFSET_EXTERNAL + kGain, kGain),
        knobwidth(PARAM_OFFSET_EXTERNAL + kStereoWidth, kStereoWidth)
    {
        setBackgroundRendered(true);
        padding = 4;
        margin  = 4;
        add(&knobwidth);
        add(&knobgain);
    }
    ~guicontainer_stereowidth() override {
        remove(&knobgain);
        remove(&knobwidth);
    }

    void onGuiOpen() {
#if BUILD_VSTHOST
        knobwidth.setEffectInstance(plugin->getHostSideHandle());
        knobgain.setEffectInstance(plugin->getHostSideHandle());
#endif
#if BUILD_EXTERNAL_PLUGIN
        knobwidth.setAudioEffect(plugin);
        knobgain.setAudioEffect(plugin);
#endif
    }

    void onGuiClose() {
#if BUILD_VSTHOST
        knobwidth.setEffectInstance(nullptr);
        knobgain.setEffectInstance(nullptr);
#endif
#if BUILD_EXTERNAL_PLUGIN
        knobwidth.setAudioEffect(nullptr);
        knobgain.setAudioEffect(nullptr);
#endif
    }

    guiknob_pluginparam* getKnobFromParameter(int32_t index) {
        switch (index) {
            case kGain:
                return &knobgain;
            case kStereoWidth:
                return &knobwidth;
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

    void layout() override {
        ivec2 cs           = getSizeContent();
        const int inset    = 4;
        const int knobSize = math::max(32, (cs.x - inset * 3) / 2);
        knobwidth.size     = ivec2(knobSize, cs.y - inset * 2);
        knobgain.size      = ivec2(knobSize, cs.y - inset * 2);
        knobwidth.pos      = ivec2(inset);
        knobgain.pos       = ivec2(knobwidth.right() + inset, inset);
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
};

namespace PluginStereoWidth {
    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new PluginVST2_StereoWidth(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> PluginVST2_StereoWidth::createView() {
        auto view = std::make_shared<SinglePluginViewContainers<guicontainer_stereowidth, PluginVST2_StereoWidth>>(this, 220, 150);
        this->views.push_back(view);
        return view;
    }
}// namespace PluginStereoWidth
