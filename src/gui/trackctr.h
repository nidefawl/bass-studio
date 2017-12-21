#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <vector>
#include "config.h"
#include "exceptions.h"
#include "seq_util.h"
#include "color_util.h"
#include "seq_math.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "guicontainer.h"
#include "trackcontent.h"
#include "tracktimeline.h"
#include "guicontextmenu.h"
#include "mouse.h"

#include "platform.h"
#include "dsp_util.h"
#include "track_audiodata.h"

//inline const float lvlFloor = -48.0f;
//inline const float lvlCeil = 6.0f;
#define MTR_FLOOR -48.0f
#define MTR_CEIL 6.0f

float inline scaledRange(float db, float lvlFloor, float lvlCeil) {
	if (db < dsp_util::DBFS_FLOOR)
		return 1.0f;
	float lvlRange = lvlFloor - lvlCeil;
	return (max(lvlFloor, min(db, lvlCeil)) - lvlCeil) / lvlRange;
}
int32_t getPosYFirstReturnTrack(project_t& project);
track_t *getTrackFromMouse(project_t& project, ivec2 mouse, bool isDragSnap);

class gui_trackgain: public guibase {
	track_t* const m_track;
	NVGcolor color;
	NVGcolor colorStroke;
	NVGcolor colorHover;
	NVGcolor colorPressed;
	bool bEnabled;
public:
	gui_trackgain(track_t* _track) :
		guibase(), m_track(_track) {
		uint32_t rgb = nvgToRGB(g_guiColors[COL_BG_DRK]);
		setColor(rgb);
	}
	void setColor(uint32_t hex) {
		vec4 hsl = hexToHSL(hex);
		color = nvgHSL(hsl.x, hsl.y, hsl.z);
		colorStroke = nvgHSL(hsl.x, CLAMP_F(hsl.y*1.3f), 0.4f);
		colorHover = nvgHSL(hsl.x, CLAMP_F(hsl.y*0.7f), CLAMP_F(hsl.z + 0.3f));
		colorPressed = nvgHSL(hsl.x, CLAMP_F(hsl.y*0.7f), CLAMP_F(hsl.z + 0.3f));
	}
	virtual bool hovered() {
		return this == MainCtrl::get()->guiOver;
	}
	virtual bool pressed() {
		return this == MainCtrl::get()->guiDragged;
	}
	virtual bool focused() {
		return this == MainCtrl::get()->guiFocused;
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void (*drawFn)(NVGcontext*,ivec2&, ivec2&, NVGcolor&) = NULL;
	bool enabled() {
		return bEnabled;
	}
	void render(NVGcontext* vg) {
		NVGcolor c;
		if (!enabled()) {
			c = G_BUTTON_DISABLED;
		}
		else if (pressed()) {
			c = colorPressed;
		}
		else if (hovered()) {
			c = colorHover;
		}
		else {
			c = color;
		}
		ivec2 insetP = pos+ivec2(1);
		ivec2 insetS = size-ivec2(2);
		renderWidgetBorder(vg);
		nvgBeginPath(vg);
		nvgRect(vg, insetP.x, insetP.y, insetS.x, insetS.y);
		nvgFillColor(vg, c);
		nvgFill(vg);
		NVGcolor c2 = colorStroke;
		if (hovered() || focused()) {
			c2 = G_WHITE;
		}
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgStrokeColor(vg, c2);
		nvgStrokeWidth(vg, 1);
		nvgStroke(vg);
		if (drawFn) {
			drawFn(vg, pos, size, c);
		}
		track_plugins_t* audio = m_track->audio;
		if (audio) {
			float f = audio->gain;
			float gaindBFS = dsp_util::dBFS(f);
			double scale = scaledRange(gaindBFS, -60.0f, MTR_CEIL);
			float wVal = (1.0f - scale) * insetS.x;
			float x = insetP.x;
			float y = insetP.y;
			nvgBeginPath(vg);
			nvgRect(vg, x, y, wVal, insetS.y);
			nvgFillColor(vg, rgbToNvg(0x00ddff));
			nvgFill(vg);
			setFont(vg, 20, G_WHITE, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
			String strLvl = StringFormat("%.2f", dsp_util::dBFSClampInf6(f));
			nvgText(vg, insetP.x + insetS.x / 2.0f, insetP.y + G_FONT_MIDDLE_OFFSET(insetS.y), StringAsCStr(strLvl), NULL);
		}
	}
	void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			MainCtrl::get()->captureMouse(this);
		}
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
			int disty = (int)evt.dragDistance->y / 10;
			if (abs(disty) < 1)
				return;
			evt.dragDistance->y = 0;
			track_plugins_t* audio = m_track->audio;
			if (audio) {
				float f = audio->gain;
				my_printf("disty: %d\n", disty);
				float adj = (1.0f - disty / 10.0f);
//				if (f < 1.0E-5f && adj > 1.0f)
//					f = 1.0E-5f;
				my_printf("f: %f  adj %f\n", f, adj);
				if (dsp_util::GAIN_DBFLOOR > f) {
					f = dsp_util::GAIN_DBFLOOR;
				}
				float fNew = dsp_util::clampGain(f * adj);
				my_printf("FNEW: %f %f\n", fNew, dsp_util::dBFS(fNew));
				audio->gain = fNew;
			}
		}
	}
	void handleDraggedRelease(MouseEvent& evt) {
	}
};
class gui_trackmeter: public guibase {
#define TRACK_HEIGHT_STEP 20
#define TRACK_HEIGHT_SPACING 2
public:
	track_t* const m_track;
	gui_trackmeter(track_t* _track) :
		guibase(), m_track(_track) {
	}
	void render(NVGcontext* vg) {
		int32_t spacing = 1;
		ivec2 inset(spacing);
		ivec2 mtrPos = pos + inset;
		ivec2 mtrSize = size - inset * 2;
		track_plugins_t* audio = m_track->audio;
		float channelW = (mtrSize.x-(OUTPUT_CHANNELS+1)*spacing) / (float) OUTPUT_CHANNELS;
		const double scaledZero = scaledRange(0, MTR_FLOOR, MTR_CEIL);
		float hZero = (1.0f - scaledZero) * mtrSize.y;
		float yZero = mtrPos.y + mtrSize.y - hZero;
		if (audio) {
			float x = mtrPos.x+spacing;
			for (int i = 0; i < OUTPUT_CHANNELS; i++) {
				float fMax = audio->meter.getMax(i);
				float fRms = audio->meter.getRms(i);
				float fPeak = audio->meter.getStandingPeak(i);
				float levels[3] = {fMax, fRms, fPeak};

				nvgBeginPath(vg);
				nvgRect(vg, x, mtrPos.y, channelW, mtrSize.y);
				nvgFillColor(vg, GUI_COLOR(G_S1));
				nvgFill(vg);
				NVGcolor colGainLvl[6] = {
					G_GREEN_DRK, G_YELLOW_DRK,
					G_GREEN, G_YELLOW,
					G_GREEN_DRKER, G_YELLOW_DRKER,
				};

				for (int i = 0; i < 3; i++ ){
					float fLvl = levels[i];
					if (fLvl < F_MIN) {
						continue;
					}
					double scale = scaledRange(dsp_util::dBFS(fLvl), MTR_FLOOR, MTR_CEIL);
					float hVal = (1.0f - scale) * mtrSize.y;
					float y = mtrPos.y + mtrSize.y - hVal;
					if (i == 2) {
						nvgBeginPath(vg);
						nvgMoveTo(vg, x, y);
						nvgLineTo(vg, x+channelW, y);
//						int32_t col = fLvl >= 1.0f ? 1 : 0;
						int32_t col = y < yZero ? 1 : 0;
						nvgStrokeColor(vg, colGainLvl[i*2+col]);
						nvgStrokeWidth(vg, 1.5f);
						nvgStroke(vg);
						continue;
					}
					if (hVal > 0.5) {
						float hOvershoot = max(0.0f, hVal-hZero);
						nvgBeginPath(vg);
						nvgRect(vg, x, max(y, yZero), channelW, min(hVal, hZero));
						nvgFillColor(vg, colGainLvl[i*2+0]);
						nvgFill(vg);
						if (hOvershoot > 0) {
							nvgBeginPath(vg);
							nvgRect(vg, x, y, channelW, hOvershoot);
							nvgFillColor(vg, colGainLvl[i*2+1]);
							nvgFill(vg);
						}
					}
				}
				x += channelW;
				x += spacing;
			}
		}
		float x = mtrPos.x+spacing;
		float x2 = mtrPos.x+(spacing+channelW)*2.0f;
		nvgBeginPath(vg);
		nvgMoveTo(vg, x, yZero);
		nvgLineTo(vg, x2, yZero);
		nvgStrokeColor(vg, g_guiColors[COL_GRID_BRT]);
		nvgStrokeWidth(vg, 1.5f);
		nvgStroke(vg);
	}
};
class gui_trackmixer: public guictr_base {
#define TRACK_HEIGHT_STEP 20
#define TRACK_HEIGHT_SPACING 2
public:
	track_t* const m_track;
	gui_trackmeter meter;
	gui_trackgain gain;
	int dragMode = -1;
	const int resizeHitY = 8;
	const int DRAG_RESIZE = 1;
	gui_trackmixer(track_t* _track) :
			guictr_base(), m_track(_track), meter(_track), gain(_track) {
		padding = 0;
		add(&gain);
		add(&meter);
	}
	~gui_trackmixer() {
		remove(&meter);
		remove(&gain);
	}
	bool isStaticContainer() {
		return false;
	}
	void handleDraggedBegin(MouseEvent& evt) {
		MainCtrl::get()->setSelectedTrack(m_track);
		if (isResize(evt.relMousepos+this->pos)) {
			dragMode = DRAG_RESIZE;
		}
	}

