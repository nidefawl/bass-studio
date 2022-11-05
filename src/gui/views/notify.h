#pragma once
#include "basectrl.h"
#include "gui/controls/button.h"
#include "guicolors.h"
#include "math/vec.h"
#include "gui/gui.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu.h"
#include "platform.h"
#include "renderresources.h"


class gui_notify : public guictr_base {
protected:
    bool hadMouseFocus = false;
    guibutton btnHide;
    int64_t tmDelay  = 0L;
    int64_t tmCreate = 0L;
    String strErrSrc;
    String strErrMsg;
    GuiColor::constant_t colorBackground;
    GuiColor::constant_t colorText;
public:
    gui_notify() : guictr_base() {
        padding = 1;
        margin = padding;
        setCanMouseHit(true);
        setBackgroundRendered(true);
        setBackgroundRenderedInset(false);
        add(&btnHide);
        padding           = 6;
        margin            = 0;
        btnHide.setText("Hide");
    }
    void setMessage(String errSource, String errMessage) {
        strErrSrc         = std::move(errSource);
        strErrMsg         = std::move(errMessage);
    }
    ~gui_notify() override {
        removeGuis();
    }
    void setDelay(int64_t _tmDelay) {
        this->tmDelay  = _tmDelay;
        this->tmCreate = getTimeMillis();
    }
    void onTick(AppCtrl* appctrl) override {
        if (this->tmDelay > 0 && this->tmCreate > 0 
            && math::max<int64_t>(0, this->tmDelay - (getTimeMillis() - this->tmCreate)) <= 0) {
            setVisible(false);
        }
    }
    void determineSize(ivec2& prefSize) override {
    }
    void layout() override {
        auto cs      = getSizeContent();
        btnHide.size = ivec2(cs.x / 5, HEIGHT_DEFAULT_INPUT);
        btnHide.pos  = ivec2(cs.x - btnHide.size.x - padding, (cs.y-btnHide.size.y)/2);
        for (auto* g : guis) {
            g->layout();
        }
    }

    GuiColor::constant_t getOuterBackgroundColorFromState(int32_t stateflags) const override {
        return colorBackground;
    }
    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
        guictr_base::render(vg);
        if (strErrSrc.length() > 0) {
            auto cs = getSizeContent();
            ivec2 renderSize = btnHide.isVisible() ? ivec2(btnHide.pos.x - padding, cs.y) : cs;
            ivec2 renderPos(0);
            renderPos.x += size.y;
            renderSize.x -= size.y;
            
            auto iconId = ICON_WARNING;
            drawIcon(vg, vec2(cs.y), &RenderResources::imgIcons[iconId], -2);
            auto fontScale = cs.y * 0.4f;

            renderCenteredMultilineText(vg, theme, strErrSrc + "\n" + strErrMsg, fontScale, getLabelColor(), renderPos, renderSize);
        }
    }
    GuiColor::constant_t getLabelColor() const override {
        return colorText;
    }
    void setColors(GuiColor::constant_t background, GuiColor::constant_t text) {
        colorBackground = background;
        colorText       = text;
    }
};