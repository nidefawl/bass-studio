#include "glheaders.h"
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
#include <vector>

#include "str_util.h"
#include "color_util.h"
#include "gui/knob.h"
#include "gui/gui.h"
#include "gui/guicontainer.h"
#include "pluginctr.h"
#include "basectrl.h"
#include "platform.h"
#include "plugins/plugin.h"
#include "gui/pluginviewcontainers.h"
#include "host/plugin/vst_plugin.h"

#include "adv-plugin.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"


using namespace std;
using namespace PluginTestAdv;


gui_ctr_main::gui_ctr_main()
    : guictr_base(), field(nullptr) {
    setBackgroundRendered(true);
    add(&colorPicker);
    add(&textField);
    add(&field);
    field.setRef(&this->nr);
    textField.setChangeCallback([](const String& str) {
        log_printf("text callback %s\n", StringAsCStr(str));
        return true;
    });
}

void gui_ctr_main::onTick(AppCtrl* ctrl) {
    for (guibase* gui : guis) {
        gui->onTick(ctrl);
    }
}

void gui_ctr_main::prerender(NVGcontext* vg) {
    for (guibase* gui : guis) {
        gui->prerender(vg);
    }
}

void gui_ctr_main::render(NVGcontext* vg) {
    //int w = size.x;
    //int h = size.y;
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    //nvgBeginPath(vg);
    //nvgRect(vg, 0, 0, w, h);
    //nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
    //nvgFill(vg);
    BaseCtrl* ctrl = parentCtrl;

    vector<String> strings;
    String str;
    str = ctrl->guiOver ? ctrl->guiOver->getClassName() : "<null>";
    strings.push_back(String("guiOver: ") + str);
    str = ctrl->guiDragged ? ctrl->guiDragged->getClassName() : "<null>";
    strings.push_back(String("guiDragged: ") + str);
    str = ctrl->guiCaptured ? ctrl->guiCaptured->getClassName() : "<null>";
    strings.push_back(String("guiCaptured: ") + str);
    str = ctrl->guiCtrFocused ? ctrl->guiCtrFocused->getClassName() : "<null>";
    strings.push_back(String("guiCtrFocused: ") + str);
    str = ctrl->guiFocused ? ctrl->guiFocused->getClassName() : "<null>";
    strings.push_back(String("guiFocused: ") + str);

    guibase* p = ctrl->guiFocused;
    int lvl    = 0;
    while (p != NULL) {
        String s = "";
        if (lvl == 0) {
            s = "guiFocused: ";
        }
        for (int i = 0; i < lvl; i++) {
            s += "  ";
        }
        strings.push_back(s + p->getClassName());
        p = p->parent;
        lvl++;
    }

    int x = 5;

    setFont(vg, 26, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
    //String proj = StringFormat("Project: %s", StringAsCStr(ctrl->getProjectPath()));
    //nvgText(vg, x, 0, StringAsCStr(proj), NULL);
    float lineh;
    nvgTextMetrics(vg, NULL, NULL, &lineh);
    int y = this->getSizeContent().y - ((strings.size() + g_debugStrings.size()) * lineh);

#ifdef NO_GLFW_LIB
    nvgText(vg, 5, 75, "NO GLFW", NULL);
#else
    nvgText(vg, 5, 75, "USING GLFW", NULL);
#endif

    for (String& s : strings) {
        nvgText(vg, x, y, StringAsCStr(s), NULL);
        y += lineh;
    }
    for (String& s : g_debugStrings) {
        nvgText(vg, x, y, StringAsCStr(s), NULL);
        y += lineh;
    }
    field.render(vg);
    textField.render(vg);
    colorPicker.render(vg);
    g_debugStrings.clear();
}

void gui_ctr_main::layout() {
    ivec2 cs         = getSizeContent();
    colorPicker.size = ivec2(48 * 3 + 48, 48);
    colorPicker.pos  = ivec2(cs.x - colorPicker.size.x, cs.y - colorPicker.size.y);
    field.size       = ivec2(320, 32);
    field.pos        = ivec2(0, 0);
    textField.size   = ivec2(320, 32);
    textField.pos    = ivec2(0, field.bottom() + INSET_CTR_SPACING);
    colorPicker.layout();
    field.layout();
    textField.layout();
}

bool gui_ctr_main::handleKeyInput(KeyEvent& event) {
    if (event.type != KeyEventType::K_RELEASE) {
    }
    return false;
}

void gui_ctr_main::onGuiOpen(AudioEffect* eff) {
    this->curEffect = eff;
}

void gui_ctr_main::onGuiClose(AudioEffect* eff) {
    this->curEffect = nullptr;
}

void gui_ctr_main::setVSTPlugin(vstplugin* _vstHostSide) {
    this->vstHostSide = _vstHostSide;
}


void gui_ctr_main::onSetParameter(int32_t index, float value) {

}

class ViewContainersAdvPlugin : public PluginViewContainersImpl {
public:
    gui_ctr_main ctr_main;
    ViewContainersAdvPlugin() : PluginViewContainersImpl(400, 300) {
    }
    ~ViewContainersAdvPlugin() override = default;
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

void gui_ctr_main::buttonClicked(guibase* button) {
    if (&field == button) {
        log_printf("numberinput_field input: %d\n", this->nr);
    }
}

namespace PluginTestAdv {
    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new GuiAdvPluginVST2(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> GuiAdvPluginVST2::createView() {
        std::shared_ptr<PluginViewContainers> view = std::make_shared<ViewContainersAdvPlugin>();
        this->views.push_back(view);
        return view;
    }
}
