#pragma once
#include <utility>
#include <vector>
#include "math/vec.h"
#include "event.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "basectrl.h"

namespace RenderResources {
    struct NvgImageTexture;
}
class ctxtmenu_entry {
public:
    int id = 0;
    String title;

    int width             = -1;
    int height            = 0;
    int y                 = 0;
    float fontSize        = 0;
    float fixedLeftOffset = -1;
    guitheme_t* theme     = nullptr;

    RenderResources::NvgImageTexture* icon = nullptr;
    GuiColor::constant_t iconColor;

    ctxtmenu_entry(String _title, int _id)
        : id(_id), title(std::move(_title))
    {
    }

    virtual ~ctxtmenu_entry() = default;

    void setIcon(RenderResources::NvgImageTexture* _icon, GuiColor::constant_t color) {
        this->icon = _icon;
        this->iconColor = color;
    }

    virtual void layout(ivec2 size, float _fontSize, determine_string_width& strw) {
        this->fontSize = _fontSize;
        this->height   = math::roundfS32(_fontSize * 1.1f);
        this->width = math::max<float>(size.x, leftOffset()+strw.getStringWidth(title, _fontSize, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE));
    }

    virtual float leftOffset() {
        if (fixedLeftOffset >= 0) {
            return fixedLeftOffset;
        }
        auto offset = this->fontSize / 2.4f;
        if (icon != nullptr) {
            offset += height;
        }
        return offset;
    }

    virtual void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
        if (contains(ctxtSize, mouse)) {
            nvgBeginPath(vg);
            nvgRect(vg, 0, y, ctxtSize.x, height);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
            nvgFill(vg);
        }
        if (this->icon) {
            nvgTranslate(vg, height / 4, y+2);
            drawIconColored(vg, ivec2(height - 4), icon, theme->getColor(iconColor), 4);
            nvgTranslate(vg, -height / 4, -(y+2));
        }

        renderTextLabel(vg,
                        vec2(leftOffset(), y + height * 0.5f),
                        vec2(width, height),
                        title,
                        theme,
                        fontSize,
                        theme->getColor(GuiColor::COL_TEXT),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }
    virtual bool contains(ivec2& ctxtSize, ivec2& mouse) const {
        return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
    }
    virtual int getClicked(ivec2& ctxtSize, ivec2& mouse) {
        if (contains(ctxtSize, mouse)) {
            return id;
        }
        return -1;
    }
};

class ctxtmenu_splitter : public ctxtmenu_entry {
public:
    ctxtmenu_splitter()
        : ctxtmenu_entry("-", -1) {
    }

    void render(ivec2 ctxtSize, NVGcontext* vg, int, ivec2) override {
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, y + height * 0.5f);
        nvgLineTo(vg, ctxtSize.x, y + height * 0.5f);
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_OUTLINE));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
    }

    void layout(ivec2, float _fontSize, determine_string_width& strw) override {
        this->fontSize = _fontSize;
        this->height   = math::roundfS32(_fontSize * 1.1f * 0.5f);
    }

    bool contains(ivec2&, ivec2&) const override {
        return false;
    }
};
class guictxtmenu_base : public guictr_base {
protected:
    int paddingV = 2;
    float fontSize = FONT_SIZE_CTXT;

public:
    bool scrollbarOutside  = false;
    bool canTakeInputFocus = false;
    int maxHeight          = 360;

    guictxtmenu_base() {
        margin  = 0;
        padding = 0;
    }
    void setFontSize(float _fontSize) {
        this->fontSize = _fontSize;
    }
    ~guictxtmenu_base() override {
        destroyGuis();
    }
    virtual bool isTransient() {
        return false;
    }
    virtual bool isDialog() {
        return false;
    }
    virtual void onParentWindowClose() {
    }

    void render(NVGcontext* vg) override {
        guictr_base::render(vg);
    }
    void determineSize(ivec2& prefSize) override {
        for (guibase* gui: guis) {
            auto prefSizeCpy = prefSize;
            gui->determineSize(prefSizeCpy);
            gui->size = prefSizeCpy;
        }
        ivec2 maxSize = ivec2(0);
        for (guibase* gui: guis) {
            maxSize.x = math::max(maxSize.x, gui->right());
            maxSize.y = math::max(maxSize.y, gui->bottom());
        }
        prefSize = maxSize;
    }
    void onChildLayoutChanged(guibase* g) override {
        //determineSize();
        if (this->parent) {
            this->parent->onChildLayoutChanged(this);
        }
    }
    void layout() override {
        for (auto* g: guis) {
            g->pos  = { 0, 0 };
            g->size = size;
            g->layout();
        }
    }
    void closeContextMenu() {
        // may be null if we got closed
        if (parentCtrl)
            parentCtrl->closePopup();
    }
};