	void handleDraggedMove(MouseEvent& evt) {
		if (dragMode == DRAG_RESIZE) {
			int32_t mouseDragDist = evt.relMousepos.y;
			bool resizeTop = m_track->type < TRACK_TYPE_MIDI;
			if (resizeTop) {
				mouseDragDist = -evt.relMousepos.y+size.y;
			}
			m_track->height = min(12, max(1, (mouseDragDist) / TRACK_HEIGHT_STEP));
			my_printf("%d %d %d %d \n", resizeTop, mouseDragDist, m_track->height, (mouseDragDist) / TRACK_HEIGHT_STEP);
			this->parent->onChildLayoutChanged(this);
		}
	}

	void handleDraggedRelease(MouseEvent& evt) {
		dragMode = -1;
	}
	void handleRightClick(MouseEvent& evt) {
		MainCtrl::get()->openContextMenu(new guictxtmenu_track(this->m_track->idx), evt.mousepos);
	}
	bool isResize(ivec2 mpos) {
		int32_t resizeTopOrBottom = m_track->type < TRACK_TYPE_MIDI ? top() : bottom();
		return mpos.y >= resizeTopOrBottom - resizeHitY
				&& mpos.y < resizeTopOrBottom + resizeHitY;
	}

	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		ivec2 local = this->toContainerSpace(mpos);
		for (guibase* gui : guis) {
			if (gui->mouseHitTest(local, evt)) {
				return true;
			}
		}
		if (contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		if (isResize(mpos)) {
			evt.requestFocus(this);
			if (evt.type <= MouseHitType::MOUSE_RIGHT)
				evt.requestCursor(CURSOR_RESIZE_V);
			return true;
		}
		return false;
	}
	void layout() {
		float mW = 20;
		meter.size = ivec2(mW, size.y);
		meter.pos = ivec2(size.x - mW, 0);
		gain.size = ivec2(120, mW);
		gain.pos = ivec2(INSET_TITLE, HEIGHT_TRACK_TITLE + INSET_TITLE);

	}

	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		NVGcolor color = rgbToNvg(m_track->rgb);
		ivec2 titleSize(size.x-meter.size.x, size.y);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, size.x, size.y);
		nvgFillColor(vg, g_guiColors[COL_GRID_BRT]);
		nvgFill(vg);
		if (MainCtrl::get()->getSelectedTrack() == m_track) {
			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
			nvgFill(vg);
			color = G_BLACK;
		}
		int titleHeight = min(HEIGHT_TRACK_TITLE, size.y);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, titleSize.x, titleHeight);
		nvgFillColor(vg, color);
		nvgFill(vg);
		setFont(vg, (int) (titleHeight * 0.8), getContrastFontColorNvg(color), G_TITLE_ALIGN);
		renderText(vg, 0 + INSET_TITLE, 0 + titleHeight / 2, titleSize.x, StringAsCStr(m_track->name));


		//debug stuff
