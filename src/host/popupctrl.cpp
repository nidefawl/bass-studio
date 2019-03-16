#include <nanovg.h>
#include <vector>
#include <glm/glm.hpp>

#include "window.h"

#include "keyboard.h"
#include "commands.h"

#include "mainctrl.h"
#include "seq_util.h"

#include "../gui/gui.h"
#include "../gui/guicontainer.h"
#include "../gui/guicontextmenu_base.h"
#include "../gui/guiscrollcontainer.h"
#include "../gui/scrollbar.h"

#include "leak_detect.h"

using glm::vec2;
using glm::ivec2;
using namespace std;

PopupCtrl::PopupCtrl() {
	popupCtrs = new guictr_scrollbar();
}
void PopupCtrl::focusLost() {
//	parentCtrl->closeContextMenu();
}
void PopupCtrl::closePopup() {
	popupCtrs->removeGuis();
	if (this->window)
		static_cast<window_overlay*>(this->window)->hide();
	if (guiCtrFocused) {
		if (!guiCtrFocused->isStaticContainer()) {
			guiCtrFocused = NULL;
		}
	}
	guiCaptured = guiFocused = guiOver = guiDragged = NULL;
}
void PopupCtrl::relayout(int32_t w, int32_t h) {
	popupCtrs->size = ivec2(w, h);
	popupCtrs->determineSize();
	popupCtrs->layout();
};
void PopupCtrl::open(guictxtmenu_base *_ctxtmenu, ivec2 pos) {
	mouseInside = false;
	this->m_mousePos = ivec2(-1111111);
	popupCtrs->removeGuis();
	popupCtrs->pos = ivec2(0);
	_ctxtmenu->pos = insetCtxtMenu;
	_ctxtmenu->setBackgroundRendered(false);
	_ctxtmenu->determineSize();
	_ctxtmenu->layout();
	canTakeInputFocus = _ctxtmenu->canTakeInputFocus;
	popupCtrs->maxHeight = _ctxtmenu->maxHeight;
	popupCtrs->scrollbarOutside = _ctxtmenu->scrollbarOutside;
	popupCtrs->add(_ctxtmenu);
//	ivec2 wndsize(0);
//	this->window->getSize(&wndsize);
//	wndsize.y = std::max(wndsize.y, popupCtrs->maxHeight);
	popupCtrs->size = _ctxtmenu->size;
	popupCtrs->determineSize();
	popupCtrs->layout();
	window_overlay* appW = static_cast<window_overlay*>(this->window);
	appW->positionOnScreen(pos-insetCtxtMenu, popupCtrs->size);
	appW->show();
}
void PopupCtrl::destroy() {
	isOK = false;
	this->containers.clear();
	this->containers.shrink_to_fit();
	delete popupCtrs;
	popupCtrs = nullptr;
}

PopupCtrl::~PopupCtrl() {
}
bool PopupCtrl::hasInputFocus() {
	return guiFocused && canTakeInputFocus;
}
bool PopupCtrl::init(window_overlay* _window, NVGcontext* nanovg)
{
	guitheme_t themeDefault;
	themeDefault.name = "default";
	themes.setTheme(themeDefault);
	themes.loadThemes();
	this->window = _window;
	this->vg = nanovg;
	this->containers.push_back(popupCtrs);
	for (guictr_base *ctr : containers) {
		ctr->setControl(this);
	}
	isOK = true;
	return isOK;
}
