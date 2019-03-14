#include "guimenu.h"
#include "../host/mainctrl.h"


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
void guimenu::onRemove() {
	if (this->parentMenuBar) {
		this->parentMenuBar->currentMenu = NULL;
	}
	parentCtrl->closeAppMenus(lvl);
	for (ctxtmenu_entry* e : entries) {
		guimenu_ctxtentry* e2 = dynamic_cast<guimenu_ctxtentry*>(e);
		if (e2)
			e2->isMenuOpen = false;
	}
}
void guictr_menubar_entry::render(NVGcontext* vg) {
	guictr_menubar_entry* cur = parentMenuBar->currentMenu;

	bool focused = parentCtrl->guiOver == this;
	if (cur) focused = false;
	if (focused||cur == this) {
		NVGcolor colHighlight;
		if (focused) {
			colHighlight = theme->getColor(COL_BG_DRK);
		} else {
			colHighlight = theme->getColor(COL_BG_DRKER);
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

bool guimenu::mouseHitTest(ivec2 mpos, MouseHitEvt& evt)  {
	if (this->contains(mpos)) {
		ivec2 local = toContainerSpace(mpos);
		AppCtrl* appCtrlParent = dynamic_cast<AppCtrl*>(parentMenuBar->getControl());
		assert(appCtrlParent);
		guimenu_ctxtentry* e2 = NULL;
		for (ctxtmenu_entry* e : entries) {
			int n = e->getClicked(size, local);
			if (n >= 0) {
				e2 = dynamic_cast<guimenu_ctxtentry*>(e);
				break;
			}
		}
		if (e2 && e2->menu->type == ngui::menu_type::submenu) {
			if (!e2->isMenuOpen) {
				guimenu *popup = new guimenu(e2->menu, lvl+1);
				popup->parentMenuBar = this->parentMenuBar;
				popup->size.x = 250;
				appCtrlParent->closeAppMenus(lvl);
				ivec2 vPos;
				parentCtrl->window->getPos(&vPos);
				vPos.y+=e2->y;
				vPos.x+=right()+2;
				appCtrlParent->openAppMenu(
					popup->lvl-1,
					popup,
					vPos);
			}
			e2->isMenuOpen = true;
		} else {
			for (ctxtmenu_entry* e : entries) {
				guimenu_ctxtentry* e2 = dynamic_cast<guimenu_ctxtentry*>(e);
				if (e2)
					e2->isMenuOpen = false;
			}
			appCtrlParent->closeAppMenus(lvl);
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
			appCtrl->menuCommand(_id);//deletes this
		}
		if (lvl == 0) { //then reads here, gg
			appCtrl->closeContextMenu();
			appCtrl->closeAppMenus();
		} else {
			appCtrl->closeAppMenus(lvl);
		}
	}
}