//		size_t nClips = m_track->clips.size();
//		auto it = m_track->clips.begin();
//		size_t nGuiClips = 0;
//		size_t nGuiClips2 = m_track->content->guis.size();
//		while (it != m_track->clips.end()) {
//			clip_t* c = *it;
//			if (c->gClip != NULL)
//				nGuiClips++;
//			it++;
//		}
//		String s = StringFormat("%u (%u/%u) Clips", nClips, nGuiClips, nGuiClips2);
//		setFont(vg, (int) (titleHeight * 0.6), getContrastFontColor(m_track->rgb), G_TITLE_ALIGN);
//		nvgText(vg, 0 + INSET_TITLE, pos.y + titleHeight+titleHeight, StringAsCStr(s), NULL);

		meter.render(vg);
		gain.render(vg);
	}
};

class guitrack_editor : public guictr_base {
public:
	Cursor& cursor;
	project_t& project;
	scaled_grid& grid;
	dragdrop_midifile& dragdrop;
	track_t *trSelected = NULL;
	clip_dragaction action;
	clip_clipboard* clipboard = NULL;
	tracklayout_t dragStartLayout;
	int32_t dragStartTick = 0;
	int32_t dragStartTrackIdx = 0;

	trackstate_t resizePreModifyState;
	bool selectionMoved = false;

