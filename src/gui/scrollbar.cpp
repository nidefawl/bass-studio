#include <nanovg.h>
#include "math/vec.h"
#include "scrollbar.h"

#include "gui.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "theme.h"
#include "basectrl.h"
#include "splitter.h"


namespace GuiConstant {

extern constant_t CONST_ROUND;
}

void gui_scrollbar::render(NVGcontext* vg) {
	float fRnd = theme->getFloat(GuiConstant::CONST_ROUND);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, pos.x, pos.y, size.x, size.y, fRnd);
	NVGcolor bg = theme->getColor(GuiColor::COL_BG_DRK);
	nvgFillColor(vg, bg);
	nvgFill(vg);
	ivec2 vcS = ctr.getScrollTotalSize();
	ivec2 vs = ctr.getScrollViewSize();
	if (vcS[dir] > 0 && vcS[dir] > vs[dir]) {
		vec2 barOff(0);
		vec2 barS = size;
		barS[dir] = math::min((float) size[dir], (vs[dir] / (float) vcS[dir]) * size[dir]);
		barOff[dir] = (size[dir] - barS[dir]) * scrollOffset;
		int32_t inset = 1;
		nvgBeginPath(vg);
		const int minHandleHeight = 14;
		if (barS[dir] < minHandleHeight) {
			float h = minHandleHeight-barS[dir];
			barOff[dir] -= h/2.0;
			barS[dir] = minHandleHeight;
		}
		nvgRoundedRect(vg, pos.x + barOff.x + inset, pos.y + barOff.y + inset, barS.x - inset * 2, barS.y - inset * 2, fRnd);

		bool focused = parentCtrl->guiCtrFocused == this->parent || (parentCtrl->guiDragged == NULL && parentCtrl->guiOver == this);
		if (focused) {
			//				nvgStrokeWidth(vg, 1.0f);
			//				nvgStrokeColor(vg, theme->getColor(GuiColor::COL_BG_DRK_FOCUSED));
			//				nvgStroke(vg);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_GUI_HANDLE_FOCUSED));
		} else {
			nvgFillColor(vg, theme->getColor(GuiColor::COL_GUI_HANDLE));
		}
		nvgFill(vg);

	}
}

gui_scrollbar::gui_scrollbar(int _dir, float _offset, gui_scrollcontainer& _ctr) :
		guibase(), dir(_dir), ctr(_ctr), scrollOffset(_offset) {
	setCanMouseHit(true);
}


void Splitter::render(NVGcontext* vg) {
	if (!isVisible()) {
		log_printf("warning, skip rendering container with state !isVisible()\n", 0);
		return;
	}
	if (isBackgroundRendered()) {
		renderBackground(vg);
	}
	if (!setScissorTransform(vg)) {
		return;
	}
	for (auto c : guis) {
		if (!c->isVisible()) {
//			log_printf("warning, skip rendering child container with state !isVisible()\n", 0);
			continue;
		}
		if (c->size.x <= 0 || c->size.y <= 0) {
			log_printf("warning, skip rendering child container with size <= 0 0\n", 0);
			continue;
		}
    }
    if (parentCtrl && (parentCtrl->getGuiFocused() == this || parentCtrl->guiOver == this))
    {
        nvgSave(vg);
        nvgBeginPath(vg);
        if (this->type) {
            nvgRect(vg, 0, 0, size.x, size.y);
        }
        else {
            nvgRect(vg, 0, 0, size.x, size.y);
        }
        nvgStrokeColor(vg, G_WHITE);
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
        nvgRestore(vg);
    }
}
