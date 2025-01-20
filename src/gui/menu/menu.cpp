#include "menu.hpp"
#include "buildinfo.h"
#include "guicolors.hpp"
#include "guiglobals.hpp"
#include "renderresources.hpp"
#include "window.hpp"
#include <nanovg.h>

guimenu_ctxtentry::guimenu_ctxtentry(ngui::Menu* _menu)
    : ctxtmenu_entry(_menu->getTitle(), _menu->command.command), menu(_menu) {
    rightTitle = _menu->getRight();
    int32_t iconId = menu->icon;
    if (iconId > -1) {
        setIcon(&RenderResources::imgIcons[iconId], GuiColor::COL_WHITE);
    }
}

void guimenu_ctxtentry::layout(ivec2 size, float _fontSize, determine_string_width& strw) {
    this->fontSize = _fontSize;
    this->height   = math::roundfS32(_fontSize * 1.1f);
    auto entryW = leftOffset()+strw.getStringWidth(title, _fontSize, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    if (icon) entryW += height*1.5f;
    if (!rightTitle.empty()) {
        entryW += strw.getStringWidth(rightTitle, _fontSize, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    }
    entryW *= 1.2f;
    this->width = math::max(size.x, math::roundfS32(entryW));
}

void guimenu_ctxtentry::render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
    if (contains(ctxtSize, mouse)) {
        nvgBeginPath(vg);
        nvgRect(vg, 0, y, ctxtSize.x, height);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
        nvgFill(vg);
    }
    if (this->icon) {
        nvgTranslate(vg, height / 4, y+2);
        drawIconColored(vg, ivec2(height - 4), icon, theme->getColor(iconColor));
        nvgTranslate(vg, -height / 4, -(y+2));
    }


    renderTextLabel(vg,
                    vec2(leftOffset(), y + height*0.5f),
                    vec2(width, height),
                    title,
                    theme,
                    fontSize,
                    theme->getColor(GuiColor::COL_TEXT),
                    NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    String rightSide;
    if (this->rightTitle.length()) {
        rightSide = this->rightTitle;
    } else if (menu->type == ngui::menu_type::submenu) {
        rightSide = ">";
    }
    if (rightSide.length()) {
        auto defoffset = this->fontSize / 2.4f;
        renderTextLabel(vg,
                        vec2(width - defoffset, y + height*0.5f),
                        vec2(width, height),
                        rightSide,
                        theme,
                        fontSize,
                        THEMECOL_TEXT,
                        NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    }

}


void guictr_menubar_entry::handleDraggedBegin(MouseEvent& evt) {
    parentMenuBar->openMenu(this);
}

bool guictr_menubar_entry::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        evt.requestFocus(this);
        parentMenuBar->hoverMenu(this);
        return true;
    }
    return false;
}

void guictr_menubar_entry::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    guictr_menubar_entry* cur = parentMenuBar->currentMenu;

    bool focused = parentCtrl->getGuiOver() == this;
    if (cur) focused = false;
    if (focused || cur == this) {
        NVGcolor colHighlight;
        if (focused) {
            colHighlight = theme->getColor(GuiColor::COL_BG_DRK);
        } else {
            colHighlight = theme->getColor(GuiColor::COL_BG_DRKER);
        }
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, colHighlight);
        nvgFill(vg);
    }

    renderTextLabel(vg,
                    vec2(pos) + vec2(size)*0.5f,
                    vec2(size),
                    menu->getTitle(),
                    theme,
                    fontSize,
                    THEMECOL_TEXT,
                    NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
}


guimenu::guimenu(ngui::Menu* _menu, int _lvl, guimenu_ctxtentry* parent) : guictxtmenu() /*, menu(_menu)*/, parentSubmenuEntry(parent) {
    setLevel(_lvl);
    //this->size.x    = 190;
    this->maxHeight = 0;
    for (auto e : _menu->children) {
        if (e->type == ngui::menu_type::seperator) {
            addEntry(new ctxtmenu_splitter());
        } else {
            auto* entry = new guimenu_ctxtentry(e);
            addEntry(entry);
            guimenuEntries.push_back(entry);
        }
    }
}

