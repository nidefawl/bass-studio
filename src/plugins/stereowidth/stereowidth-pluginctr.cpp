#include "glheaders.h"
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
#include <vector>
#include <cmath>
#include <memory>

#include "str_util.h"
#include "math/seq_math.h"
#include "color_util.h"
#include "gui/gui.h"
#include "gui/guicontainer.h"
#include "gui/pluginviewcontainers.h"
#include "gui/button.h"
#include "gui/knob.h"
#include "gui/guiinputfield.h"
#include "gui/knobpluginparam.h"
#include "gui/guicontainer.h"
#include "gui/guicontextmenu_daw.h"
#include "basectrl.h"
#include "platform.h"

#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "host/plugin/base_plugin.h"
#include "host/plugin/vst_plugin.h"
#if BUILD_VSTHOST
#include "host/mainctrl.h"
#endif

#include "stereowidth-plugin.h"
#include "stereowidth-pluginctr.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"

using namespace PluginStereoWidth;


class guicontainer_stereowidth : public guictr_base {
    vstplugin* vstHostSide = nullptr;
    AudioEffect* curEffect = nullptr;
    guiknob_pluginparam knobgain;
    guiknob_pluginparam knobwidth;

public:
    guicontainer_stereowidth();
    ~guicontainer_stereowidth() override {
        remove(&knobgain);
        remove(&knobwidth);
    }
    void render(NVGcontext* vg) override;
    void prerender(NVGcontext* vg) override;
    void onTick(AppCtrl* ctrl) override;
    void layout() override;
    void buttonClicked(guibase* button) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
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
    void onGuiOpen(AudioEffect* eff);
    void onGuiClose(AudioEffect* eff);
    void setVSTPlugin(vstplugin* vstHostSide);

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
        guiknob_pluginparam* knob = getKnobFromParameter(index);
        if (knob && curEffect) {
            knob->setValueInit(value);
            knob->setDisplayValueFromEffect();
        }
    }
};


guicontainer_stereowidth::guicontainer_stereowidth()
    : guictr_base(), knobgain(PARAM_OFFSET_EXTERNAL + kGain, kGain), knobwidth(PARAM_OFFSET_EXTERNAL + kStereoWidth, kStereoWidth) {
    setBackgroundRendered(true);
    padding = 4;
    margin  = 4;
    add(&knobwidth);
    add(&knobgain);
}
void guicontainer_stereowidth::onGuiOpen(AudioEffect* eff) {
    this->curEffect = eff;
    knobwidth.setAudioEffect(eff);
    knobgain.setAudioEffect(eff);
}
void guicontainer_stereowidth::onGuiClose(AudioEffect* eff) {
    this->curEffect = nullptr;
}
void guicontainer_stereowidth::setVSTPlugin(vstplugin* _vstHostSide) {
    this->vstHostSide = _vstHostSide;
#if BUILD_VSTHOST
    knobwidth.setEffectInstance(_vstHostSide);
    knobgain.setEffectInstance(_vstHostSide);
#endif
}
void guicontainer_stereowidth::onTick(AppCtrl* ctrl) {
    for (guibase* gui : guis) {
        gui->onTick(ctrl);
    }
}
void guicontainer_stereowidth::buttonClicked(guibase* button) {
}
void guicontainer_stereowidth::prerender(NVGcontext* vg) {
    for (guibase* gui : guis) {
        gui->prerender(vg);
    }
}

void guicontainer_stereowidth::render(NVGcontext* vg) {
    //nvgBeginPath(vg);
    //nvgRect(vg, pos.x, pos.y, size.x, size.y);
    //nvgFillColor(vg, GUI_COLORRGB(50, 50, 150, 180));
    //nvgFill(vg);
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    //ivec2 cs = getSizeContent();
    //nvgBeginPath(vg);
    //nvgRect(vg, 0, 0, cs.x, cs.y);
    //nvgFillColor(vg, GUI_COLORRGB(50, 150, 150, 180));
    //nvgFill(vg);
    //nvgBeginPath(vg);
    //const int INS = 2;
    //nvgRect(vg, INS, INS, cs.x - INS * 2, cs.y - INS * 2);
    //nvgFillColor(vg, GUI_COLORRGB(150, 150, 50, 180));
    //nvgFill(vg);

    for (guibase* gui : guis) {
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }
}
void guicontainer_stereowidth::layout() {
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
bool guicontainer_stereowidth::handleKeyInput(KeyEvent& event) {
    if (event.type != KeyEventType::K_RELEASE) {
    }
    return false;
}


class ViewContainersStereoWidth : public PluginViewContainersImpl {
public:
    guicontainer_stereowidth ctr_main;
    ViewContainersStereoWidth() : PluginViewContainersImpl(220, 150) {
    }
    ~ViewContainersStereoWidth() override = default;
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
namespace PluginStereoWidth {
    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new PluginVST2_StereoWidth(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> PluginVST2_StereoWidth::createView() {
        std::shared_ptr<PluginViewContainers> view = std::make_shared<ViewContainersStereoWidth>();
        this->views.push_back(view);
        return view;
    }
}// namespace PluginStereoWidth
