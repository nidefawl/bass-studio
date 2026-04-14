#pragma once
#include "nanovg/nanovg.h"
#include <functional>
#include "math/vec.hpp"
#include "math/seq_math.hpp"
#include "guicolors.hpp"
#include "color_util.hpp"
#include "str_util.hpp"
#include "seq_util.hpp"
#include "gui/gui.hpp"
#include "guicolors.hpp"
#include "basectrl.hpp"
#include "event.hpp"

class guibutton : public guibase {
protected:
    GuiColor::constant_t buttonColor;
    String str         = "";

public:
    void (*drawFn)(NVGcontext*, ivec2&, ivec2&, const NVGcolor&, int drawParm, int drawParm2) = NULL;
    int drawParm                                                                              = 0;

public:
    guibutton();
    void setButtonColor(GuiColor::constant_t color) {
        buttonColor = color;
        setFlagInternal(FLG_HAS_COLOR_BG);
    }

    GuiColor::constant_t getBackgroundColorFromState(int32_t stateflags) const override;

    void handleDraggedMove(MouseEvent& evt) override {
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        if (parent)
            parent->buttonClicked(this);
    }
    void handleRightClick(MouseEvent& evt) override {
        if (parent)
            parent->rightClicked(evt, this);
    }
    void setText(String _str) {
        if (label.empty())
            setLabel(_str);
        str = _str;
    }
    const String& getText() const {
        return str;
    }
    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
        int32_t fl = getStateFlags();
        renderWidgetBorder(vg, fl);
        renderButtonLabel(vg, fl);
    }
    virtual void renderButtonLabel(NVGcontext* vg, int32_t stateFlags);
    virtual bool getState() const {
        return true;
    }
    void renderWidgetBorder(NVGcontext* vg, int32_t flags) const override {
        if ((flags & FLG_RENDER_BACKGROUND) || ((flags & FLG_HAS_COLOR_BG) && getState())) {
            renderWidgetBorderPosSize(vg, flags, pos, size);
        }
    }

};
class guibuttonstate : public guibutton {
protected:
    bool* statePtr = NULL;

public:
    guibuttonstate() : guibutton() {
    }
    bool getState() const override {
        if (statePtr)
            return *statePtr;
        return true;
    }
    void setStateRef(bool* _enabledPtr) {
        statePtr = _enabledPtr;
    }
    bool* getStateRef() {
        return statePtr;
    }
    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
        int32_t fl = getStateFlags();
        renderWidgetBorder(vg, fl);
        renderButtonLabel(vg, fl);
    }
};
class guibuttontoggle : public guibuttonstate {
    int _getIcon() {
        return getIcon ? getIcon() : icon;
    }

public:
    float radius = 0;
    int icon     = -1;
    std::function<int()> getIcon;
    std::function<bool()> fnGetState;
    GuiColor::constant_t colorActive = GuiColor::COL_BTN_BG_DEFAULT_INACTIVE;
    guibuttontoggle() : guibuttonstate() {
    }
    void setRadius(float fRadius) {
        this->radius = fRadius;
        if (size.x == 0 && size.y == 0) {
            size = ivec2((int32_t) std::round(this->radius * 2.0f));
        }
    }
    bool getState() const override {
        if (statePtr)
            return *statePtr;
        if (fnGetState)
            return fnGetState();
        return true;
    }
    
    void render(NVGcontext* vg) override;
};
