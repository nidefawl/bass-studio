#pragma once
#include <nanovg_min.h>
#include <vector>
#include "math/seq_math.h"
#include "basectrl.h"
#include "gui.h"
#include "guicolors.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu.h"
#include "menu.h"

class guimenu_ctxtentry : public ctxtmenu_entry {
public:
    ngui::Menu* menu;
    bool isMenuOpen = false;
    guimenu_ctxtentry(ngui::Menu* _menu);
    void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override;
};
class guictr_menubar;
class guimenu : public guictxtmenu {
    //ngui::Menu* menu;
    int lvl = 0;
    std::vector<guimenu_ctxtentry*> guimenuEntries;
    guimenu_ctxtentry* const parentSubmenuEntry;

public:
    guictr_menubar* parentMenuBar = NULL;
    guimenu(ngui::Menu* _menu, int _lvl = 0, guimenu_ctxtentry* parent = nullptr);
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
    int fontSize = 0;
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
    std::vector<guictr_menubar_entry> list;
    ngui::MenuBar& menubar;

public:
    guictr_menubar_entry* currentMenu = NULL;
    guictr_menubar(ngui::MenuBar& _menubar) : guictr_base(), menubar(_menubar) {
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
        NVGcontext* vg = parentCtrl->vg;
        int fontSize   = (int) (size.y * 0.8);
        int padding    = math::max(4, (int) (size.y * 0.8));
        int x          = 0;
        int y          = 0;

        std::vector<ngui::Menu*> list = menubar.children;
        setFont(vg, fontSize, G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (ngui::Menu* m : list) {
            guictr_menubar_entry* entry = new guictr_menubar_entry(m, this);

            const char* cstr   = StringAsCStr(m->title);
            float textBound[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            nvgStaticTextBounds(vg, 0, 0, cstr, nullptr, textBound);

            int textW = (int) (textBound[2] - textBound[0]);


            entry->pos      = ivec2(x, y);
            entry->size     = ivec2(textW + padding, size.y);
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
        guimenu* popup       = new guimenu(entry->menu, 0);
        popup->parentMenuBar = this;
        popup->size.x        = 250;
        parentCtrl->openAppMenu(0, popup, entry->toScreenSpace(ivec2(0, entry->size.y)) - popup->pos + ivec2(1));
        currentMenu = entry;
    }
    void hoverMenu(guictr_menubar_entry* entry) {
        if (currentMenu != NULL && currentMenu != entry) {
            openMenu(entry);
        }
    }
};
