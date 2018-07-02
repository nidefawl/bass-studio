#include "tracktimeline.h"
#include <stdbool.h>
#include <stdint.h>
#include <vector>
#include "seq_math.h"
#include "exceptions.h"
#include "seq_util.h"
#include "color_util.h"
#include "platform.h"

void guitrack_timeline::handleDraggedBegin(MouseEvent& evt) {
	if (evt.guiDragged == this) {
		MainCtrl::get()->captureMouse(this);
		startDrag = evt.relMousepos;
		dragDirection = -1;
		float anchor_dragposx = (float)(startDrag.x < 50 ? 0 : evt.relMousepos.x);
		dragPosObjSpace = grid.toObjSpace(anchor_dragposx);
	}
}
void guitrack_timeline::handleDraggedMove(MouseEvent& evt) {
	if (evt.guiDragged == this) {
		bool lockGesture = true;
		bool isMove = true;
		if (lockGesture) {
			if (dragDirection < 0) {
				float initialx = (float)abs(evt.mousepos.x - evt.dragStart.x);
				float initialy = (float)abs(evt.mousepos.y - evt.dragStart.y);
				if (initialx + initialy < 4)
					return;
				if (initialx > initialy) dragDirection = 1;
				else dragDirection = 0;
			}
			isMove = dragDirection == 1;
		}
		float distx = (float)(evt.dragDistance->x);
		float disty = (float)(evt.dragDistance->y);

		if (abs(distx) && (!lockGesture || (lockGesture && isMove))) {
			int prevOffset = grid.offset;
			grid.setOffset(grid.offset - evt.dragDistance->x);
			evt.dragDistance->x = 0;
//				assert(grid.offset != prevOffset);
//				MainCtrl::get()->updateGrid();
			grid.notifyChange();
		}

		if ((!lockGesture && abs(disty) > 0) || (lockGesture && !isMove)) {
			evt.dragDistance->y = 0;
			disty = 1.0f + disty * -0.01f;
			float anchor_dragposx = (float)(startDrag.x < 50 ? 0 : evt.relMousepos.x);
			grid.setZoom(grid.zoom * disty);
			double newOffset = grid.calcOffset(anchor_dragposx, dragPosObjSpace);
			grid.setOffset((int)newOffset);
			grid.notifyChange();
//				MainCtrl::get()->updateGrid();
		}
	}
}
void guitrack_timeline::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	std::vector<grid_div>& gridList = grid.gridList;
	if (gridList.size() == 0)
		return;
	int printoffset = 1;
//		grid_div& first = gridList[0];
	int gap = 16;
	///String text = String.format("%d.%d.%d", first.bar + printoffset, first.bar_sub + printoffset, first.bar_sub_sub + printoffset);
	float textWidth = 34;
	float barSize = grid.bar_size;
	int step = 1;
	while (barSize < textWidth) {
		step *= 2;
		barSize *= 2;
	}
	int substeps = -1;
	if (barSize > textWidth) {
		substeps = 4;
		while (substeps > 1 && barSize > textWidth * 2) {
			substeps /= 2;
			barSize /= 2;
		}
	}
	int subsubsteps = -1;
	if (barSize > textWidth) {
		subsubsteps = 8;
		while ((subsubsteps > 1 && barSize > textWidth*subsubsteps * 2 * 2)) {
			subsubsteps /= 2;
			barSize /= 2;
		}
	}
//		float lastDrawnRight = 0;
	int len = gridList.size();
	String text;
	int fontSize;

	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
	float scale = 1.33f;
	for (int a = 0; a < 2; a++) {
		for (int i = 0; i < len; i += 1) {
			grid_div& n = gridList[i];
			beatbar16th_t notePos = n.pos;
			if ((n.time&TICK_MASK_16TH) != 0)
				continue;
			if (notePos.bar % step == 0) {
				if ((notePos.beat == 0 || ((substeps > 0 && notePos.beat%substeps == 0)))
					&& (notePos.th == 0 || (subsubsteps > 0 && notePos.th%subsubsteps == 0))) {
					if (a == 0) {
						int h = 10;

						if (notePos.beat != 0 || notePos.th != 0) {
							h = 5;

						}
						nvgBeginPath(vg);
						nvgMoveTo(vg, n.screenpos, this->size.y);
						nvgLineTo(vg, n.screenpos, this->size.y - h*scale);
						nvgStrokeWidth(vg, 1.f);
						nvgStrokeColor(vg, G_WHITE);
						nvgStroke(vg);
					} else {
						int color = -1;
						if (notePos.beat != 0 || notePos.th != 0) {
							/*		if (n.screenpos + gap / 2 < lastDrawnRight)
							continue;*/
							fontSize = 14;
							if (notePos.th != 0) {
								color = 0xababab;
								text = StringFormat(".%d.%d", notePos.beat + printoffset, notePos.th + printoffset);
							} else {
								text = StringFormat("%d.%d", notePos.bar + printoffset, notePos.beat + printoffset);
							}

						}
						else {
							fontSize = 16;
							text = StringFormat("%d", notePos.bar + printoffset);
						}
						nvgFontSize(vg, fontSize*scale);
						nvgFillColor(vg, rgbToNvg(color));
						nvgText(vg, n.screenpos + gap / 2, this->size.y, StringAsCStr(text), NULL);
						if (this->size.y > 28) {
							text = StringFormat("%d", n.time);
							nvgFontSize(vg, fontSize*scale*0.66f);
//								nvgText(vg, n.screenpos + gap / 2, this->size.y-21, StringAsCStr(text), NULL);
							text = StringFormat("%f", n.screenpos);
							nvgText(vg, n.screenpos + gap / 2, this->size.y-21, StringAsCStr(text), NULL);
						}
					}
				}

			}

		}
	}
}