void guimenu::layout() {
    for (auto entry : guimenuEntries) {
        entry->fixedLeftOffset = -1;
    }
    guictxtmenu::layout();
    float leftOffset = 0;
    for (auto entry : guimenuEntries) {
        leftOffset = math::max(entry->leftOffset(), leftOffset);
    }
    for (auto e : guimenuEntries) {
        e->fixedLeftOffset = leftOffset;
    }
}

void guimenu::onRemove() {
    if (this->parentMenuBar) {
        this->parentMenuBar->currentMenu = nullptr;
    }
    parentCtrl->closeAppMenusAtLvl(lvl);
    for (ctxtmenu_entry* e : entries) {
        auto* e2 = dynamic_cast<guimenu_ctxtentry*>(e);
        if (e2)
            e2->bIsMenuOpen = false;
    }
}

void guimenu::onParentWindowClose() {
    if (parentSubmenuEntry) {
        parentSubmenuEntry->bIsMenuOpen = false;
    }
}

bool guimenu::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        ivec2 local                 = toContainerSpace(mpos);
        guimenu_ctxtentry* entryHit = nullptr;
        for (guimenu_ctxtentry* e : guimenuEntries) {
            int n = e->getClicked(size, local);
            if (n >= 0) {
                entryHit = e;
                break;
            }
        }
        BaseCtrl* appCtrlParent = parentMenuBar->getControl();

        if (!entryHit || !entryHit->isMenuOpen() || entryHit->menu->type != ngui::menu_type::submenu) {
            //close other submenu at same level
            closeAllSubmenus();
            if (!parent || !parentCtrl)
                return true;
        } 
        if (entryHit && !entryHit->isMenuOpen() && entryHit->menu->type == ngui::menu_type::submenu) {
            entryHit->setIsMenuOpen(true);
            //and open new one
            auto* popup = new guimenu(entryHit->menu, getLevel() + 1, entryHit);
            popup->parentMenuBar = this->parentMenuBar;
            popup->size = math::maxvec2(vec2(APP_MENU_MIN_WIDTH, 0), popup->size);
            ivec2 popupPos = this->parentCtrl->toScreenSpace(toScreenSpace(ivec2(size.x, entryHit->y)) + ivec2(2, 0));
            appCtrlParent->openAppMenu(
                    popup->getLevel(),
                    popup,
                    popupPos,
                    WINDOW_IS_BORDERLESS | WINDOW_POS_ABSOLUTE);
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}

bool guimenu::clickedElement(ctxtmenu_entry* e, int _id) {
    dbgassert(this->parentMenuBar);
    BaseCtrl* ctrlParentBar = this->parentMenuBar->getControl();
    dbgassert(ctrlParentBar);
    auto* appCtrl = dynamic_cast<AppCtrl*>(ctrlParentBar);
    if (appCtrl) {
        auto* entry = dynamic_cast<guimenu_ctxtentry*>(e);
        if (entry) {
            if (_id > 0) {
                auto menuCommand = entry->menu->command;
                appCtrl->menuCommand(menuCommand);//possibly deletes this
            }
            if (entry->menu->type == ngui::menu_type::submenu) {
                return true;
            }
        }
        appCtrl->closeAllAppMenus();
    }
    return true;
}

void guictr_menubar::render(NVGcontext* vg) {
    if (!setScissorTransform(vg))
        return;
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, size.x, size.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_BRT));
    nvgFill(vg);
    for (guibase* gui : guis) {
        gui->render(vg);
    }
    const auto* buildVersion = BuildInfo::BUILD_BINARY_VERSION;
    float offsetX = 0.0f;
    if (buildVersion) {
        offsetX += size.y*0.5f + renderTextLabel(vg,
                        vec2(size.x - size.y*0.25f, size.y * 0.5f),
                        vec2(size),
                        buildVersion,
                        theme,
                        size.y,
                        theme->getColor(GuiColor::COL_LABEL_INACTIVE),
                        NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    }
    if (label.length() > 0) {
        renderTextLabel(vg,
                        vec2(size.x - size.y*0.25f - offsetX, size.y * 0.5f),
                        vec2(size),
                        label,
                        theme,
                        size.y * FONT_AUTOSCALE,
                        theme->getColor(GuiColor::COL_LABEL_INACTIVE),
                        NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    }
}
