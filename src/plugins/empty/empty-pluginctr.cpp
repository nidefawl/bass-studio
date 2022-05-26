#include "glheaders.h"
#include <nanovg.h>
#include <vector>
#include <memory>

#include "str_util.h"
#include "color_util.h"
#include "platform.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/controls/knob.h"
#include "gui/controls/button.h"
#include "gui/container/container.h"
#include "gui/controls/colorpick.h"
#include "gui/controls/inputfield.h"
#include "gui/plugin/pluginviewcontainers.h"

#include "basectrl.h"

#include "plugins/plugin.h"
#include "host/plugin/vst_plugin.h"

#include "empty-plugin.h"
#include "empty-pluginctr.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"


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
        String str = StringFormat("%d processBlock calls", this->plugin->numCalls);
        nvgText(vg, 5, y, StringAsCStr(str), NULL);
        y += line;
        str = StringFormat("%d finished blocks", this->plugin->numCalls2);
        nvgText(vg, 5, y, StringAsCStr(str), NULL);
    }

    bool guictr_emptyvst::handleKeyInput(KeyEvent& event) {
        if (event.type != KeyEventType::K_RELEASE) {
        }
        return false;
    }

    void guictr_emptyvst::onGuiOpen() {
    }

    void guictr_emptyvst::onGuiClose() {
    }

    inline void guictr_emptyvst::onSetParameter(int32_t index, float value) {
    }


    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new EmptyPluginVST2(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> EmptyPluginVST2::createView() {
        auto view = std::make_shared<SinglePluginViewContainers<guictr_emptyvst, EmptyPluginVST2>>(this);
        this->views.push_back(view);
        return view;
    }
}
