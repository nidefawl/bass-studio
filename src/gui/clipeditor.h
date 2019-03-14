#pragma once
#include <list>
#include <vector>
#include "seq_math.h"
#include "str_util.h"
#include "color_util.h"
#include "clip.h"
#include "track.h"
#include "gui.h"
#include "guicontainer.h"
#include "tracktimeline.h"
#include "button.h"
#include "trackcontent.h"
#include "tempocontrols.h"
#include "inputfield.h"
#include "note.h"
#include "grid.h"
#include "keyboard.h"
#include "edithistory.h"
#include "leak_detect.h"
#include "../host/mainctrl.h"
#include "guiarp.h"
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
using glm::vec2;
using glm::ivec2;

#define PIANO_COLOR_WHITE rgbToNvg(0xffffff)
#define PIANO_COLOR_BLACK rgbToNvg(0x111111)
#define PIANO_COLOR_STR rgbToNvg(0x444444)
#define CONTENT_COLOR_SHARP rgbaToNvg(0x33111111)
#define MAX_OCTAVES (8-(-2))
#define PIANOROLL_MIN_SCALE 4
#define PIANOROLL_MAX_SCALE 48
inline bool isSharp(int n) {
	n = n%12;
	switch (n) {
	case 1:
	case 3:
	case 6:
	case 8:
	case 10:
		return true;
	}
	return false;
}

class piano_scale {
private:
	int& sizeY;
	clip_view& clipview;
protected:
	layout_pianoroll_t& layoutRoll;
public:
	piano_scale(layout_pianoroll_t& _layout, clip_view& _clipview, int& _sizeY)
	: sizeY(_sizeY),
	  clipview(_clipview),
	  layoutRoll(_layout)
	{

	}
	float toNoteFNoFolding(int32_t y) {
		int32_t rel = (sizeY) - y;
		float offsetKey = rel + layoutRoll.offset();
		float note = offsetKey / layoutRoll.scale();
		return note;
	}
	float toNoteFImpl(int32_t y, const bool clamp) {
		int32_t rel = (sizeY) - y;
		float offsetKey = rel + layoutRoll.offset();
		float note = offsetKey / layoutRoll.scale();
		if (layoutRoll.fold) {
			if (clamp) {
				return this->clipview.unfoldNoteClamped(note);
			} else {
				return this->clipview.unfoldNote(note);
			}
		}
		return note;
	}
	float toNoteFClamped(int32_t y) {
		return toNoteFImpl(y, true);
	}
	float toNoteF(int32_t y) {
		return toNoteFImpl(y, false);
	}
	float toScreenF(float note) {
		if (layoutRoll.fold) {
			std::vector<int32_t> pitches;
			note = this->clipview.toFoldNote(note);
		}
		float offsetKey = note * layoutRoll.scale();
		float rel = offsetKey - layoutRoll.offset();
		return (sizeY) - rel;
	}
	void setOffset(float f) {
		this->layoutRoll.offset() = f < -(layoutRoll.scale()*MAX_OCTAVES*1) ? -(layoutRoll.scale()*MAX_OCTAVES*1) : f > layoutRoll.scale()*(MAX_OCTAVES-1)*12 ? layoutRoll.scale()*(MAX_OCTAVES-1)*12 : f;
	}
	void setScale(float f) {
		this->layoutRoll.scale() = f < PIANOROLL_MIN_SCALE ? PIANOROLL_MIN_SCALE : f > PIANOROLL_MAX_SCALE ? PIANOROLL_MAX_SCALE : f;
	}
	void showRange(int32_t noteFrom, int32_t noteTo) {
		noteTo++;
		int32_t nNotes = abs(noteFrom - noteTo);
		float rangeScale = sizeY / (float) nNotes;
		setScale(rangeScale); //TODO: maybe only zoom out here, not in (or determine on upper level)
		setOffset(min(noteFrom, noteTo)*layoutRoll.scale());
	}
	void makeNoteVisible(int32_t noteFrom) {
		float foldNote = layoutRoll.fold ? this->clipview.toFoldNote(noteFrom) : noteFrom;
		float offsetNote = foldNote*layoutRoll.scale();
		if (offsetNote < layoutRoll.offset()) { // below visible area
			setOffset(offsetNote);
		} else if (offsetNote > layoutRoll.offset()+sizeY-layoutRoll.scale()) {
			setOffset(offsetNote-(sizeY-layoutRoll.scale()));
		}
	}
};
class gui_pianoroll : public guibase, piano_scale {
	float keysX, widthKeys;


	ivec2 startDrag;
	int dragDirection = -1;
	float dragPosObjSpace = 0;
	clip_view& view;
public:
	gui_pianoroll(clip_view& _view, layout_pianoroll_t& _layout) : guibase(), piano_scale(_layout, _view, size.y), view(_view) {
		keysX = 0; widthKeys = 0;
	}
	~gui_pianoroll() {
	}
	void render(NVGcontext* vg);

