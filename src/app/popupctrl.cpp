#include <nanovg.h>
#include <vector>
#include "logging.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "str_util.h"
#include "window.h"

#include "keyboard.h"
#include "commands.h"

#include "basectrl.h"
#include "seq_util.h"

#include "../gui/gui.h"
#include "guiconstant.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/container/scrollcontainer.h"
#include "gui/controls/scrollbar.h"

void PopupCtrl::closePopup() {
    if (isShown()) {
        static_cast<window_main*>(this->window)->hide();
    }
}

void PopupCtrl::onWindowClose() {
    AppCtrl::onWindowClose();
    popupCtrs->removeGuis();
    resetMouseContext();
}
void PopupCtrl::render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) {
    BaseCtrl::render(nanovgCtxt, x, y, w, h, ratio);
}
void PopupCtrl::relayout(int32_t w, int32_t h) {
    if (bResizeable) {
        closeAllAppMenus();
        closeContextMenu();
        if (popupCtrs->guis.size() == 1) {
            auto singleCtr  = popupCtrs->guis[0];
            singleCtr->size = getScaledSize();
            singleCtr->determineSize(singleCtr->size);
            singleCtr->layout();
            popupCtrs->size      = singleCtr->size;
            popupCtrs->maxHeight = singleCtr->size.y;
            popupCtrs->determineSize(popupCtrs->size);
            auto unscaledWindowSize = vec2(singleCtr->size) * m_scale;
            window->setSize({math::ceilfS32(unscaledWindowSize.x), math::ceilfS32(unscaledWindowSize.y)});
        }
    }
    popupCtrs->layout();
}
bool PopupCtrl::mouseDownPre() {
    if (this->ctxtmenu && this->ctxtmenu->isDialog()) {
        return false;
    }
    closeAllContextMenus();
    return true;
}


void PopupCtrl::open(guictxtmenu_base* _ctxtmenu, ivec2 pos, bool bResizeable, bool bFocused) {
    //dbgassert(!isShown());
    mouseInside       = false;
    this->bResizeable = bResizeable;
    this->m_mousePos  = ivec2(-1111111);
    popupCtrs->removeGuis();
    popupCtrs->pos              = ivec2(0);
    _ctxtmenu->pos              = insetCtxtMenu;
    canTakeInputFocus           = _ctxtmenu->canTakeInputFocus;
    popupCtrs->maxHeight        = _ctxtmenu->maxHeight;
    popupCtrs->scrollbarOutside = _ctxtmenu->scrollbarOutside;
    popupCtrs->setBackgroundRendered(_ctxtmenu->isBackgroundRendered());
    _ctxtmenu->setParent(popupCtrs);
    _ctxtmenu->setControl(this);
    _ctxtmenu->determineSize(_ctxtmenu->size);
    _ctxtmenu->layout();

    auto mainCtrl = this->mainWindow->getCtrl();
    if (mainCtrl) {
        *getTheme() = *mainCtrl->getTheme();
    }

    popupCtrs->size = vec2(_ctxtmenu->size.x, math::max(0, popupCtrs->maxHeight > 0 ? popupCtrs->maxHeight : _ctxtmenu->size.y));
    popupCtrs->add(_ctxtmenu);
    popupCtrs->determineSize(popupCtrs->size);
    popupCtrs->layout();

    this->guiFocused    = _ctxtmenu;
    this->guiCtrFocused = _ctxtmenu;

    if (this->window) {
        auto* appW = dynamic_cast<window_main*>(this->window);
        m_size = popupCtrs->size;
        auto scaledSize = ivec2(vec2(m_size) * m_scale);
        appW->positionOnScreen(pos - insetCtxtMenu, scaledSize);
        appW->show();
#ifndef _WIN32
        appW->positionOnScreen(pos - insetCtxtMenu, scaledSize);
#endif
        if (bFocused)
            appW->focus();
    }
    int32_t clearc = getTheme()->getColorInt32(GuiColor::COL_CLEAR_COLOR);
    if (popupCtrs->isBackgroundRendered()) {
        clearc |= 0xFF000000;
    } else {
        clearc &= 0x00FFFFFF;
        clearc &= 0x00000000;
    }
    getTheme()->setColor(GuiColor::COL_CLEAR_COLOR, clearc);
}

void PopupCtrl::destroy() {
    dbgassert(isOK);
    if (!isOK) {
        return;
    }
    isOK = false;
    this->containers.clear();
    this->containers.shrink_to_fit();
    delete popupCtrs;
    popupCtrs = nullptr;
}

class guictr_scrollbar_outline : public guictr_scrollbar {
public:
    guictr_scrollbar_outline() : guictr_scrollbar() {
        //padding=0;
        //margin=0;
    }
    void render(NVGcontext* vg) override {
        renderFrameBase(vg);
        nvgSave(vg);
        guictr_scrollbar::render(vg);
        nvgRestore(vg);
        renderFrameOutline(vg);
    }
};
bool PopupCtrl::initAppWindow(window_main* _window, NVGcontext* nanovg) {
    guitheme_t themeDefault;
    themeDefault.name = "default";
    themes.setTheme(themeDefault);
    themes.loadThemes();
    this->mainWindow = _window;
    this->window     = _window;
    this->vg         = nanovg;
    popupCtrs        = new guictr_scrollbar_outline();
    this->containers.push_back(popupCtrs);
    for (guictr_base* ctr : containers) {
        ctr->setControl(this);
    }
    isOK = true;
    return isOK;
}
bool PopupCtrl::initPopup(window_overlay* _window, NVGcontext* nanovg) {
    dbgassert(0);
    return false;
}

void PopupCtrl::onTick() {
    for (guictr_base* ctr : containers) {
        ctr->onTick(this);
    }
    for (guictr_base* ctr : containers) {
        ctr->onIdle();
    }
    mainWindow->requestRedraw();
}
