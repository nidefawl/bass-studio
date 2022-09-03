#pragma once

#include <nanovg_min.h>
#include <vector>
#include <memory>

#include "str_util.h"
#include "color_util.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/controls/knob.h"
#include "gui/controls/button.h"



namespace PluginEmptyVST2 {
    class EmptyPluginVST2;

    class guictr_emptyvst : public guictr_base {
        EmptyPluginVST2* const plugin;

    public:
        explicit guictr_emptyvst(EmptyPluginVST2* plugin)
            : guictr_base(),
            plugin(plugin)
        {
            setBackgroundRendered(true);
        }
        ~guictr_emptyvst() override {
            removeGuis();
        }
        bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
        void render(NVGcontext* vg) override;
        bool handleKeyInput(KeyEvent& event) override;
        void onGuiOpen();
        void onGuiClose();
        void onSetParameter(int32_t index, float value);
        void getSizeScale(int& w, int& h) const;
    };

}// namespace PluginEmptyVST2
