#include "macros-plugin.h"
#include "automation.h"
#include "dsp_util.h"
#include "event.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugincontrol.h"
#include "seq_util.h"
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

namespace PluginMacros {
    static constexpr int32_t PARAM_NUM_MACROS = 32;
    static constexpr int32_t PARAM_MACROS_FIRST = 16;
    class guictr_macros : public guictr_vst2_simple {
        int32_t numKnobs = 4;
        public:
        guictr_macros(internalplugin* module) : guictr_vst2_simple(module) {
        }
        void layoutEntries(ivec2 dir) override {
            guictr_vst2_simple::layoutEntries(dir);
        }
        void setNumKnobs(int32_t num) {
            numKnobs = num;
            auto knobs = getKnobs();
            for (int32_t i = 0; i < numKnobs && i < CtrSize(knobs); ++i) {
                knobs[i]->setVisible(true);
            }
            for (int32_t i = numKnobs; i < CtrSize(knobs); ++i) {
                knobs[i]->setVisible(false);
            }
        }
        void getSizeScale(int& w, int& h) const {
            w = 100*numKnobs;
            h = 300;
        }
    };
    class MacroViewContainer : public PluginViewContainers {
    protected:
        uint32_t width;
        uint32_t height;

    public: 
        guictr_macros ctr_main;
        explicit MacroViewContainer(module_macros* eff, uint32_t _width = 320, uint32_t _height = 320)
            : width(_width), height(_height), ctr_main(eff) {
            ctr_main.setNumKnobs(4);
        }
        ~MacroViewContainer() override = default;
        guictr_macros& getPluginUI() {
            return ctr_main;
        }
        const guictr_macros& getPluginUI() const {
            return ctr_main;
        }
        void layout(int32_t winW, int32_t winH) override {
            ctr_main.pos  = { 0, 0 };
            ctr_main.size = { winW, winH };
        }
        void addTo(std::vector<guictr_base*>& v) override {
            v.push_back(&ctr_main);
        }
        void onGuiOpen() override {
            ctr_main.onGuiOpen();
        }
        void onGuiClose() override {
            ctr_main.onGuiClose();
        }
        void onSetParameter(int32_t index, float value) override {
            ctr_main.onSetParameter(index, value);
        }
        void getFixedSize(int32_t* w, int32_t* h) override {
            ctr_main.getSizeScale(*w, *h);
        }
    };

    module_macros::module_macros(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internal_automator("Macros", getModuleType(), _projectGlobalId, _hostCallback)
    {
        struct effectgain_param_entry {
            int32_t id;
            String name;
            String unit;
            float val;
        };
        for (int32_t i = 0; i < PARAM_NUM_MACROS; ++i) {
            auto paramIdx = PARAM_MACROS_FIRST + i;
            automatable_param_t* regparam = registerParam(paramIdx);
            regparam->defaultValue = 0.0f;
            regparam->value = 0.0f;
            regparam->name  = StringFormat("Macro %d", i);
            regparam->shortLabel  = StringFormat("Macro %d", i);
            regparam->unit  = "%";
        }
    }
    
    module_macros::~module_macros() {
    }

    void module_macros::process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert( in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);
    }
    std::shared_ptr<PluginViewContainers> module_macros::createViewCtrInternal() {
        return std::make_shared<MacroViewContainer>(this, 100, 150);
    }
}// namespace PluginMacros

template<>
effectbase* makeInstance<PluginMacros::module_macros>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginMacros::module_macros(_projectGlobalId, _hostCallback);
}