	void handleDraggedBegin(MouseEvent& evt) {
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
	void handleDraggedMove(MouseEvent& evt) {
		if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
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
				isMove = dragDirection == 0;
			}
			float distx = (float)(evt.dragDistance->x);
			float disty = (float)(evt.dragDistance->y);

			if ((!lockGesture && abs(disty) > 0) || (lockGesture && isMove)) {
				evt.dragDistance->y = 0;
				setOffset(layoutRoll.offset() + disty);
//				grid.setOffset(grid.offset - distx);
			}

			if ((!lockGesture && abs(distx) > 0) || (lockGesture && !isMove)) {
				evt.dragDistance->x = 0;
//				disty = 1.0f + disty * -0.01f;
//				float anchor_dragposx = (float)(startDrag.x < 50 ? 0 : evt.relMousepos.x);
				setScale(layoutRoll.scale() + distx*0.05f);
				int32_t rel = min(size.y-1, max(0, size.y - evt.relMousepos.y));
				float offset = (size.y - toScreenF(dragPosObjSpace)) + layoutRoll.offset();
				setOffset(offset - rel);

//				double newOffset = grid.calcOffset(anchor_dragposx, dragPosObjSpace);
//				grid.setOffset((float)newOffset);
//				MainCtrl::get()->updateGrid();
			}
		}
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
	}
	virtual void handleRightClick(MouseEvent& evt) {
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos) && mpos.x-pos.x<keysX) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void layout() {

		keysX = size.x/2;
		widthKeys = size.x-keysX;
	}
};
class gui_clipsettings : public guictr_base {
public:
	scaled_grid& grid;
	clip_view& view;
	guibutton btnLoop;
	gui_timeinput clipLoopStart;
	gui_timeinput clipLoopLen;
	gui_timeinput clipTimeStart;
	gui_timeinput clipTimeLen;
	gui_timeinput clipTimeStartOffsetTicks;
	gui_numberinput_field clipTimeStartOffsedSamples;
	gui_numberinput_field clipAudioId;
	gui_clipsettings(scaled_grid& _grid, clip_view& _view)
		: guictr_base(),
		grid(_grid),
		view(_view), clipLoopStart(nullptr), clipLoopLen(nullptr, false), clipTimeStart(nullptr), clipTimeLen(nullptr, false),
		clipTimeStartOffsetTicks(nullptr), clipTimeStartOffsedSamples(nullptr), clipAudioId(nullptr)
	{
		padding = 2; margin = 0;
		btnLoop.drawFn = drawTextureSymbol;
		btnLoop.drawParm = ICON_LOOP;
		btnLoop.setEnabledRef(nullptr);
		clipLoopStart.setRef(nullptr);
		clipLoopLen.setRef(nullptr);
		clipTimeStart.setRef(nullptr);
		clipTimeLen.setRef(nullptr);
		clipTimeStartOffsetTicks.setRef(nullptr);
		clipTimeStartOffsedSamples.setRef(nullptr);
		clipAudioId.setRef(nullptr);
		btnLoop.setLabel("Loop");
		clipLoopStart.setLabel("Loop Position");
		clipLoopLen.setLabel("Loop Length");
		clipTimeStart.setLabel("Clip Position");
		clipTimeLen.setLabel("Clip Length");
		clipTimeStartOffsetTicks.setLabel("Tick offset");
		clipTimeStartOffsedSamples.setLabel("Sample offset");
		clipAudioId.setLabel("Sample ID");
		add(&btnLoop);
		add(&clipLoopStart);
		add(&clipLoopLen);
		add(&clipTimeStart);
		add(&clipTimeLen);
		add(&clipTimeStartOffsetTicks);
		add(&clipTimeStartOffsedSamples);
		add(&clipAudioId);
	}
	~gui_clipsettings()
	{
		remove(&clipAudioId);
		remove(&clipTimeStartOffsedSamples);
		remove(&clipTimeStartOffsetTicks);
		remove(&clipTimeLen);
		remove(&clipTimeStart);
		remove(&clipLoopLen);
		remove(&clipLoopStart);
		remove(&btnLoop);
	}
	void render(NVGcontext* vg);

