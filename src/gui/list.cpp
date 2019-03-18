#include <nanovg.h>
#include <algorithm>
#include "list.h"
#include "gui.h"
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
		int32_t extImg = 2;
		int32_t iconW = rowHeight + extImg * 2;
		RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
		NVGpaint paintIcon = nvgImagePattern(vg, -extImg, -extImg, iconW, iconW, 0, image.id, 1.0f);
		nvgBeginPath(vg);
		nvgRect(vg, -extImg, -extImg, iconW, iconW);
		nvgFillPaint(vg, paintIcon);
		nvgFill(vg);
	}
	setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
	nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
	nvgTranslate(vg, -pos.x, -pos.y);
}

void gui_list_entry::handleDraggedBegin(MouseEvent& evt) {
}
