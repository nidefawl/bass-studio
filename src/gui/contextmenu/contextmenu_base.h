#pragma once
#include <utility>
#include <vector>
#include "assert_dbg.h"
#include "math/vec.h"
#include "event.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "guicolors.h"
#include "basectrl.h"

namespace RenderResources {
    struct NvgImageTexture;
}
class ctxtmenu_entry {
public:
    int id = 0;
    String title;
    String rightTitle;

    int width             = -1;
    int height            = 0;
    int y                 = 0;
    float fontSize        = 0;
    float fixedLeftOffset = -1;
    guitheme_t* theme     = nullptr;
    bool bIsMenuOpen = false;
    bool bGrayedOut = false;
    GlobalCommandType commandtype = GlobalCommandType::CMD_NONE;

    RenderResources::NvgImageTexture* icon = nullptr;
    GuiColor::constant_t iconColor;

    ctxtmenu_entry(String _title, int _id)
        : id(_id), title(std::move(_title))
    {
    }

    ctxtmenu_entry(AppCtrl* ctrl, GlobalCommandType _type);

    virtual ~ctxtmenu_entry() = default;

    void setGrayedOut(bool b) {
        bGrayedOut = b;
    }

    bool isGrayedOut() const {
        return bGrayedOut;
    }

    void setIcon(RenderResources::NvgImageTexture* _icon, GuiColor::constant_t color) {
        this->icon = _icon;
        this->iconColor = color;
    }

    virtual void layout(ivec2 size, float _fontSize, determine_string_width& strw);

    virtual float leftOffset();

    virtual bool showSubmenuArrow() {
        return false;
    }

    virtual void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse);
    virtual bool contains(ivec2& ctxtSize, ivec2& mouse) const {
        return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
    }
    virtual int getClicked(ivec2& ctxtSize, ivec2& mouse) {
        if (contains(ctxtSize, mouse)) {
            return id;
        }
        return -1;
    }
    void setIsMenuOpen(bool isMenuOpen) { this->bIsMenuOpen = isMenuOpen; }
    bool isMenuOpen() const { return bIsMenuOpen; }
};

class ctxtmenu_splitter final : public ctxtmenu_entry {
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

struct ctxmenu_enum_select_entry {
    int32_t id = 0;
    String name;
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
};
template<typename EntryType>
class ctxtmenu_enum_option_select_base : public ctxtmenu_entry {
protected:
    int32_t perRowEntries = 6;
    std::vector<EntryType> entries;
    const int pad   = 10;
    const int inset = 5;
    int32_t selectedId = -1;
public:
    ctxtmenu_enum_option_select_base(int32_t _id, String _title)
        : ctxtmenu_entry(std::move(_title), _id)
    {
        width = 200;
    }
    void addEntry(EntryType e) {
        entries.push_back(e);
    }
    virtual bool isEntrySelected(ctxmenu_enum_select_entry& e) const {
        return e.id == selectedId;
    }
    void setSelectedId(int32_t id) {
        selectedId = id;
    }
    int32_t getSelectedId() const {
        return selectedId;
    }

    void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
        auto h = fontSize * 1.1f;

        for (auto& e : entries) {
            if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                nvgBeginPath(vg);
                nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                nvgFill(vg);
            }
            if (isEntrySelected(e)) {
                nvgBeginPath(vg);
                nvgCircle(vg, e.x + 10, y + e.y + h / 2, 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_TEXT));
                nvgFill(vg);
            }
        }

        renderTextLabel(vg,
                        vec2(leftOffset(), y + h * 0.5f),
                        vec2(width, h),
                        title,
                        theme,
                        fontSize,
                        theme->getColor(GuiColor::COL_TEXT),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        for (auto& e : entries) {
            renderTextLabel(vg,
                            vec2(e.x + 20.0f, y + e.y + h * 0.5f),
                            vec2(width, h),
                            e.name,
                            theme,
                            fontSize * 0.9f,
                            theme->getColor(GuiColor::COL_TEXT),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }

    void layout(ivec2 size, float _fontSize, determine_string_width& strw) override {
        width = size.x;
        this->fontSize = _fontSize;
        const int h    = math::roundfS32(_fontSize);
        layoutE(width, h, math::max(1, math::min(perRowEntries, int(entries.size()))));
    }

    void layoutE(int tw, int h, int perRow) {
        int iX      = inset;
        int iY      = h + 2;
        int elW     = (tw - inset * 2) / perRow;
        for (auto& e : entries) {
            this->height = iY + h;
            e.x = iX;
            e.y = iY;
            e.w = elW;
            iX += e.w;
            if (iX >= tw - inset * 2) {
                iX = inset;
                iY += h;
            }
        }
    }

    int getClicked(ivec2& ctxtSize, ivec2& mouse) override {
        if (contains(ctxtSize, mouse)) {
            const auto h = this->fontSize;
            for (auto& e : entries) {
                if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= 0 && mouse.x < e.x + e.w) {
                    return this->id + e.id;
                }
            }
        }
        return -1;
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
    virtual bool isTransient() const {
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

    GuiColor::constant_t getBackgroundColorFromState(int32_t stateflags) const override {
        if (focused()) {
            return GuiColor::COL_BG_BRT;
        }
        return GuiColor::COL_BG_BRT;
    }

    GuiColor::constant_t getOuterBackgroundColorFromState(int32_t stateflags) const override {
        if (focused()) {
            return GuiColor::COL_BG_BRT;
        }
        return GuiColor::COL_BG_BRT;
    }
};