	void layout();
	void renderBackground(NVGcontext* vg) override {
		drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
	}
	void buttonClicked(guibase* button) override {
		if (&btnLoop == button) {
			clip_t* clip = view.clip();
			if (clip != NULL) {
				clip->loopEnabled = !clip->loopEnabled;
			}
		}
		if (&btnLoop == button || &clipTimeStart == button || &clipLoopStart == button || &clipTimeLen == button || &clipTimeStartOffsedSamples == button
				|| &clipTimeStartOffsetTicks == button || &clipLoopLen == button) {
			clip_t* clip = view.clip();
			if (clip && clip->gClip) {
				clip->setDirty();
				track_t* track = clip->gClip->m_track;
				if (track) {
					resizeOtherClips(track->getMidi(), clip);
					MainCtrl::getGuiTrackCtr()->layout();
					MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
				}
			}
		}
	}
	void showEditClip() {
		clip_t* clip = view.clip();
		if (clip != NULL) {
			btnLoop.setEnabledRef(&clip->loopEnabled);
			clipLoopStart.setRef(&clip->loopStart);
			clipLoopLen.setRef(&clip->loopLen);
			clipTimeStart.setRef(&clip->time);
			clipTimeLen.setRef(&clip->getLenRef());
			clipTimeStartOffsedSamples.setRef(&clip->offsetSamples);
			clipTimeStartOffsetTicks.setRef(&clip->offsetStart);
			clipAudioId.setRef(&clip->audio.id);
//			if (clip->noLayout) {
//				grid.showRange(clip->offsetStart, clip->offsetStart+clip->len);
//				zoomPianoRollToClipsNoteRange();
//			} else {
//				clip_editor_layout_t& layout = clip->editorLayout;
//				grid.setLayout(layout.layoutGrid);
//				setLayout(layout.layoutPianoRoll);
//			}
		} else {

			btnLoop.setEnabledRef(nullptr);
			clipLoopStart.setRef(nullptr);
			clipLoopLen.setRef(nullptr);
			clipTimeStart.setRef(nullptr);
			clipTimeLen.setRef(nullptr);
			clipTimeStartOffsedSamples.setRef(nullptr);
			clipTimeStartOffsetTicks.setRef(nullptr);
			clipAudioId.setRef(nullptr);
		}
	}
};
class gui_clipcontent : public guictr_base, public piano_scale {
public:
	enum dragmode {
		drag_none,
		drag_frame,
		drag_notes_move,
		drag_notes_copy,
		drag_note_left,
		drag_note_right,
	};
	clip_cursor_t dragStartCursor;
	dragmode dragMode = drag_none;
	std::set<note_t*> selectionStart;
	ivec2 dragBegin=ivec2(0);
	ivec2 dragTo=ivec2(0);
	note_t beginDragNote;
	scaled_grid& grid;
	clip_view& view;
	gui_clipcontent(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout)
		: guictr_base(), piano_scale(_layout, _view, size.y),
		grid(_grid),
		view(_view)
	{
		padding = 0;
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {

			ivec2 local = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(local, evt)) {
					return true;
				}
			}
			if (view.clip() && evt.type <= MouseHitType::MOUSE_RIGHT) {
				int32_t pitch = toNoteF(local.y);
				tick_t tickExact = grid.screenToTickSnap(local.x, SNAP_OFF);
				const note_t* contextNote = view.clip()->notes.get(tickExact, pitch);
				if (contextNote && local.x-grid.tickToScreenD(contextNote->start())<DRAG_RANGE) {
					evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
				}
				if (contextNote && grid.tickToScreenD(contextNote->end())-local.x<DRAG_RANGE) {
					evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void setStatusText();
	void expandSelectionFrame(std::pair<note_t*, note_t*> minMax);
	void setSelectionFrame(std::pair<note_t*, note_t*> minMax);
	void mergeDraggedNotes(dragmode mergeMode);
	void handleRightClick(MouseEvent& evt);
	void handleDraggedBegin(MouseEvent& evt);
	void handleDraggedMove(MouseEvent& evt);
	void handleDraggedRelease(MouseEvent& evt);
	bool handleKeyInput(KeyEvent& kevt);
	void render(NVGcontext* vg);

	void layout() {
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
protected:
	void setGlobalSelectionFromClipSelection();
};
class ce_constants {
protected:
	const int32_t heightTimeLine = 26;
	const int32_t heightSelIndicator = 8;
	const int32_t heightLoopInidicator = 14;
	const int32_t heightClipIndicators = heightSelIndicator+heightLoopInidicator*2;
};
class guictr_cliphandles : public guibase, ce_constants {
	scaled_grid& grid;
	clip_view& view;
	enum dragmode {
		drag_handle_none,
		drag_handle_left,
		drag_handle_right,
		drag_handle_loopleft,
		drag_handle_loopright,
		drag_handle_loopbar
	};
	dragmode dragHandle = drag_handle_none;
public:
	ivec2 clipViewSize;
	guictr_cliphandles(scaled_grid& _grid, clip_view& _view) :
			guibase(), grid(_grid), view(_view) {

	}
	int32_t dragOffset = 0;
	void handleDraggedBegin(MouseEvent& evt) {
		dragHandle = drag_handle_none;
		clip_t* clip = view.clip();
		if (!clip) {
			return;
		}
		ivec2 local = evt.relMousepos;
		dragHandle = getDragZone(local);
		dragOffset = local.x-(int32_t)grid.tickToScreenD(clip->loopStart);
	}
	void handleDraggedMove(MouseEvent& evt) {
		clip_t* clip = view.clip();
		if (!clip)
			return;
		track_t* track = view.track();
		if (!track)
			return;
		if (dragHandle == drag_handle_none) {
			return;
		}
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		trackdata_midi_t& midi = track->getMidi();
		clip_t* clNext = midi.getNextClip(clip);
		assert(clNext == NULL || (clNext != clip));
		assert(clNext == NULL || clNext->start() >= clip->end());
		int32_t mousePosX = evt.relMousepos.x;
		if (dragHandle == drag_handle_loopbar) {
			mousePosX -= dragOffset;
		}
		tick_t tickAt = grid.screenToTickSnap(mousePosX, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
		tick_t curEnd = clip->offsetStart + clip->getLen();
		tick_t curLoopEnd = clip->loopStart + clip->loopLen;
		if (dragHandle == drag_handle_right) {
			tick_t tickDelta = (tickAt - curEnd);
			tick_t newLen = clip->getLen()+tickDelta;
			if (newLen > 0) {
				tick_t newEnd = clip->start() + newLen;
				if (clNext && newEnd >= clNext->start()) {
					clip->setLen(clNext->start() - clip->start());
				} else {
					clip->setLen(newLen);
				}
			}
		}
		if (dragHandle == drag_handle_left) {
			tick_t curStart = clip->offsetStart;
			tick_t tickDelta = (tickAt - curStart);
			tick_t newStart = clip->offsetStart+tickDelta;
			if (newStart < curEnd) {
				tick_t newLen = curEnd-newStart;
				tick_t newEnd = clip->start() + newLen;
				if (clNext && newEnd >= clNext->start()) {
					clip->setLen(clNext->start() - clip->start());
					clip->offsetStart = curEnd - clip->getLen();
				} else {
					clip->offsetStart = newStart;
					clip->setLen(curEnd-newStart);
				}
			}
		}
		if (dragHandle == drag_handle_loopright) {
			tick_t tickDelta = (tickAt - curLoopEnd);
			tick_t newLen = clip->loopLen + tickDelta;
			if (newLen > 0) {
				clip->loopLen = newLen;
			}
		}
		if (dragHandle == drag_handle_loopleft) {
			tick_t curLoopStart = clip->loopStart;
			tick_t tickDelta = (tickAt - curLoopStart);
			tick_t newStart = clip->loopStart + tickDelta;
			if (newStart < curLoopEnd) {
				clip->loopStart = newStart;
				clip->loopLen = curLoopEnd - newStart;
			}
		}
		if (dragHandle == drag_handle_loopbar) {
			tick_t curLoopStart = clip->loopStart;
			tick_t tickDelta = (tickAt - curLoopStart);
			bool inLoop = clip->offsetStart >= clip->loopStart;
			clip->loopStart += tickDelta;
			if (inLoop && clip->offsetStart < clip->loopStart) {
				clip->offsetStart += clip->loopLen;
			}
			if (inLoop && clip->offsetStart >= clip->loopStart+clip->loopLen) {
				clip->offsetStart -= clip->loopLen;
			}
		}
		clip->setDirty();
		MainCtrl::get()->updateVisibleTrackContents();
	}
	void handleDraggedRelease(MouseEvent& evt) {
		dragHandle = drag_handle_none;
	}
	float dist(float x, float y, ivec2 mpos) {
		x = x - mpos.x;
		y = y - mpos.y;
		return x*x+y*y;
	}
	dragmode getDragZone(ivec2 local) {
		if (view.clip()) {
			struct dist_draghandle {
				float dist = 0;
				dragmode mode = drag_handle_none;
			};;
			float dragTop = heightLoopInidicator/2.0f;
			float dragBottom = dragTop + heightLoopInidicator;
			float distBar = std::numeric_limits<float>::max();
			float barSX = clipLoopStartScrX();
			float barEX = clipLoopEndScrX();
			if (local.x >= barSX && local.x < barEX
					&& local.y >= heightLoopInidicator && local.y < heightLoopInidicator*2) {
				distBar = DRAG_RANGE*DRAG_RANGE*0.8f;
			}
			std::vector<dist_draghandle> hndls {
				{dist(clipStartScrX(), dragTop, local), dragmode::drag_handle_left},
				{dist(clipEndScrX(), dragTop, local), dragmode::drag_handle_right},
				{dist(barSX, dragBottom, local), dragmode::drag_handle_loopleft},
				{dist(barEX, dragBottom, local), dragmode::drag_handle_loopright},
				{distBar, dragmode::drag_handle_loopbar}
			};
			std::sort(hndls.begin(), hndls.end(), [](dist_draghandle const & a, dist_draghandle const & b) {
				return a.dist  < b.dist;
			});
			if (hndls[0].dist < DRAG_RANGE*DRAG_RANGE) {
				return hndls[0].mode;
			}
		}
		return drag_handle_none;
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 local = this->toContainerSpace(mpos);
			if (view.clip() && evt.type <= MouseHitType::MOUSE_RIGHT) {
				dragmode mode = getDragZone(local);
				if (mode == dragmode::drag_handle_loopleft || mode == dragmode::drag_handle_left) {
					evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
					evt.requestFocus(this);
					return true;
				}
				if (mode == dragmode::drag_handle_loopright || mode == dragmode::drag_handle_right) {
					evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
					evt.requestFocus(this);
					return true;
				}
				if (mode == dragmode::drag_handle_loopbar) {
					evt.requestCursor(CURSOR_RESIZE_H);
					evt.requestFocus(this);
					return true;
				}
			}
		}
		return false;
	}
	float clipStartScrX() {
		return (float)grid.tickToScreenD(view.clip()->offsetStart);
	}
	float clipEndScrX() {
		return (float)grid.tickToScreenD(view.clip()->offsetStart + view.clip()->getLen());
	}
	float clipLoopStartScrX() {
		return (float)grid.tickToScreenD(view.clip()->loopStart);
	}
	float clipLoopEndScrX() {
		return (float)grid.tickToScreenD(view.clip()->loopStart + view.clip()->loopLen);
	}
	void render(NVGcontext* vg) {
		ivec2 cs = clipViewSize;
		if (cs.y <= 0 || cs.x <= 0) {
			return;
		}
		MainCtrl* ctrl = MainCtrl::get();
		tick_t clipOffset = (view.clip()) ? view.clip()->getOffsetStart() : 0;
		nvgIntersectScissor(vg, pos.x, pos.y, cs.x, cs.y);
		nvgTranslate(vg, pos.x, pos.y);
		nvgBeginPath(vg);
		nvgRect(vg, -2, 0, cs.x+2, size.y);
		nvgFillColor(vg, theme->getColor(COL_GRID_DRK));
		nvgFill(vg);
//		{
//
//			float w = (float)size.x;
//			float bgRepeat = grid.incr_bg*2.0f;
//			float bgOffset = (float)std::fmod(grid.offset, bgRepeat);
//			int steps_bg = (int)ceil((w + bgRepeat) / grid.incr_bg);
//			float x = -bgOffset;
//			for (int i = 0; i < steps_bg; i+=2)
//			{
//				nvgBeginPath(vg);
//				nvgRect(vg, x, 0, grid.incr_bg, size.y);
//				nvgFillColor(vg, theme->getColor(COL_GRID_DRK));
//				nvgFill(vg);
//				x += grid.incr_bg*2.0f;
//				if (x > w)
//					break;
//			}
//		}

		for (grid_div g : grid.gridList) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, g.screenpos, 0);
			nvgLineTo(vg, g.screenpos, heightLoopInidicator*2);
			nvgStrokeColor(vg, theme->getColor(COL_LINE_BAR + g.color));
			nvgStrokeWidth(vg, g.thickness);
			nvgStroke(vg);
		}
		nvgBeginPath(vg);
		nvgRect(vg, -2, heightLoopInidicator*2, cs.x+2, heightSelIndicator);
		nvgFillColor(vg, theme->getColor(COL_BG_DRKER2));
		nvgFill(vg);

		Cursor& c = ctrl->cursor;
		if (view.clip()) {
			const NVGcolor colLI = GUI_COLOR(120);
			const NVGcolor colLIStroke = theme->getFrameColorOutline();
			const float strokeWidthLI = 1.0f;
			const float wLoopInidicator = heightLoopInidicator;

			float tickBeginX = clipStartScrX();
			float tickEndX = clipEndScrX();

			int yOffset = 0;

			if (!(tickBeginX - wLoopInidicator > cs.x || tickEndX + wLoopInidicator < 0)) {
				float barBeginX = max(-wLoopInidicator, tickBeginX);
				float barEndX = min(cs.x+wLoopInidicator, tickEndX);
				NVGcolor color = rgbToNvg(view.clip()->rgb);
				nvgBeginPath(vg);
				nvgRect(vg, barBeginX, yOffset, barEndX - barBeginX, heightLoopInidicator*2);
				nvgFillColor(vg, color);
				nvgFill(vg);
			}
			if (tickBeginX > -wLoopInidicator && tickBeginX < cs.x + wLoopInidicator) {
				nvgBeginPath(vg);
				nvgMoveTo(vg, tickBeginX, yOffset);
				nvgLineTo(vg, tickBeginX, yOffset+cs.y);
				nvgStrokeColor(vg, colLI);
				nvgStrokeWidth(vg, strokeWidthLI);
				nvgStroke(vg);
				drawTri(vg, tickBeginX, yOffset, heightLoopInidicator, 0, colLI, colLIStroke, strokeWidthLI);

			}

			if (tickEndX > -wLoopInidicator && tickEndX < cs.x + wLoopInidicator) {
				nvgBeginPath(vg);
				nvgMoveTo(vg, tickEndX, yOffset);
				nvgLineTo(vg, tickEndX, yOffset+cs.y);
				nvgStrokeColor(vg, colLI);
				nvgStrokeWidth(vg, strokeWidthLI);
				nvgStroke(vg);
				drawTri(vg, tickEndX, yOffset, heightLoopInidicator, 1, colLI, colLIStroke, strokeWidthLI);
			}
			yOffset += heightLoopInidicator;
			tickBeginX = clipLoopStartScrX();
			tickEndX = clipLoopEndScrX();
			if (!(tickBeginX - wLoopInidicator > cs.x || tickEndX + wLoopInidicator < 0)) {
				float barBeginX = max(-wLoopInidicator, tickBeginX);
				float barEndX = min(cs.x + wLoopInidicator, tickEndX);
				nvgBeginPath(vg);
				nvgRect(vg, barBeginX, yOffset, barEndX-barBeginX, heightLoopInidicator);

				nvgFillColor(vg, colLI);
				nvgFill(vg);
				nvgStrokeColor(vg, colLIStroke);
				nvgStrokeWidth(vg, strokeWidthLI);
				nvgStroke(vg);

				if (tickBeginX > -wLoopInidicator && tickBeginX < cs.x + wLoopInidicator) {
					nvgBeginPath(vg);
					nvgMoveTo(vg, tickBeginX, yOffset);
					nvgLineTo(vg, tickBeginX, yOffset+cs.y);
					nvgStrokeColor(vg, colLI);
					nvgStrokeWidth(vg, strokeWidthLI);
					nvgStroke(vg);
					drawTri(vg, tickBeginX, yOffset, wLoopInidicator, 0, colLI, colLIStroke, strokeWidthLI);
				}


				if (tickEndX > -wLoopInidicator && tickEndX < cs.x + wLoopInidicator) {
					nvgBeginPath(vg);
					nvgMoveTo(vg, tickEndX, yOffset);
					nvgLineTo(vg, tickEndX, yOffset+cs.y);
					nvgStrokeColor(vg, colLI);
					nvgStrokeWidth(vg, strokeWidthLI);
					nvgStroke(vg);
					drawTri(vg, tickEndX, yOffset, wLoopInidicator, 1, colLI, colLIStroke, strokeWidthLI);


				}
			}
			yOffset += heightLoopInidicator;
		}
		if (c.selRange) {
			int32_t tickBegin = c.getTickBegin() - clipOffset;
			int32_t tickEnd = c.getTickEnd() - clipOffset;
			float tickBeginX = (float) grid.tickToScreenD(tickBegin);
			float tickEndX = (float) grid.tickToScreenD(tickEnd);
			if (tickEndX > -4.0f && tickBeginX < cs.x + 4.0f) {
				tickBeginX = CLAMP_I(tickBeginX, -4.0f, cs.x + 3.0f);
				tickEndX = CLAMP_I(tickEndX, -3.0f, cs.x + 4.0f);
				float width = (float) (tickEndX - tickBeginX);
				nvgBeginPath(vg);
				nvgRect(vg, (float) tickBeginX, heightLoopInidicator * 2.0f, width, heightSelIndicator);
				nvgFillColor(vg, G_SELECTION);
				nvgFill(vg);
			}
		}
//		-view.clip->start()+view.clip->offsetStart
		clip_t* clip = view.clip();
		if (clip) {
			tick_t pos = ctrl->playbackPos - clip->time + clip->offsetStart;
			if (clip->loopEnabled && clip->loopLen > 0) {
				if (pos > clip->loopStart) {
					pos = clip->loopStart + (pos - clip->loopStart) % clip->loopLen;
				}
			}
			float playBackX = (float) grid.tickToScreenD(pos);
			if (playBackX > -4.0f && playBackX < cs.x + 4.0f) {
				nvgBeginPath(vg);
				nvgMoveTo(vg, playBackX, 0);
				nvgLineTo(vg, playBackX, cs.y);
				nvgStrokeColor(vg, GUI_COLOR(120));
				nvgStrokeWidth(vg, 3);
				nvgStroke(vg);
				nvgBeginPath(vg);
				nvgMoveTo(vg, playBackX, 0);
				nvgLineTo(vg, playBackX, cs.y);
				nvgStrokeColor(vg, GUI_COLOR(250));
				nvgStrokeWidth(vg, 1);
				nvgStroke(vg);
			}
		}
	}
};
class guictr_noteeditor : public guictr_base, public layout_pianoroll_t, grid_changed_cb, ce_constants {
public:

