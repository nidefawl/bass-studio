#include <nanovg.h>
#include <vector>

#include "window.h"

#include "keyboard.h"
#include "commands.h"

#include "mainctrl.h"
#include "seq_util.h"

#include "../gui/gui.h"
#include "../gui/guicontainer.h"
#include "../gui/guicontextmenu.h"
#include "../gui/scrollbar.h"

#include "leak_detect.h"

using glm::vec2;
using glm::ivec2;
using namespace std;


#define INSET_CTXT_MENU_X 1
#define INSET_CTXT_MENU_Y 2
static const ivec2 insetCtxtMenu = ivec2(INSET_CTXT_MENU_X, INSET_CTXT_MENU_Y);

class guictr_popup : public guictr_base, public gui_scrollcontainer {
	gui_scrollbar scrollbar;
public:
	int contentHeight = 0;
	bool hasScrollbar = false;
	bool scrollbarOutside = false;
	int maxHeight = 220;
	guictr_popup() : guictr_base(), scrollbar(1, 0.0f, *this) {
		padding = 0;
	}
	~guictr_popup() {
	}
	virtual void render(NVGcontext* vg) {
//		renderBackground(vg);
//		if (!setScissorTransform(vg)) {
//			return;
//		}
		nvgBeginPath(vg);
		nvgMoveTo(vg, pos.x + size.x, pos.y);
		nvgLineTo(vg, pos.x, pos.y);
		nvgLineTo(vg, pos.x, pos.y + size.y);
		nvgLineTo(vg, pos.x + size.x, pos.y + size.y);
		nvgLineTo(vg, pos.x + size.x, pos.y);
		nvgStrokeColor(vg, g_guiColors[COL_CTXTMNU_OUTLINE]);
		nvgStrokeWidth(vg, 2);
		nvgStroke(vg);
		ivec2 ipos=pos+ivec2(1);
		ivec2 isize=size-ivec2(2);
		nvgBeginPath(vg);
		nvgRect(vg, ipos.x, ipos.y, isize.x, isize.y);
		nvgFillColor(vg, g_guiColors[COL_CTXTMNU_BG]);
		nvgFill(vg);
//		nvgTranslate(vg, insetCtxtMenu.x, insetCtxtMenu.y);
//		ctxtmenu->render(vg);
		for (auto c : guis) {
			nvgSave(vg);
			c->render(vg);
			nvgRestore(vg);
		}
//		knobTest.render(vg);
	}
	void layout() override {
		hasScrollbar = false;
		size = ivec2(0);
		for (guibase* gui : guis) {
			gui->layout();
			size.x = max(size.x, gui->right());
			size.y = max(size.y, gui->bottom());
		}
		contentHeight = size.y;
		if (maxHeight > 0 && size.y > maxHeight+5) {
			size.y = maxHeight-5;
			hasScrollbar = true;
			guis.insert(guis.begin(), &scrollbar);
			scrollbar.parent = this;
		}
		size += ivec2(insetCtxtMenu*2);
		ivec2 cs = getSizeContent();
		if (hasScrollbar) {
			if (scrollbarOutside) {
				int scrollW = gui_scrollbar::smallW;
				scrollbar.size = ivec2(scrollW-2, cs.y-2);
				scrollbar.pos = ivec2(cs.x, 1);
				size.x += scrollW+2;
			} else {
				int scrollW = gui_scrollbar::defaultW;
				int entryW = cs.x - scrollW;
				scrollbar.size = ivec2(scrollW-2, cs.y-2);
				scrollbar.pos = ivec2(cs.x-scrollW+1, 1);
				for (guibase* gui : guis) {
					if (gui == &scrollbar)
						continue;
					gui->size.x = min(entryW, gui->size.x);
				}
			}
			scrollOffsetChanged(1, scrollbar.scrollOffset);
		}
	}
	bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override {
		if (this->contains(v)) {
			ivec2 localMouse = this->toContainerSpace(v);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
		}
		return false;
	}

	ivec2 getScrollTotalSize() override {
		ivec2 cs = getSizeContent();
		cs.y = contentHeight;
		return cs;
	}
	ivec2 getScrollViewSize() override {
		return getSizeContent();
	}
	void scrollOffsetChanged(int dir, float offset) {
		if (hasScrollbar) {
			for (guibase* gui : guis) {
				if (gui == &scrollbar)
					continue;
				gui->pos.y = -offset*(contentHeight-size.y);
			}
		}
	}
	virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
		return scrollbar.handleMouseScroll(evt, xoffset, yoffset);
	}
};
PopupCtrl::PopupCtrl() {
	popupCtrs = new guictr_popup();
}
void PopupCtrl::focusLost() {
//	AppCtrl::get()->closeContextMenu();
}
void PopupCtrl::close() {
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
void PopupCtrl::onCursorEnter(int entered) {
	mouseInside = entered;
}
void PopupCtrl::open(guictxtmenu_base *_ctxtmenu, ivec2 pos) {
	mouseInside = false;
	this->m_mousePos = ivec2(-1111111);
	popupCtrs->removeGuis();
	_ctxtmenu->ctrl = this;
	popupCtrs->pos = ivec2(0);
	_ctxtmenu->pos = insetCtxtMenu;
	popupCtrs->maxHeight = _ctxtmenu->maxHeight;
	popupCtrs->scrollbarOutside = _ctxtmenu->scrollbarOutside;
	popupCtrs->add(_ctxtmenu);
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
}

PopupCtrl::~PopupCtrl() {
}
bool PopupCtrl::init(window_overlay* _window, NVGcontext* nanovg)
{
	this->window = _window;
	this->vg = nanovg;
	this->containers.push_back(popupCtrs);
	isOK = true;
	return isOK;
}
