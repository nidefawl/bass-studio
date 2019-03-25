#include "clipeditor.h"
#include "gui.h"
#include "guicolors.h"
#include "track.h"
#include "track_impl.h"
#include "note.h"
#include "seq_time.h"
#include "cursor.h"
#include "keyboard.h"
#include "grid.h"
#include "leak_detect.h"

#include "guicontextmenu_daw.h"

namespace GuiColor {

extern constant_t COL_PIANOROLL_WHITE;
extern constant_t COL_PIANOROLL_BLACK;
extern constant_t COL_PIANOROLL_STROKE;
}
namespace GuiConstant {
extern constant_t CONST_PIANOROLL_STROKE_WIDTH;
}


gui_pianoroll::gui_pianoroll(clip_view& _view, layout_pianoroll_t& _layout) :
		guibase(), piano_scale(_layout, _view, size.y), view(_view) {
	keysX = 0;
	widthKeys = 0;
}

void gui_pianoroll::handleDraggedBegin(MouseEvent& evt) {
	if (evt.type == M_EVT_DOUBLECLICK) {
		parent->handleDraggedBegin(evt);
		return;
	}
	if (evt.guiDragged == this) {
		MainCtrl::get()->captureMouse(this);
		startDrag = evt.relMousepos;
		dragDirection = -1;
		dragPosObjSpace = toNoteF(evt.relMousepos.y);
	}
}

void gui_pianoroll::handleDraggedMove(MouseEvent& evt) {
	if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
		bool lockGesture = true;
		bool isMove = true;
		if (lockGesture) {
			if (dragDirection < 0) {
				float initialx = (float) (math::abs(evt.mousepos.x - evt.dragStart.x));
				float initialy = (float) (math::abs(evt.mousepos.y - evt.dragStart.y));
				if (initialx + initialy < 4)
					return;

				if (initialx > initialy)
					dragDirection = 1;
				else
					dragDirection = 0;
			}
			isMove = dragDirection == 0;
		}
		float distx = (float) ((evt.dragDistance->x));
		float disty = (float) ((evt.dragDistance->y));
		if ((!lockGesture && math::abs(disty) > 0) || (lockGesture && isMove)) {
			evt.dragDistance->y = 0;
			setOffset(layoutRoll.offset() + disty);
			//				grid.setOffset(grid.offset - distx);
		}
		if ((!lockGesture && math::abs(distx) > 0) || (lockGesture && !isMove)) {
			evt.dragDistance->x = 0;
			//				disty = 1.0f + disty * -0.01f;
			//				float anchor_dragposx = (float)(startDrag.x < 50 ? 0 : evt.relMousepos.x);
			setScale(layoutRoll.scale() + distx * 0.05f);
			int32_t rel = math::min(size.y - 1, math::max(0, size.y - evt.relMousepos.y));
			float offset = (size.y - toScreenF(dragPosObjSpace)) + layoutRoll.offset();
			setOffset(offset - rel);
			//				double newOffset = grid.calcOffset(anchor_dragposx, dragPosObjSpace);
			//				grid.setOffset((float)newOffset);
			//				MainCtrl::get()->updateGrid();
		}
	}
}

void gui_pianoroll::handleDraggedRelease(MouseEvent& evt) {
}

void gui_pianoroll::handleRightClick(MouseEvent& evt) {
}

bool gui_pianoroll::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (this->contains(mpos) && mpos.x - pos.x < keysX) {
		evt.requestFocus(this);
		return true;
	}
	return false;
}

