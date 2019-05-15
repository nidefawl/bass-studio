#pragma once
#include <list>
#include <vector>
#include "math/vec.h"
#include "math/seq_math.h"
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
#include "note.h"
#include "grid.h"
#include "keyboard.h"
#include "edithistory.h"
#include "../host/mainctrl.h"
#include "guiarp.h"
#include "guiinputfield.h"

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
		int32_t nNotes = math::abs(noteFrom - noteTo);
		float rangeScale = sizeY / (float) nNotes;
		setScale(rangeScale); //TODO: maybe only zoom out here, not in (or determine on upper level)
		setOffset(math::min(noteFrom, noteTo)*layoutRoll.scale());
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
	gui_pianoroll(clip_view& _view, layout_pianoroll_t& _layout);
	~gui_pianoroll() {
	}
	void render(NVGcontext* vg);

	void handleDraggedBegin(MouseEvent& evt);
	void handleDraggedMove(MouseEvent& evt);
	virtual void handleDraggedRelease(MouseEvent& evt);
	virtual void handleRightClick(MouseEvent& evt);
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	void layout();
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
	gui_clipsettings(scaled_grid& _grid, clip_view& _view);
	~gui_clipsettings();
	void render(NVGcontext* vg);

	void layout();
	void renderBackground(NVGcontext* vg) override;
	void buttonClicked(guibase* button) override;
	void showEditClip();
};
;
class gui_clipcontent : public guictr_base, public piano_scale {
public:
	enum dragmode {
		drag_none,
		drag_frame,
		drag_notes_move,
		drag_notes_copy,
		drag_note_left,
		drag_note_right,
		drag_velocity,
	};
	clip_cursor_t dragStartCursor;
	dragmode dragMode = drag_none;
	std::set<note_t*> selectionStart;
	ivec2 dragBegin=ivec2(0);
	ivec2 dragTo=ivec2(0);
	note_t beginDragNote;
	scaled_grid& grid;
	clip_view& view;
	const bool isVelocity;
	gui_clipcontent(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout, bool _isVel)
		: guictr_base(), piano_scale(_layout, _view, size.y),
		grid(_grid),
		view(_view),
		isVelocity(_isVel)
	{
		padding = 0;
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	void setStatusText();
	void expandSelectionFrame(std::pair<note_t*, note_t*> minMax);
	void setSelectionFrame(std::pair<note_t*, note_t*> minMax);
	void mergeDraggedNotes(dragmode mergeMode);
	void handleRightClick(MouseEvent& evt);
	void handleDraggedBegin(MouseEvent& evt);
	void handleDraggedMove(MouseEvent& evt);
	void handleDraggedRelease(MouseEvent& evt);
	bool handleKeyInput(KeyEvent& kevt);

	void layout() {
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
protected:
	void setGlobalSelectionFromClipSelection();
};
class gui_clipcontent_notes : public gui_clipcontent {
public:
	gui_clipcontent_notes(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout) : gui_clipcontent(_grid, _view, _layout, false)
	{

	}
	void render(NVGcontext* vg);

};
class gui_clipcontent_velocities : public gui_clipcontent {
public:
	gui_clipcontent_velocities(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout) : gui_clipcontent(_grid, _view, _layout, true)
	{

	}
	void render(NVGcontext* vg);
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
	ivec2 clipViewSize{ 0, 0 };
	int32_t dragOffset = 0;
public:
	guictr_cliphandles(scaled_grid& _grid, clip_view& _view) :
			guibase(), grid(_grid), view(_view) {

	}
	void handleDraggedBegin(MouseEvent& evt);
	void handleDraggedMove(MouseEvent& evt);
	void handleDraggedRelease(MouseEvent& evt);
	float dist(float x, float y, ivec2 mpos) {
		x = x - mpos.x;
		y = y - mpos.y;
		return x*x+y*y;
	}
	dragmode getDragZone(ivec2 local);
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
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
	void render(NVGcontext* vg);
};
class gui_velocities;
class guictr_noteeditor : public guictr_base, public layout_pianoroll_t, grid_changed_cb, ce_constants {
public:
	scaled_grid grid;
	gui_pianoroll piano;
	gui_clipcontent_notes content;
	gui_clipcontent_velocities velocities;
	guitrack_timeline timeline;
	guictr_cliphandles clipHandles;
	clip_view& view;
	guibutton btnToggleFold;
	int32_t velHeight = 120;
public:
	guictr_noteeditor(clip_view& _view);
	~guictr_noteeditor();
	void setLayout(layout_pianoroll_t& layout);
	virtual void buttonClicked(guibase* button);
	int32_t getTotalWidth();
	void renderBackground(NVGcontext* vg) override;
	void render(NVGcontext* vg);
	void layout();
	void gridChanged(scaled_grid& _grid) override;
	void handleDraggedBegin(MouseEvent& evt);
	void zoomPianoRollToClipsNoteRange();
	void showEditClip();
	void storeLayout();
	bool handleKeyInput(KeyEvent& kevt);
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
		setBackgroundRendered(true);
		setBackgroundRenderedInset(false);
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
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
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
			lenTime = math::max(clip->getLen(), lenTime);
			dbgassert(distPitch >= 0);
			dbgassert(lenTime >= 0);
			double noteScale = cs.y / (double)distPitch;
			double tickScale = cs.x / (double)lenTime;
			nvgBeginPath(vg);
			for (note_t& note : notes.m_list) {
				float nX = (float) ((note.start()-minTime)*tickScale);
				float nW = (float) (note.len*tickScale);
				nvgRect(vg, cp.x + nX, cp.y + (note.pitch-minPitch) * noteScale, nW, noteScale);
			}
			nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE));
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
