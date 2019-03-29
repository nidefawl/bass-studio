#include <nanovg.h>
#include <algorithm>
#include "list.h"
#include "gui.h"
#include "mouse.h"
#include "event.h"
#include "guicolors.h"
#include "guicontainer.h"
#include "renderresources.h"
#include "basectrl.h"


void gui_list_entry::handleDraggedMove(MouseEvent& evt) {
	parentCtrl->objectDragMove(this, evt);
}

void gui_list_entry::handleDraggedRelease(MouseEvent& evt) {
	parentCtrl->objectDragRelease(this, evt);
}

void gui_list_entry::render(NVGcontext* vg) {
	BaseCtrl* ctrl = parentCtrl;
	float spacing = INSET_TITLE;
	float x = spacing;
	float rowHeight = size.y;
	if (icon > -1) {
		x += rowHeight + spacing;
	}
	if (ctrl->isCtrOrChildFocused(this)) {
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER));
		nvgFill(vg);
	}
	nvgTranslate(vg, pos.x, pos.y);
	if (icon > -1) {
		RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
		drawIcon(vg, size, &image);
	}
	setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
	nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
	nvgTranslate(vg, -pos.x, -pos.y);
}

void gui_list_entry::handleDraggedBegin(MouseEvent& evt) {
}
bool gui_list::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (this->contains(mpos)) {
		ivec2 localMouse = this->toContainerSpace(mpos);
		if (scrollbar.mouseHitTest(localMouse, evt)) {
			//				my_printf("clicked on %s %d\n", scrollbar.getClassName().c_str(), (int) h);
			return true;
		}
		ivec2 localMouseOffset = localMouse;
		if (first < last) {
			gui_list_entry* g = listGuis[first];
			localMouseOffset.y += g->top();
		}
		for (int32_t idx = first; idx < last; idx++) {
			if (listGuis[idx]->mouseHitTest(localMouseOffset, evt)) {
				//					my_printf("clicked on %s %s %d\n", listGuis[idx]->getClassName().c_str(), listGuis[idx]->getText().c_str(), (int) h);
				return true;
			}
		}
		evt.requestFocus(this);
		//			my_printf("clicked on %s %d\n", getClassName().c_str(), (int) h);
		return true;
	}
	return false;
}
