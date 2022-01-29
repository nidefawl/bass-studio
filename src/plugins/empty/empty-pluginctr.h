#pragma once

#include <nanovg_min.h>
#include <vector>
#include <memory>

#include "str_util.h"
#include "color_util.h"
#include "gui/gui.h"
#include "gui/guicontainer.h"
#include "gui/knob.h"
#include "gui/button.h"


class vstplugin;
class AudioEffect;

namespace PluginEmptyVST2 {
    class EmptyPluginVST2;

    class guictr_emptyvst : public guictr_base {
        vstplugin* vstHostSide     = nullptr;
        EmptyPluginVST2* curEffect = nullptr;

    public:
        guictr_emptyvst() : guictr_base() {
            setBackgroundRendered(true);
        }
        ~guictr_emptyvst() {
            removeGuis();
        }
        bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
        void render(NVGcontext* vg) override;
        bool handleKeyInput(KeyEvent& event) override;
        void onGuiOpen(AudioEffect* eff);
        void onGuiClose(AudioEffect* eff);
        void setVSTPlugin(vstplugin* vstHostSide);
        void onSetParameter(int32_t index, float value);
    };

}// namespace PluginEmptyVST2