	scaled_grid grid;
	gui_pianoroll piano;
	gui_clipcontent content;
	guitrack_timeline timeline;
	guictr_cliphandles clipHandles;
	clip_view& view;
	guibutton btn;

	guictr_noteeditor(clip_view& _view)
	: guictr_base(), layout_pianoroll_t(),
	  piano(_view, *this),
	  content(grid, _view, *this),
	  timeline(grid),
	  clipHandles(grid, _view),
	  view(_view)
	{
		padding = 2;
		grid.showRange(0, TICKS_BAR*4);
		grid.addCallback(this);
		add(&piano);
		add(&content);
		add(&timeline);
		add(&clipHandles);
		add(&btn);
		btn.setText("Fold");
		btn.setEnabledRef(&fold);
		btn.setActiveColor(nvgToRGB(theme->getColor(COL_NOTE)));
		content.showRange(2*12, 4*12);
	}
	~guictr_noteeditor() {
		remove(&btn);
		remove(&timeline);
		remove(&content);
		remove(&piano);
		remove(&clipHandles);
	}
	virtual void buttonClicked(guibase* button) {
		if (button == &btn) {
			fold = !fold;
			view.updateNotePitches(true);
			if (fold&&yscalefold==0&&yoffsetfold==0) {
				zoomPianoRollToClipsNoteRange();
			}
		}
	}
	int32_t getTotalWidth() {
		return max(10000, getSizeContent().x);
	}
//	virtual ivec2 toContainerSpace(ivec2 in) {
//		ivec2 offsetPos = in - getPosContent();
//		offsetPos.x += scrolloffset;
//		return offsetPos;
//	}
	void renderBackground(NVGcontext* vg) override {
		drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
	}
	void render(NVGcontext* vg);
	void layout() {
		ivec2 cs = getSizeContent();
		piano.pos = ivec2(0, heightTimeLine+heightClipIndicators);
		piano.size = ivec2(100, cs.y-heightTimeLine-heightClipIndicators);
		timeline.pos = ivec2(piano.right(), 0);
		timeline.size = ivec2(cs.x-piano.size.x, heightTimeLine);

		clipHandles.pos = ivec2(timeline.left(), timeline.bottom());
		clipHandles.size = ivec2(timeline.size.x, heightClipIndicators);
		btn.pos = ivec2(padding, padding);
		btn.size = ivec2((piano.size.x)/2, 18);
		content.pos = ivec2(timeline.left(), clipHandles.bottom());
		content.size = ivec2(timeline.size.x, piano.size.y);
		clipHandles.clipViewSize = ivec2(content.size.x, content.size.y+clipHandles.size.y);
		grid.update(content.size);

		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	void gridChanged(scaled_grid& _grid) override {
		ivec2 cs = getSizeContent();
		_grid.update(ivec2(timeline.size.x, cs.y-30));
	}
	void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == &piano) {
			if (evt.type == M_EVT_DOUBLECLICK)
				zoomPianoRollToClipsNoteRange();
			return;
		}
	}
	void zoomPianoRollToClipsNoteRange() {
		clip_t* clip = view.clip();
		if (!clip) {
			content.showRange(2*12, 4*12);
			return;
		}
		int32_t minSemi = clip->notes.minNote.pitch;
		int32_t maxSemi = clip->notes.maxNote.pitch;
		if (fold) {
			minSemi = 0;
			maxSemi = view.notePitches.size();
		}
		int32_t range = abs(maxSemi-minSemi);
		if (range<6) {
			int32_t add = 6-range;
			minSemi -= add/2;
			maxSemi += add/2;
		} else {
			maxSemi+=2;
			minSemi-=2;
		}
		content.showRange(minSemi, maxSemi);
	}
	void setLayout(layout_pianoroll_t& layout) {
		yscale = layout.yscale;
		yoffset = layout.yoffset;
		yscalefold = layout.yscalefold;
		yoffsetfold = layout.yoffsetfold;
		fold = layout.fold;
	}
	void showEditClip() {
		clip_t* clip = view.clip();
		if (clip != NULL) {
			if (clip->noLayout) {
				grid.showRange(clip->offsetStart, clip->offsetStart+clip->getLen());
				zoomPianoRollToClipsNoteRange();
			} else {
				clip_editor_layout_t& layout = clip->editorLayout;
				grid.setLayout(layout.layoutGrid);
				setLayout(layout.layoutPianoRoll);
			}
		}
	}
	void storeLayout() {
		clip_t* clip = view.clip();
		if (clip != NULL) {
			clip_editor_layout_t& layout = clip->editorLayout;
			layout.layoutPianoRoll = *this;
			layout.layoutGrid = grid;
			clip->noLayout = false;
		}
	}
	bool handleKeyInput(KeyEvent& kevt) {
		return content.handleKeyInput(kevt);
	}
};

