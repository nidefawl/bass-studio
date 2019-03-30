#include <nanovg.h>
#include <vector>
#include "math/seq_math.h"
#include "math/vec.h"
#include "window.h"

#include "keyboard.h"
#include "commands.h"

#include "basectrl.h"
#include "seq_util.h"

#include "../gui/gui.h"
#include "../gui/guicontainer.h"
#include "../gui/guicontextmenu_base.h"
#include "../gui/guiscrollcontainer.h"
#include "../gui/scrollbar.h"


using namespace std;

PopupCtrl::PopupCtrl() {
}

PopupCtrl::~PopupCtrl() {
}

void PopupCtrl::focusLost() {
//	parentCtrl->closeContextMenu();
}

void PopupCtrl::closePopup() {
	if (isShown()) {
		static_cast<window_overlay*>(this->window)->hide();
	}
}

void PopupCtrl::onWindowClose() {
	popupCtrs->removeGuis();
	if (guiCtrFocused) {
		if (!guiCtrFocused->isStaticContainer()) {
			guiCtrFocused = NULL;
		}
	}
	guiCaptured = guiFocused = guiOver = guiDragged = NULL;
}

bool PopupCtrl::onWindowCloseRequest() {
	return true;
}

void PopupCtrl::relayout(int32_t w, int32_t h) {

	// Popup window shouldn't change its shape, just call layout
//	ivec2 prefSize(w, h)
//	popupCtrs->determineSize(prefSize);
//	popupCtrs->size = prefSize;
	popupCtrs->layout();
}

void PopupCtrl::open(guictxtmenu_base *_ctxtmenu, ivec2 pos) {
	assert(!isShown());
	mouseInside = false;
	this->m_mousePos = ivec2(-1111111);
	popupCtrs->removeGuis();
	popupCtrs->pos = ivec2(0);
	_ctxtmenu->pos = insetCtxtMenu;
	canTakeInputFocus = _ctxtmenu->canTakeInputFocus;
	popupCtrs->maxHeight = _ctxtmenu->maxHeight;
	popupCtrs->scrollbarOutside = _ctxtmenu->scrollbarOutside;
	popupCtrs->setBackgroundRendered(_ctxtmenu->isBackgroundRendered());
	_ctxtmenu->determineSize(_ctxtmenu->size);
	_ctxtmenu->layout();



	popupCtrs->size = vec2(_ctxtmenu->size.x, math::max(0, popupCtrs->maxHeight));
	popupCtrs->add(_ctxtmenu);
	popupCtrs->determineSize(popupCtrs->size);
	popupCtrs->layout();
	window_overlay* appW = static_cast<window_overlay*>(this->window);
	appW->positionOnScreen(pos-insetCtxtMenu, popupCtrs->size);
	appW->show();
	int32_t clearc = getTheme()->getColorInt32(GuiColor::COL_CLEAR_COLOR);
	if (popupCtrs->isBackgroundRendered()) {
		clearc |= 0xFF000000;
	} else {
		clearc &= 0x00FFFFFF;
	}
	getTheme()->setColor(GuiColor::COL_CLEAR_COLOR, clearc);
}

void PopupCtrl::destroy() {
	isOK = false;
	this->containers.clear();
	this->containers.shrink_to_fit();
	delete popupCtrs;
	popupCtrs = nullptr;
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
	popupCtrs = new guictr_scrollbar();
	popupCtrs->setBackgroundRendered(true);
	this->containers.push_back(popupCtrs);
	for (guictr_base *ctr : containers) {
		ctr->setControl(this);
	}
	isOK = true;
	return isOK;
}
