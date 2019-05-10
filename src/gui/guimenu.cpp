#include "guimenu.h"
#include "math/vec.h"
#include "renderresources.h"

guimenu_ctxtentry::guimenu_ctxtentry(ngui::Menu* _menu)
	: ctxtmenu_entry(_menu->title, _menu->command), menu(_menu)
{
	int32_t iconId = menu->icon;
	if (iconId > -1) {
		setIcon(&RenderResources::imgIcons[iconId]);
	}

}

void guimenu_ctxtentry::render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
	if (contains(ctxtSize, mouse)) {
		nvgBeginPath(vg);
		nvgRect(vg, 0, y, ctxtSize.x, height);
		nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
		nvgFill(vg);
	}
	if (this->icon) {
		ivec2 iconSize(height, height);
		nvgTranslate(vg, height / 4, y);
		drawIcon(vg, iconSize, icon);
		nvgTranslate(vg, -height / 4, -y);
	}
	//		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(title), NULL);
	String t1 = title;
	String t2;
	auto p = title.find("\t");
	if (p != String::npos) {
		t1 = title.substr(0, p);
		t2 = title.substr(p + 1);
	}
	setFont(vg, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(t1), NULL);
	int32_t defoffset = (int32_t) round(this->fontSize/2.4f);
	if (t2.length()) {
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		nvgText(vg, width - defoffset, y + height / 2, StringAsCStr(t2), NULL);
	}
	if (menu->type == ngui::menu_type::submenu) {
		nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		nvgText(vg, width - defoffset, y + height / 2, ">", NULL);
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
	guictr_menubar_entry* cur = parentMenuBar->currentMenu;

	bool focused = parentCtrl->guiOver == this;
	if (cur) focused = false;
	if (focused||cur == this) {
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
	const char* cstr = StringAsCStr(menu->title);
	setFont(vg, fontSize, G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgText(vg, pos.x + size.x/2, pos.y+size.y/2, cstr, NULL);
}


guimenu::guimenu(ngui::Menu* _menu, int _lvl, guimenu_ctxtentry* parent) :
		guictxtmenu() /*, menu(_menu)*/, lvl(_lvl), parentSubmenuEntry(parent) {
	this->size.x = 190;
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
	int leftOffset = 0;
	for (auto entry : guimenuEntries) {
		leftOffset = math::max(entry->leftOffset(), leftOffset);
	}
	for (auto e : guimenuEntries) {
		e->fixedLeftOffset = leftOffset;
	}
}

void guimenu::onRemove() {
	if (this->parentMenuBar) {
		this->parentMenuBar->currentMenu = NULL;
	}
	parentCtrl->closeAppMenusAtLvl(lvl);
	for (ctxtmenu_entry* e : entries) {
		guimenu_ctxtentry* e2 = dynamic_cast<guimenu_ctxtentry*>(e);
		if (e2)
			e2->isMenuOpen = false;
	}
}

void guimenu::onParentWindowClose() {
	if (parentSubmenuEntry) {
		parentSubmenuEntry->isMenuOpen = false;
	}
}

bool guimenu::mouseHitTest(ivec2 mpos, MouseHitEvt& evt)  {
	if (this->contains(mpos)) {
		ivec2 local = toContainerSpace(mpos);
		guimenu_ctxtentry* entryHit = NULL;
		for (guimenu_ctxtentry* e : guimenuEntries) {
			int n = e->getClicked(size, local);
			if (n >= 0) {
				entryHit = e;
				break;
			}
		}
		BaseCtrl* appCtrlParent = parentMenuBar->getControl();

		auto closeAllSubmenus = [this, appCtrlParent]() {
			bool anyOpen = false;
			for (guimenu_ctxtentry* e : guimenuEntries) {
				anyOpen |= e->isMenuOpen;
				e->isMenuOpen = false;
			}
			if (anyOpen) {
				//close all menus deeper than this menu
				appCtrlParent->closeAppMenusAtLvl(lvl + 1);
			}
		};
		//close lvl+1 windows if we didn't hit any menu
		if (!entryHit || entryHit->menu->type != ngui::menu_type::submenu) {
			//TODO: maybe defer closing for usability
			closeAllSubmenus();
		}
		if (entryHit && entryHit->menu->type == ngui::menu_type::submenu) {
			if (!entryHit->isMenuOpen) {
				//close other submenu at same level
				closeAllSubmenus();
				//and open new one
				guimenu *popup = new guimenu(entryHit->menu, lvl+1, entryHit);
				popup->parentMenuBar = this->parentMenuBar;
				popup->size.x = 250;
				ivec2 vPos(right()+2, pos.y+entryHit->y);
				appCtrlParent->openAppMenu(
					popup->lvl,
					popup,
					vPos);
				entryHit->isMenuOpen = true;
			}
		}
		evt.requestFocus(this);
		return true;
	}
	return false;
}

void guimenu::clicked(int _id) {
	assert(this->parentMenuBar);
	BaseCtrl* ctrlParentBar = this->parentMenuBar->getControl();
	assert(ctrlParentBar);
	AppCtrl* appCtrl = dynamic_cast<AppCtrl*>(ctrlParentBar);
	if (appCtrl) {
		if(_id > 0) {
			appCtrl->menuCommand(_id);//possibly deletes this
		}
		appCtrl->closeAllAppMenus();
	}
}
