#pragma once
#include "guicolors.hpp"
#include "str_util.hpp"
#include "color_util.hpp"
#include "gui/container/container.hpp"
#include "theme.hpp"

class gui_statusbar final : public guictr_base {
public:
    String text;
    GuiColor::constant_t color = GuiColor::COL_TEXT;
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
        auto& bgColor = GuiColor::COL_BG_DRKER2;
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, theme->getColor(bgColor));
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
                            color == GuiColor::COL_TEXT ? theme->getContrastColor(bgColor) : theme->getColor(color),
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
    void setTitle(String _text, GuiColor::constant_t& _color = GuiColor::COL_TEXT) {
        text = std::move(_text);
        color = _color;
    }
};