void gui_pianoroll::layout() {
	keysX = size.x / 2;
	widthKeys = size.x - keysX;
}
void gui_pianoroll::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	float h = size.y;

	nvgBeginPath(vg);
	nvgRect(vg, keysX, -4, widthKeys, size.y+8);
	nvgFillColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_WHITE));
	nvgFill(vg);

	bool fold = layoutRoll.fold;
	float offset = layoutRoll.offset();
	float scale = layoutRoll.scale();
	int32_t firstKey = math::max((int32_t)floorf(offset/scale), 0);

	if (fold) {
		//render one extra key on top and bottom to fix antialiasing on edge of container
		if (firstKey > 0) {
			firstKey--;
		}

		firstKey = firstKey % 12;
		std::vector<int32_t> pitches;
		this->view.getNotePitches(pitches);
		float yOff = offset - firstKey*scale - scale;


		nvgSave(vg);
		nvgTranslate(vg, 0, yOff);

		int len = (int) pitches.size();
		nvgBeginPath(vg);
		float y = 0;
		for (int i = firstKey; i < len; i++) {
			int32_t pitch = pitches[i];
			if (isSharp(pitch)) {
				nvgRect(vg, keysX, h-y, widthKeys, scale);
			}
			y += scale;
			if (y >= size.y+scale*2) {
				break;
			}
		}
		nvgFillColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_BLACK));
		nvgFill(vg);
		nvgRestore(vg);
		yOff = offset - scale;

		nvgSave(vg);
		nvgTranslate(vg, 0, yOff);
		nvgBeginPath(vg);

		nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
		y = 0;
		float prevSeperator = -30;
		for (int i = 0; i <= len; i++) {
			if (y-prevSeperator > 25 || i == len) {
				int wSep = 55;
				if (i == len && y-prevSeperator < 25)
					wSep = 20;
				if ((h-y+scale)+offset > -30 && (h-y+scale)+offset < h+40) {
					nvgMoveTo(vg, keysX - wSep, h-y+scale);
					nvgLineTo(vg, keysX, h-y+scale);
				}
				prevSeperator = y;
			}
			if ((h-y+scale)+offset > -30 && (h-y+scale)+offset < h+40) {
				nvgMoveTo(vg, keysX, h-y+scale);
				nvgLineTo(vg, keysX+widthKeys, h-y+scale);
			}
			y += scale;
			if (y >= h+scale*2) {
				break;
			}
		}
		nvgStroke(vg);
		nvgRestore(vg);
		setFont(vg, 24, G_BLACK, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
		prevSeperator = -30;
		for (int i = 0; i < len; i++) {
			int32_t pitch = pitches[i];
			y = scale*i;
			if (y-prevSeperator > 25) {
				float textY = h - y + offset;
				if (textY > -20 && textY < size.y+scale+20) {
					const char* notename = noteName(pitch);
					nvgText(vg, 4, textY, notename, NULL);
				}
				prevSeperator = y;
			}
		}
	} else {
		nvgSave(vg);

		//render one extra key on top and bottom to fix antialiasing on edge of container
		if (firstKey > 0) {
			firstKey--;
		}
		float yOff = offset - firstKey*scale - scale;

		int32_t firstOctave = floorf(firstKey/12.0f);
		firstKey = firstKey % 12;

		nvgTranslate(vg, 0, yOff);

		float yoct = 0;
		for (int32_t octave = firstOctave; octave < MAX_OCTAVES; octave++) {
			float y = yoct;
			nvgBeginPath(vg);
			for (int i = firstKey; i < 12; i++) {
				if (isSharp(i)) {
					nvgRect(vg, keysX, h-y, widthKeys, scale);
				}
				y += scale;
				if (y >= size.y+scale*2) {
					break;
				}
			}
			nvgFillColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_BLACK));
			nvgFill(vg);
			y = yoct;

			nvgBeginPath(vg);
			nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
			nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
			if (firstKey == 0) {
				nvgMoveTo(vg, keysX - 55, h - (y-scale));
				nvgLineTo(vg, keysX, h - (y-scale));
			}
			if (firstKey == 0 && octave == 0) {
				nvgMoveTo(vg, keysX, h - (y-scale));
				nvgLineTo(vg, keysX+widthKeys, h - (y-scale));
			}
			nvgStroke(vg);

			nvgBeginPath(vg);
			for (int i = firstKey; i < 12; i++) {
				nvgMoveTo(vg, keysX, h-y);
				nvgLineTo(vg, keysX+widthKeys, h-y);
				y += scale;
				if (y >= size.y+scale*2) {
					break;
				}
			}
			nvgStroke(vg);
			yoct = y;
			if (yoct >= size.y+scale*2) {
				break;
			}
			firstKey = 0;
		}
		nvgRestore(vg);
		setFont(vg, 24, G_BLACK, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
		char buf[5];
		for (int32_t octave = 0; octave < MAX_OCTAVES; octave++) {
			float y = scale*octave*12;
			float textY = h - y + offset;
			if (textY > size.y+scale+20 || textY < -20) {
				continue;
			}

			snprintf(buf, sizeof(buf), "C%d", octave-2);
			nvgText(vg, 4, textY, buf, NULL);
		}
	}
	nvgBeginPath(vg);
	nvgMoveTo(vg, keysX+widthKeys, 0);
	nvgLineTo(vg, keysX+widthKeys, size.y);
	nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
	nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
	nvgStroke(vg);
}
