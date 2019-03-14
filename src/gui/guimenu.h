#pragma once
#include <nanovg.h>
#include "basectrl.h"
#include "gui.h"
#include "guicolors.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "guicontainer.h"
#include "guicontextmenu.h"
#include "menu.h"
#include <vector>

class guimenu_ctxtentry : public ctxtmenu_entry {
public:
	ngui::Menu* menu;
	bool isMenuOpen = false;
	guimenu_ctxtentry(ngui::Menu* _menu) : ctxtmenu_entry(_menu->title, _menu->command), menu(_menu) {

	}
	virtual void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
		if (contains(ctxtSize, mouse)) {
			nvgBeginPath(vg);
			nvgRect(vg, 0, y, ctxtSize.x, height);
			nvgFillColor(vg, theme->getColor(COL_CTXTMNU_HILIGHT));
			nvgFill(vg);
		}
//		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(title), NULL);
		String t1 = title;
		String t2;
		auto p = title.find("\t");
		if (p != String::npos) {
			t1 = title.substr(0, p);
			t2 = title.substr(p+1);
		}
		setFont(vg, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(t1), NULL);
		if (t2.length()) {
			nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
			nvgText(vg, width-leftOffset(), y + height / 2, StringAsCStr(t2), NULL);
		}
		if (menu->type == ngui::menu_type::submenu) {
			nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
			nvgText(vg, width-leftOffset(), y + height / 2, ">", NULL);
		}
	}
};
class guictr_menubar;
class guimenu : public guictxtmenu {
	//ngui::Menu* menu;
	int lvl = 0;
public:
	guictr_menubar* parentMenuBar = NULL;
	guimenu(ngui::Menu* _menu, int _lvl = 0) : guictxtmenu()/*, menu(_menu)*/, lvl(_lvl) {
		this->size.x = 190;
		this->maxHeight = 0;
		for (auto e : _menu->children) {
			if (e->type == ngui::menu_type::seperator) {
				addEntry(new ctxtmenu_splitter());
			} else {
				addEntry(new guimenu_ctxtentry(e));
			}
		}
		 my_printf("guimenu\n", 0);
	}
	~guimenu() {
	 my_printf("~guimenu\n", 0);
	}
	void clicked(int _id);
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	virtual void onRemove();
};
class guictr_menubar_entry : public guibase {
public:
	ngui::Menu* const menu;
	guictr_menubar* const parentMenuBar;
	int fontSize = 0;
	int padding = 0;
	guictr_menubar_entry(ngui::Menu* _menu, guictr_menubar* _parentMenuBar) : guibase(), menu(_menu), parentMenuBar(_parentMenuBar) {
	}
	virtual void render(NVGcontext* vg);
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	virtual void handleDraggedBegin(MouseEvent& evt);
	virtual guibase* getFocusedControl() {
		return nullptr;
	}
	virtual guibase* getFocusedContainer() {
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
	~guictr_menubar() {

		destroyGuis();
	}
	virtual void render(NVGcontext* vg) {
		setScissorTransform(vg);
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgFillColor(vg, theme->getColor(COL_BG_BRT));
		nvgFill(vg);
		for (guibase* gui : guis) {
			gui->render(vg);
		}
	}
	void updateMenu() {
		layout();
	}
	void layout() {
		parentCtrl->closeContextMenu();
		parentCtrl->closeAppMenus();
		destroyGuis();
		NVGcontext* vg = parentCtrl->vg;
		int fontSize = (int) (size.y * 0.8);
		int padding = max(4, (int) (size.y * 0.8));
		int x = 0;
		int y = 0;
		std::vector<ngui::Menu*> list = menubar.children;
		setFont(vg, fontSize, G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		for (ngui::Menu* m : list) {
			const char* cstr = StringAsCStr(m->title);
	        float textBound[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	        nvgStaticTextBounds(vg, 0, 0, cstr, nullptr, textBound);
	        int textW = (int) (textBound[2] - textBound[0]);
			guictr_menubar_entry* entry = new guictr_menubar_entry(m, this);
			entry->pos = ivec2(x, y);
			entry->size = ivec2(textW+padding, size.y);
			entry->fontSize = fontSize;
			entry->padding = padding;
			add(entry);
			x = entry->right();
		}
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	void openMenu(guictr_menubar_entry* entry) {
		parentCtrl->closeContextMenu();
		guimenu *popup = new guimenu(entry->menu, 0);
		popup->parentMenuBar = this;
		popup->size.x = 250;
		parentCtrl->openContextMenu(popup, entry->toScreenSpace(ivec2(0, entry->size.y)) - popup->pos + ivec2(1));
		currentMenu = entry;
	}
	void hoverMenu(guictr_menubar_entry* entry) {
		if (currentMenu != NULL && currentMenu != entry) {
			openMenu(entry);
		}
	}
};