class guictr_clipeditor : public guictr_base {
	clip_view& view;
public:
	gui_clipsettings settings;
	gui_arp arp;
	guictr_noteeditor& noteeditor;
	guictr_clipeditor(guictr_noteeditor& _noteeditor, clip_view& _view)
	: guictr_base(),
	  view(_view),
	  settings(_noteeditor.grid, _view),
	  arp(_view),
	  noteeditor(_noteeditor)
	{
		add(&noteeditor);
		add(&arp);
		add(&settings);
	}
	~guictr_clipeditor() {
		remove(&settings);
		remove(&arp);
		remove(&noteeditor);
	}
	void storeLayout() {
		noteeditor.storeLayout();
	}
	void showEditClip() {
		settings.showEditClip();
		noteeditor.showEditClip();
		arp.showEditClip();
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (!view.clip()) return false;
		return guictr_base::mouseHitTest(mpos, evt);
	}
	void render(NVGcontext* vg) {
		renderBackground(vg);
//		guictr_base::setScissorTransform(vg);
		ivec2 posInset = getPosContent();
		nvgTranslate(vg, posInset.x, posInset.y);

		ivec2 center = getSizeContent()/2;
		if (view.clip() != NULL) {
			nvgSave(vg);
			settings.render(vg);
			nvgRestore(vg);
			nvgSave(vg);
			arp.render(vg);
			nvgRestore(vg);
			noteeditor.render(vg);
		} else {
			setFont(vg, 18, G_WHITE, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
			nvgText(vg, center.x, center.y, "No clip selected", NULL);
		}
		for (guibase* gui : guis) {
			if (gui == &noteeditor)
				continue;
			if (gui == &settings)
				continue;
			if (gui == &arp)
				continue;
			gui->render(vg);
		}
//		nvgResetScissor(vg);
		nvgResetTransform(vg);
	}
	virtual void renderBackground(NVGcontext* vg) override {
		bool focused = MainCtrl::get()->isCtrOrChildFocused(this);
		drawBackground(vg, theme, getPosContent(), getSizeContent(), margin, focused, false);
	}
	void layout() {
		ivec2 cs = getSizeContent();
		settings.pos = ivec2(0, 0);
		settings.size = ivec2(250, cs.y);
		arp.pos = ivec2(settings.right()+margin, 0);
		arp.size = ivec2(250, cs.y);
		noteeditor.pos = ivec2(arp.right()+margin, 0);
		noteeditor.size = ivec2(cs.x-arp.right(), cs.y);
//		arp.size = ivec2(210, cs.y);
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	bool handleKeyInput(KeyEvent& kevt) {
		return noteeditor.handleKeyInput(kevt);
	}

};


class guictr_clipeditorview : public guictr_base {
public:
	guictr_noteeditor& noteeditor;
	guictr_clipeditorview(guictr_noteeditor& _noteeditor)
	: guictr_base(),
	  noteeditor(_noteeditor)
	{
	}
	~guictr_clipeditorview() {
	}
	vec2 getScale() {
		ivec2 cs = this->getSizeContent();
		ivec2 csp = noteeditor.getSizeContent();
		return vec2(cs.x / (double)noteeditor.getTotalWidth(), cs.y / (double)csp.y);
	}



	void render(NVGcontext* vg) {
		ivec2 cp = this->getPosContent();
		ivec2 cs = this->getSizeContent();
		if (MainCtrl::get()->isClipEditorVisible()) {
			drawAttachedBackground(vg, theme, cp, cs, margin);
		} else {
			drawBackground(vg, theme, cp, cs, margin, false);
		}
		clip_view& view = MainCtrl::get()->getClipView();
		clip_t* clip = view.clip();
		if (clip && !clip->notes.empty()) {
			clip_notes_t& notes = clip->notes;
			tick_t lenTime = notes.lastNote.end() - notes.firstNote.start();
			int32_t minPitch = notes.minNote.pitch;
			int32_t minTime = notes.firstNote.start();
			int32_t distPitch = notes.maxNote.pitch - notes.minNote.pitch;
			distPitch++;
			lenTime = max(clip->getLen(), lenTime);
			assert(distPitch >= 0);
			assert(lenTime >= 0);
			double noteScale = cs.y / (double)distPitch;
			double tickScale = cs.x / (double)lenTime;
			nvgBeginPath(vg);
			for (note_t& note : notes.m_list) {
				float nX = (float) ((note.start()-minTime)*tickScale);
				float nW = (float) (note.len*tickScale);
				nvgRect(vg, cp.x + nX, cp.y + (note.pitch-minPitch) * noteScale, nW, noteScale);
			}
			nvgFillColor(vg, theme->getColor(COL_NOTE));
			nvgFill(vg);


		}
	}
	void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			MainCtrl::get()->showClipEditor();
//			lastscrolloffset = noteeditor.scrolloffset;
		}
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (evt.guiDragged == this) {
//			ivec2 move = evt.mousepos - evt.dragStart;
//			vec2 scale = getScale();
//			float minScale = min(scale.x, scale.y);
//			noteeditor.setScrolloffset(lastscrolloffset + (int)(move.x*(1.0 / minScale)));
		}
	}
	void layout() {
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
};
