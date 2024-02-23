#pragma once
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/knoblabeled.h"
#include "gui/controls/inputfield.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "str_util.h"
#include "inputfield.h"


class gui_color_pick final : public guictr_base {
    guiknob_labeled_base knH;
    guiknob_labeled_base knS;
    guiknob_labeled_base knL;
    guiknob_labeled_base knA;
    gui_input_filtered hexInput;
    NVGcolor nvgColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    uint32_t colorU32     = 0xFFFFFFFFU;
    NVGcolor* ptrNvgColor = nullptr;
    uint32_t* ptrColorU32 = nullptr;
    void setHSL_(float h, float s, float v, float a);

public:
    std::function<void(uint32_t)> fnSetValue;

public:
    gui_color_pick();
    void setHSL(float h, float s, float l, float a);
    void setU32(uint32_t rgba);
    void init();
    ~gui_color_pick() override {
        removeGuis();
    }
    void buttonClicked(guibase* button) override;
    void layout() override;
    void setRefU32(uint32_t* ptrU32);
    void setRefNvg(NVGcolor* ptrNvg);
    void render(NVGcontext* vg) override;
    void handleRightClick(MouseEvent& evt) override;
    NVGcolor getNvg() {
        return nvgColor;
    }
    uint32_t getU32() {
        return colorU32;
    }
};

class gui_color_select : public guibutton {
    uint32_t colorU32     = 0xFFFFFFFFU;
public:
    enum dropdown_field_selectitem {
        SELECT_IDX,
        SELECT_NEXT,
        SELECT_PREVIOUS,
        SELECT_FIRST,
        SELECT_LAST,
    };
    gui_color_select() : guibutton() {
        setFlag(FLG_RENDER_BACKGROUND_INSET, true);
        setFlag(FLG_BG_SHADING, true);
    }
    uint32_t getColor() {
        return colorU32;
    }
    String getString() {
        return StringFormat("#%08X", colorU32);
    }
    void drawColor(NVGcontext* vg, ivec2 pos, ivec2 size, uint32_t rgba) {
        const int32_t inset = 3;
        int sizeQuad = size.y - inset * 2;
        nvgBeginPath(vg);
        nvgRect(vg, pos.x + size.x - inset - sizeQuad, pos.y + inset, sizeQuad, sizeQuad);
        auto previous = nvgGetCurrentAndSetFillColor(vg, rgbaToNvg(rgba));
        nvgFill(vg);
        nvgFillColor(vg, previous);
    }
    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
        const auto stateFlags = getStateFlags();
        renderWidgetBorder(vg, stateFlags);
        auto fontSizeScaled = math::clamp(size.y, 4, 48) * FONT_AUTOSCALE;
        auto sizeColorBox = vec2(size.y);
        if (this->label.length()) {
            auto posColorBox = vec2(pos) + vec2(size.x - size.y, 0);
            auto posText = vec2(posColorBox.x, pos.y + size.y * 0.5f);
            float textWidth = renderTextLabel(vg,
                            posText,
                            vec2(size),
                            getString(),
                            theme,
                            fontSizeScaled,
                            theme->getColor(getLabelColor()),
                            NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            renderTextLabel(vg,
                            vec2(pos) + vec2(3.0f, size.y * 0.5f),
                            vec2(size.x - textWidth - 6.0f, size.y),
                            label,
                            theme,
                            fontSizeScaled,
                            theme->getColor(GuiColor::COL_LABEL_INACTIVE),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            drawColor(vg, vec2(pos) + vec2(size.x - size.y, 0), sizeColorBox, colorU32);
        } else {
            renderTextLabel(vg,
                            vec2(pos) + vec2(size) * 0.5f,
                            vec2(size),
                            getString(),
                            theme,
                            fontSizeScaled,
                            theme->getColor(getLabelColor()),
                            NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            drawColor(vg, vec2(pos), sizeColorBox, colorU32);
        }
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        gui_color_pick* color = new gui_color_pick();
        color->size = {480, 240};
        color->pos = {0, 0};
        //color->setRefNvg(&value);
        color->setU32(colorU32);
        color->fnSetValue = [this](int32_t rgba) {
            colorU32 = rgba;
            if (parent && parent->getControl())
                parent->buttonClicked(this);
        };
        guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
        ctxtMenu->size = color->size;
        ctxtMenu->add(color);
        // color->layout();
        // ctxtMenu->layout();
        ctxtMenu->canTakeInputFocus = true;
        ctxtMenu->maxHeight = color->size.y;
        dbgassert(!ctxtMenu->isBackgroundRendered());
        ctxtMenu->setBackgroundRendered(false);
        parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
        // table->setActiveControl(nullptr);
        dbgassert(!ctxtMenu->isBackgroundRendered());
    }
};