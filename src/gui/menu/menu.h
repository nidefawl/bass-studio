#pragma once
#include <nanovg_min.h>
#include <vector>
#include "math/seq_math.h"
#include "basectrl.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu.h"
#include "menu.h"

class guimenu_ctxtentry : public ctxtmenu_entry {
public:
    ngui::Menu* menu;
    bool isMenuOpen = false;
    explicit guimenu_ctxtentry(ngui::Menu* _menu);
    void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override;
    void layout(ivec2 size, float _fontSize, determine_string_width& strw) override;
};
class guictr_menubar;
class guimenu : public guictxtmenu {
    //ngui::Menu* menu;
    int lvl = 0;
    std::vector<guimenu_ctxtentry*> guimenuEntries;
    guimenu_ctxtentry* const parentSubmenuEntry;

public:
    guictr_menubar* parentMenuBar = nullptr;
    explicit guimenu(ngui::Menu* _menu, int _lvl = 0, guimenu_ctxtentry* parent = nullptr);
    ~guimenu() override = default;

    void clicked(int _id) override;

    void clickedElement(ctxtmenu_entry* e, int _id) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void onRemove() override;
    void onParentWindowClose() override;
    void layout() override;
};
class guictr_menubar_entry : public guibase {
public:
    ngui::Menu* const menu;
    guictr_menubar* const parentMenuBar;
    float fontSize = 0;
    int padding  = 0;
    guictr_menubar_entry(ngui::Menu* _menu, guictr_menubar* _parentMenuBar) : guibase(), menu(_menu), parentMenuBar(_parentMenuBar) {
    }
    void render(NVGcontext* vg) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    guibase* getFocusedControl() override {
        return nullptr;
    }
    guibase* getFocusedContainer() override {
        return nullptr;
    }
};
class guictr_menubar : public guictr_base {
    //std::vector<guictr_menubar_entry> list;
    ngui::MenuBar& menubar;

public:
    guictr_menubar_entry* currentMenu = nullptr;
    explicit guictr_menubar(ngui::MenuBar& _menubar) : guictr_base(), menubar(_menubar) {
        padding = 0;
    }
    ~guictr_menubar() override {

        destroyGuis();
    }
    void render(NVGcontext* vg) override;
    void updateMenu() {
        layout();
    }
    void layout() override {
        dbgassert(parentCtrl);
        dbgassert(parentCtrl->vg);
        parentCtrl->closeAllAppMenus();
        destroyGuis();

        auto fontSize = size.y * 0.8f;
        int padding   = math::max(1, math::roundfS32(size.y * 0.5f));

        std::vector<ngui::Menu*> entryList = menubar.children;
        determine_string_width strw(parentCtrl, theme);

        int x = 0;
        int y = 0;
        for (ngui::Menu* m : entryList) {
            auto* entry = new guictr_menubar_entry(m, this);

            auto textW = strw.getStringWidth(m->title, fontSize, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

            entry->pos      = ivec2(x, y);
            entry->size     = ivec2(math::roundfS32(textW) + padding, size.y);
            entry->fontSize = fontSize;
            entry->padding  = padding;
            add(entry);
            x = entry->right();
        }
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    void openMenu(guictr_menubar_entry* entry) {
        parentCtrl->closeAllAppMenus();
        parentCtrl->closeContextMenu();
        parentCtrl->onMenuOpen(entry->menu);
        auto* popup = new guimenu(entry->menu, 0);
        popup->parentMenuBar = this;
        popup->size = math::maxvec2(vec2(APP_MENU_MIN_WIDTH, 0), popup->size);
        parentCtrl->openAppMenu(0, popup, entry->toScreenSpace(ivec2(0, entry->size.y)) - popup->pos + ivec2(1));
        currentMenu = entry;
    }
    void hoverMenu(guictr_menubar_entry* entry) {
        if (currentMenu && currentMenu != entry) {
            openMenu(entry);
        }
    }
};
