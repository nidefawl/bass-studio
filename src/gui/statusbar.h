#pragma once
#include "guicolors.h"
#include "str_util.h"
#include "color_util.h"
#include "guicontainer.h"
#include "theme.h"

class gui_statusbar : public guictr_base {
public:
    String text;
    gui_statusbar() : guictr_base() {
        padding = CONTENT_INSET/2;
        margin = padding;
        setBackgroundRendered(false);
    }
    ~gui_statusbar() override = default;
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, rgbaToNvg(0x7f1f1f1f));
        nvgFill(vg);
        if (!setScissorTransform(vg)) {
            return;
        }
        const auto cs = getSizeContent();
        if (!this->text.empty()) {
            renderTextLabel(vg,
                            vec2(0, cs.y * 0.5f),
                            cs,
                            text,
                            theme,
                            cs.y,
                            theme->getContrastColor(GuiColor::COL_CLEAR_COLOR),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (contains(mpos)) {
            evt.requestFocus(this);
            return true;
        }
        return false;
    }
    void setTitle(String _text) {
        text = _text;
    }
};