	guitrack_editor(Cursor& _cursor, project_t& _project, scaled_grid& _grid, dragdrop_midifile& _dragdropclip)
		: guictr_base(), 
		cursor(_cursor),
		project(_project),
		grid(_grid),
		dragdrop(_dragdropclip)
	{
		padding = 0;
		sortChildren = true;
	}
	bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override {
		if (this->contains(v)) {
			if (evt.type == MOUSE_DRAGDROP_CLIP) {
				evt.requestFocus(this);
				return true;
			}
			ivec2 localMouse = this->toContainerSpace(v);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	bool handleKeyInput(KeyEvent& kevt);

	void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt);
	void trackViewDragMove(guitrack_editor* view, MouseEvent& evt);
	void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt);
	void dragSelectionBegin(gui_clip* gClip, MouseEvent& evt);
	void dragSelectionMove(gui_clip* gui, MouseEvent& evt);
	void dragSelectionRelease(gui_clip* gui, MouseEvent& evt);
	void dragClipboardMove(ivec2 local);

	bool clipDropBegin(dragdrop_midifile& clip, ivec2 mousepos);
	bool clipDropMove(dragdrop_midifile& clip, ivec2 mousepos);
	bool clipDropFinal(dragdrop_midifile& clip, ivec2 mousepos);


	void handleRightClick(MouseEvent& evt);

	void renderClip(NVGcontext* vg, track_t* tr, const clip_t* cl, tick_t offset);
	void renderAction(NVGcontext* vg, clip_dragaction& action);
	void render(NVGcontext* vg);


	void handleDraggedBegin(MouseEvent& evt) {
		evt.guiDragged->trackViewDragBegin(this, evt);
	}
	void handleDraggedMove(MouseEvent& evt) {
		evt.guiDragged->trackViewDragMove(this, evt);
	}
	void handleDraggedRelease(MouseEvent& evt) {
		evt.guiDragged->trackViewDragRelease(this, evt);
	}

	void resizeOtherClips(track_t* tr, clip_t* clip);

	void setSelectionRange(clip_t* clicked, track_t *trackClicked) {
		cursor.selRange = clicked->len;
		cursor.selTrackRange = 0;
		cursor.cursorPos = clicked->time;
		cursor.cursorTrack = trackClicked->idx;
	}

