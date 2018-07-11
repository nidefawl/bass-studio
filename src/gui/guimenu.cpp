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
	AppCtrl::get()->closeAppMenus(lvl);
	for (ctxtmenu_entry* e : entries) {
		guimenu_ctxtentry* e2 = dynamic_cast<guimenu_ctxtentry*>(e);
		if (e2)
			e2->isMenuOpen = false;
	}
}
void guictr_menubar_entry::render(NVGcontext* vg) {
	guictr_menubar_entry* cur = parentMenuBar->currentMenu;

	bool focused = AppCtrl::get()->guiOver == this;
	if (cur) focused = false;
	NVGcolor* colHighlight = NULL;
	if (focused) colHighlight = &g_guiColors[COL_BG_DRK];
	if (cur == this) {
		colHighlight = &g_guiColors[COL_BG_DRKER];
	}
	if (colHighlight) {
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgFillColor(vg, *colHighlight);
		nvgFill(vg);
	}
	const char* cstr = StringAsCStr(menu->title);
	setFont(vg, fontSize, G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgText(vg, pos.x + size.x/2, pos.y+size.y/2, cstr, NULL);
}
