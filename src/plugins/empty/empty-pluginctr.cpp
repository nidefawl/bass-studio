#include "glheaders.h"
#include <nanovg.h>
#include <vector>
#include <memory>

#include "str_util.h"
#include "color_util.h"
#include "platform.h"
#include "gui/gui.h"
#include "gui/guicontainer.h"
#include "gui/knob.h"
#include "gui/button.h"
#include "gui/guicontainer.h"
#include "gui/guicolorpick.h"
#include "gui/guiinputfield.h"
#include "gui/pluginviewcontainers.h"

#include "basectrl.h"

#include "plugins/plugin.h"
#include "host/plugin/vst_plugin.h"

#include "empty-plugin.h"
#include "empty-pluginctr.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"


class vstplugin;
class AudioEffect;

namespace PluginEmptyVST2 {
    bool guictr_emptyvst::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
        if (this->contains(mpos)) {
            ivec2 localMouse = this->toContainerSpace(mpos);
            for (guibase* gui : guis) {
                if (gui->mouseHitTest(localMouse, evt)) {
                    return true;
                }
            }
            if (evt.type == MouseHitType::MOUSE_LEFT) {
                evt.requestFocus(this);
                return true;
            }
        }
        return false;
    }

    void guictr_emptyvst::render(NVGcontext* vg) {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        int line = 26;
        setFont(vg, line - 2, THEMECOL_TEXT, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
        int y = line;
        nvgText(vg, 5, y, PluginEmptyVST2::getName(), NULL);
        y += line;
        String str = StringFormat("%d processBlock calls", this->curEffect->numCalls);
        nvgText(vg, 5, y, StringAsCStr(str), NULL);
        y += line;
        str = StringFormat("%d finished blocks", this->curEffect->numCalls2);
        nvgText(vg, 5, y, StringAsCStr(str), NULL);
    }

    bool guictr_emptyvst::handleKeyInput(KeyEvent& event) {
        if (event.type != KeyEventType::K_RELEASE) {
        }
        return false;
    }

    void guictr_emptyvst::onGuiOpen(AudioEffect* eff) {
        this->curEffect = static_cast<PluginEmptyVST2::EmptyPluginVST2*>(eff);
    }

    void guictr_emptyvst::onGuiClose(AudioEffect* eff) {
        this->curEffect = nullptr;
    }

    void guictr_emptyvst::setVSTPlugin(vstplugin* _vstHostSide) {
        this->vstHostSide = _vstHostSide;
    }

    inline void guictr_emptyvst::onSetParameter(int32_t index, float value) {
    }


    class ViewContainersEmptyPlugin : public PluginViewContainersImpl {
    public:
        guictr_emptyvst ctr_main;
        ViewContainersEmptyPlugin() : PluginViewContainersImpl(400, 300) {
        }
        ~ViewContainersEmptyPlugin() override = default;

        void layout(int32_t winW, int32_t winH) override {
            ctr_main.pos  = { 0, 0 };
            ctr_main.size = { winW, winH };
        }
        void addTo(std::vector<guictr_base*>& v) override {
            v.push_back(&ctr_main);
        }
        void onGuiOpen(AudioEffect* eff) override {
            ctr_main.onGuiOpen(eff);
        }
        void onGuiClose(AudioEffect* eff) override {
            ctr_main.onGuiClose(eff);
        }
        void onSetParameter(int32_t index, float value) override {
            ctr_main.onSetParameter(index, value);
        }
        void getFixedSize(int32_t* w, int32_t* h) override {
            *w = this->width;
            *h = this->height;
        }
        void setVSTPlugin(vstplugin* _hostsideplugin) override {
            ctr_main.setVSTPlugin(_hostsideplugin);
        }
    };

    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new EmptyPluginVST2(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> EmptyPluginVST2::createView() {
        std::shared_ptr<PluginViewContainers> view = std::make_shared<ViewContainersEmptyPlugin>();
        this->views.push_back(view);
        return view;
    }
}