	void addTrack(track_t* t) {
		if (t->content)
			throw applogicexception("expected t->content == NULL");
		t->content = new gui_trackcontent(t);
		t->content->setZOrder(t->type >= TRACK_TYPE_MIDI ? 0 : 1);
		add(t->content);
	}
	void removeTrack(track_t* t) {
		if (t->content) {
			t->content->destroyGuis();
			remove(t->content);
			DELETE_PTR(t->content)
		}
	}
	void updateVisibleTrackContents() {
		for (track_t* g : project.trackList) {
			g->content->updateVisibleTrackContents(grid);
		}
	}
	void layout() {
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
};

class guitrack_mixers : public guictr_base {
	project_t& project;
public:
	guitrack_mixers(project_t& _project)
		: guictr_base(),
		  project(_project)
	{
		padding = 0;
		sortChildren = true;
	}
	bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override {
		if (this->contains(v)) {
			ivec2 localMouse = this->toContainerSpace(v);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void handleDraggedBegin(MouseEvent& evt) {
	}
	void handleDraggedMove(MouseEvent& evt) {
	}
	void handleDraggedRelease(MouseEvent& evt) {
	}
	void handleRightClick(MouseEvent& evt) {
		MainCtrl::get()->openContextMenu(new guictxtmenu_notrack(), evt.mousepos);
	}
	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		ivec2 cs = getSizeContent();
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, cs.x, cs.y);
		nvgFillColor(vg, g_guiColors[COL_GRID_BRT]);
		nvgFill(vg);

		for (track_t* g : project.tracksBottom) {
			//content
			nvgSave(vg);
			g->mixer->render(vg);
			nvgRestore(vg);
		}
		int ySplit = getPosYFirstReturnTrack(project);
		if (ySplit > 0) {
			nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
			for (track_t* g : project.trackCtr) {
				//content
				nvgSave(vg);
				g->mixer->render(vg);
				nvgRestore(vg);
			}
		}

	}
	void addTrack(track_t* t) {
		if (t->mixer)
			throw applogicexception("expected t->mixer == NULL");
		t->mixer = new gui_trackmixer(t);
		t->mixer->setZOrder(t->type >= TRACK_TYPE_MIDI ? 0 : 1);
		this->add(t->mixer);
	}
	void removeTrack(track_t* t) {
		if (t->mixer) {
			this->remove(t->mixer);
			DELETE_PTR(t->mixer)
		}
	}
	void layout() {
		for (guibase* gui : guis) {
			gui->layout();
		}
	}


};

class te_constants {
protected:
	const uint32_t heightSeperator = 10;
	const uint32_t heightLoopIndicators = 24;
	const uint32_t heightTimelineControls = heightLoopIndicators + heightSeperator;
};
class guictr_tracks_loophandles : public guibase, te_constants {
	project_t& project;
	scaled_grid& grid;
	enum dragmode {
		drag_handle_none,
		drag_handle_loopleft,
		drag_handle_loopright,
		drag_handle_loopbar
	};
	dragmode dragHandle = drag_handle_none;
public:
	ivec2 clipViewSize;
	guictr_tracks_loophandles(project_t& _project, scaled_grid& _grid) :
			guibase(), project(_project), grid(_grid) {

	}
	int32_t dragOffset = 0;
	void handleDraggedBegin(MouseEvent& evt) {
		dragHandle = drag_handle_none;
		ivec2 local = evt.relMousepos;
		dragHandle = getDragZone(local);
		dragOffset = local.x-(int32_t)grid.tickToScreenD(project.loopStart);
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (dragHandle == drag_handle_none) {
			return;
		}
		int32_t mousePosX = evt.relMousepos.x;
		if (dragHandle == drag_handle_loopbar) {
			mousePosX -= dragOffset;
		}
		tick_t tickAt = grid.screenToTickSnap(mousePosX, SNAP_ON);
		tick_t curLoopEnd = project.loopStart + project.loopLen;

		if (dragHandle == drag_handle_loopright) {
			tick_t tickDelta = (tickAt - curLoopEnd);
			tick_t newLen = project.loopLen + tickDelta;
			if (newLen > 0) {
				project.loopLen = newLen;
			}
		}
		if (dragHandle == drag_handle_loopleft) {
			tick_t curLoopStart = project.loopStart;
			tick_t tickDelta = (tickAt - curLoopStart);
			tick_t newStart = project.loopStart + tickDelta;
			if (newStart < curLoopEnd) {
				project.loopStart = newStart;
				project.loopLen = curLoopEnd - newStart;
			}
		}
		if (dragHandle == drag_handle_loopbar) {
			tick_t curLoopStart = project.loopStart;
			tick_t tickDelta = (tickAt - curLoopStart);
			project.loopStart += tickDelta;
		}
//		MainCtrl::get()->updateVisibleTrackContents();
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
		struct dist_draghandle {
			float dist = 0;
			dragmode mode = drag_handle_none;
		};;
		float dragTop = heightLoopIndicators/2.0f;
		float distBar = std::numeric_limits<float>::max();
		float barSX = clipLoopStartScrX();
		float barEX = clipLoopEndScrX();
		if (local.x >= barSX && local.x < barEX
				&& local.y >= 0 && local.y < heightLoopIndicators) {
			distBar = DRAG_RANGE*DRAG_RANGE*0.8f;
		}
		std::vector<dist_draghandle> hndls {
			{dist(barSX, dragTop, local), dragmode::drag_handle_loopleft},
			{dist(barEX, dragTop, local), dragmode::drag_handle_loopright},
			{distBar, dragmode::drag_handle_loopbar}
		};
		std::sort(hndls.begin(), hndls.end(), [](dist_draghandle const & a, dist_draghandle const & b) {
			return a.dist  < b.dist;
		});
		if (hndls[0].dist < DRAG_RANGE*DRAG_RANGE) {
			return hndls[0].mode;
		}

		return drag_handle_none;
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 local = this->toContainerSpace(mpos);
			if (evt.type <= MouseHitType::MOUSE_RIGHT) {
				dragmode mode = getDragZone(local);
				if (mode == dragmode::drag_handle_loopleft) {
					evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
					evt.requestFocus(this);
					return true;
				}
				if (mode == dragmode::drag_handle_loopright) {
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
	float clipLoopStartScrX() {
		return (float)grid.tickToScreenD(project.loopStart);
	}
	float clipLoopEndScrX() {
		return (float)grid.tickToScreenD(project.loopStart + project.loopLen);
	}
	void render(NVGcontext* vg) {
		ivec2 cs = clipViewSize;
		if (cs.x <= 0 || cs.y <= 0)
			return;
		nvgIntersectScissor(vg, pos.x, pos.y, cs.x, cs.y);
		nvgTranslate(vg, pos.x, pos.y);
		nvgBeginPath(vg);
		nvgRect(vg, -2, 0, cs.x+2, size.y);
		nvgFillColor(vg, g_guiColors[COL_GRID_DRK]);
		nvgFill(vg);

		for (grid_div g : grid.gridList) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, g.screenpos, 0);
			nvgLineTo(vg, g.screenpos, heightLoopIndicators);
			nvgStrokeColor(vg, g_guiColors[COL_LINE_BAR + g.color]);
			nvgStrokeWidth(vg, g.thickness);
			nvgStroke(vg);
		}
		nvgBeginPath(vg);
		nvgRect(vg, -2, heightLoopIndicators, cs.x+2, heightSeperator);
		nvgFillColor(vg, g_guiColors[COL_BG_DRKER2]);
		nvgFill(vg);


		const NVGcolor colLI = GUI_COLOR(120);
		const NVGcolor colLIStroke = GUI_COLOR(G_S1);
		const float strokeWidthLI = 1.0f;
		const float wLoopInidicator = heightLoopIndicators;


		int yOffset = 0;
		float tickBeginX = clipLoopStartScrX();
		float tickEndX = clipLoopEndScrX();
		if (!(tickBeginX - wLoopInidicator > cs.x || tickEndX + wLoopInidicator < 0)) {
			float barBeginX = max(-wLoopInidicator, tickBeginX);
			float barEndX = min(cs.x + wLoopInidicator, tickEndX);
			nvgBeginPath(vg);
			nvgRect(vg, barBeginX, yOffset, barEndX-barBeginX, heightLoopIndicators);

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
				nvgLineTo(vg, tickEndX, cs.y-yOffset+1);
				nvgStrokeColor(vg, colLI);
				nvgStrokeWidth(vg, strokeWidthLI);
				nvgStroke(vg);
				drawTri(vg, tickEndX, yOffset, wLoopInidicator, 1, colLI, colLIStroke, strokeWidthLI);


			}
		}
		float xJmpFrom = grid.tickToScreenD(MainCtrl::get()->tickJmpFrom);
		float xJmpTo = grid.tickToScreenD(MainCtrl::get()->tickJmpTo);
		nvgBeginPath(vg);
		nvgMoveTo(vg, xJmpFrom, yOffset);
		nvgLineTo(vg, xJmpFrom, cs.y-yOffset+1);
		nvgStrokeColor(vg, G_YELLOW_DRK);
		nvgStrokeWidth(vg, strokeWidthLI);
		nvgStroke(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, xJmpTo, yOffset);
		nvgLineTo(vg, xJmpTo, cs.y-yOffset+1);
		nvgStrokeColor(vg, G_GREEN_DRK);
		nvgStrokeWidth(vg, strokeWidthLI);
		nvgStroke(vg);

		yOffset += heightLoopIndicators;


	}
};
class guictr_tracks : public guictr_base, grid_changed_cb, te_constants {
public:
	scaled_grid& grid;
	project_t& project;
	guitrack_mixers trackControls;
	guitrack_editor trackView;
	guitrack_timeline trackTimeline;
	guictr_tracks_loophandles loophandles;
	guictr_tracks(Cursor& _cursor, project_t& _project, scaled_grid& _grid, dragdrop_midifile& _dragdropclip)
		: guictr_base(),
		grid(_grid),
		project(_project),
		trackControls(_project),
		trackView(_cursor, _project, _grid, _dragdropclip),
		trackTimeline(_grid),
		loophandles(_project, _grid)
	{
		_grid.addCallback(this);
		add(&trackTimeline);
		add(&loophandles);
		add(&trackControls);
		add(&trackView);
	}
	~guictr_tracks() {
		remove(&trackView);
		remove(&trackControls);
		remove(&loophandles);
		remove(&trackTimeline);
	}
	void addSingleTrack(track_t* t) {
		trackControls.addTrack(t);
		trackView.addTrack(t);
		layout();
	}
	void removeSingleTrack(track_t* t) {
		trackControls.removeTrack(t);
		trackView.removeTrack(t);
		layout();
	}
	void addTrack(track_t* t) {
		trackControls.addTrack(t);
		trackView.addTrack(t);
	}
	void removeTrack(track_t* t) {
		trackControls.removeTrack(t);
		trackView.removeTrack(t);
	}
	void drawSeperator(NVGcontext* vg, track_t* g, ivec2& cs) {
		//draw (seperator) line at top or bottom of track
		int seperatorY = g->mixer->pos.y;
		if (g->type >= TRACK_TYPE_MIDI) {
			seperatorY += g->mixer->size.y;
		}
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, seperatorY);
		nvgLineTo(vg, cs.x, seperatorY);
		nvgStrokeColor(vg, g_guiColors[COL_LINE_SEPERATOR]);
		nvgStrokeWidth(vg, TRACK_HEIGHT_SPACING);
		nvgStroke(vg);
	}
	void render(NVGcontext* vg) {
		guictr_base::renderBackground(vg);
		ivec2 cs = getSizeContent();
		ivec2 cp = getPosContent();
		if (cs.y <= 0 || cs.x <= 0) {
			return;
		}
		nvgIntersectScissor(vg, cp.x, cp.y, cs.x, cs.y);
		nvgTranslate(vg, cp.x, cp.y);
		nvgSave(vg);
			trackView.render(vg);
		nvgRestore(vg);
		nvgSave(vg);
			trackControls.render(vg);
		nvgRestore(vg);
		nvgSave(vg);
			trackTimeline.render(vg);
		nvgRestore(vg);

		nvgSave(vg);
			nvgTranslate(vg, 0, trackView.top());
			int ySplit = getPosYFirstReturnTrack(project);
			if (ySplit > 0) {
				nvgSave(vg);
				nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
				for (track_t* g : project.trackCtr) {
					drawSeperator(vg, g, cs);
				}
				nvgRestore(vg);
				nvgIntersectScissor(vg, 0, ySplit, cs.x, trackView.size.y-ySplit);
			} else {
				nvgIntersectScissor(vg, 0, 0, cs.x, trackView.size.y);
			}
			for (track_t* g : project.tracksBottom) {
				drawSeperator(vg, g, cs);
			}
		nvgRestore(vg);

		nvgBeginPath(vg);
		nvgMoveTo(vg, trackControls.left(), trackControls.top());
		nvgLineTo(vg, trackControls.left(), trackControls.bottom());
		nvgStrokeColor(vg, g_guiColors[COL_LINE_SEPERATOR]);
		nvgStrokeWidth(vg, 3);
		nvgStroke(vg);




		nvgSave(vg);
		loophandles.render(vg);
		nvgRestore(vg);

		nvgIntersectScissor(vg, 0, 0, trackView.size.x, cs.y);
		tick_t pos = project.playbackPos;
//		if (project.loopEnabled) {
//			if (pos > project.loopStart) {
//				pos = project.loopStart + (pos - project.loopStart) % project.loopLen;
//			}
//		}
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
//		nvgIntersectScissor(vg, 0, 0, trackView.size.x, trackView.size.y);
//		nvgTranslate(vg, 0, trackTimeline.bottom());

//		double playBackX = grid.tickToScreenD(MainCtrl::get()->playbackPos);
//		if (playBackX > -4 && playBackX < cs.x+4) {
//			nvgBeginPath(vg);
//			nvgMoveTo(vg, playBackX, 0);
//			nvgLineTo(vg, playBackX, cs.y);
//			nvgStrokeColor(vg, GUI_COLOR(120));
//			nvgStrokeWidth(vg, 3);
//			nvgStroke(vg);
//			nvgBeginPath(vg);
//			nvgMoveTo(vg, playBackX, 0);
//			nvgLineTo(vg, playBackX, cs.y);
//			nvgStrokeColor(vg, GUI_COLOR(250));
//			nvgStrokeWidth(vg, 1);
//			nvgStroke(vg);
//		}
	}
	void setTrackPosition(track_t* t, int32_t trackheight, int32_t y) {
		t->content->pos.x = 0;
		t->content->pos.y = y;
		t->mixer->pos.x = 0;
		t->mixer->pos.y = y;
		t->content->size = ivec2(trackView.size.x, trackheight);
		t->mixer->size = ivec2(trackControls.size.x, trackheight);
	}
	void layout() {
		const int mixerwidth = 200;
		ivec2 cs = getSizeContent();
		trackTimeline.pos = ivec2(0, 0);
		trackTimeline.size = ivec2(cs.x - mixerwidth, 32);
		loophandles.pos = ivec2(trackTimeline.left(), trackTimeline.bottom());
		loophandles.size = ivec2(trackTimeline.size.x, heightTimelineControls);

		trackView.pos = ivec2(0, loophandles.bottom());
		trackControls.pos = ivec2(cs.x - mixerwidth, loophandles.bottom());
		trackView.size = ivec2(cs.x - mixerwidth - trackView.pos.x, cs.y - loophandles.bottom());
		trackControls.size = ivec2(mixerwidth, trackView.size.y);

		loophandles.clipViewSize = ivec2(trackView.size.x, trackView.size.y+loophandles.size.y);


		ivec2 csTrackView = trackView.getSizeContent();
		int y = 0;
		for (track_t* t : project.trackCtr) {
			int trackheight = t->height * TRACK_HEIGHT_STEP;
			assert(t->content != NULL);
			setTrackPosition(t, trackheight, y);
			y += trackheight + TRACK_HEIGHT_SPACING;
		}
		y = csTrackView.y;
//		y = 0;
		auto itMastersTracks = project.tracksBottom.rbegin();
		auto itMastersEnd = project.tracksBottom.rend();
		while (itMastersTracks != itMastersEnd) {
			track_t* t = *itMastersTracks;
			int trackheight = t->height * TRACK_HEIGHT_STEP;
			y -= trackheight;
//			y -= 100;
			assert(t->content != NULL);
			setTrackPosition(t, trackheight, y);
//			y += trackheight + TRACK_HEIGHT_SPACING;
			y -= TRACK_HEIGHT_SPACING;
			itMastersTracks++;
		}

		for (guibase* gui : guis) {
			gui->layout();
		}
		MainCtrl::get()->updateGrid();
	}
	void updateVisibleTrackContents() {
		trackView.updateVisibleTrackContents();
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

	void onChildLayoutChanged(guibase* g) {
		layout();
	}
	void gridChanged(scaled_grid& _grid) override {
		MainCtrl::get()->updateGrid();
	}
	bool handleKeyInput(KeyEvent& kevt) {
		return trackView.handleKeyInput(kevt);
	}
};

