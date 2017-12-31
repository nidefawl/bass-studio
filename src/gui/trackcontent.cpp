
#include "track.h"
#include "trackcontent.h"
#include "trackctr.h"
#include "guicontextmenu.h"
#include "button.h"
#include "event.h"
#include "../host/vst_plugin.h"
#include "trackautomation.h"
#include "dropdown.h"
#include "leak_detect.h"
#include <glm/geometric.hpp>

float noteToScreen(float note, float scale, float offset, float sizeY) {
	float offsetKey = note * scale;
	float rel = offsetKey - offset;
	return (sizeY) - rel;
}
void gui_clip::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
/*static*/ void gui_clip::renderClip(NVGcontext* vg, const track_t* tr, const clip_t* cl, ivec2 pos, ivec2 size) {
	if (cl->len <= 0) {
		return;
	}
	NVGcolor color = rgbToNvg(cl->rgb);
	nvgBeginPath(vg);
	nvgRect(vg, pos.x, pos.y, size.x, HEIGHT_CLIP_TITLE);
	nvgFillColor(vg, color);
	nvgFill(vg);
	nvgStrokeColor(vg, G_BLACK);
	nvgStrokeWidth(vg, 1.f);
	nvgStroke(vg);
	if (cl->name.length()) {
		setFont(vg, (int) (HEIGHT_CLIP_TITLE * 0.95), getContrastFontColor(cl->rgb), G_TITLE_ALIGN);
		renderText(vg, pos.x + INSET_TITLE, pos.y + HEIGHT_CLIP_TITLE / 2, size.x-INSET_TITLE*3, StringAsCStr(cl->name));
	}
	ivec2 posContents = ivec2(pos.x, pos.y+HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
	ivec2 sizeContents = ivec2(size.x, size.y-HEIGHT_CLIP_TITLE-INSET_CLIP_CONTENT*2);

	tick_t clipLen = cl->len;
	float numBars = clipLen / (float) TICKS_BAR;
	float barSize = sizeContents.x / (float) numBars;
	if (sizeContents.x > 0 && sizeContents.y > 0) {
		nvgSave(vg);
		nvgTranslate(vg, posContents.x, posContents.y);
		nvgBeginPath(vg);

		clip_notes_t& notesView = cl->getNoteViewRender();
		clip_notes_t& notesPlay = cl->getNoteViewPlayback();
	//	clip_notes_t notesPlay;
	//	cl->getNotesView(0, cl->len, notesPlay, true);
		for (int i = 0; i < (tr?(tr->idx%2)+1:1); i++) {
			int32_t rgbNote = i == 0 ? 0xff9933 : 0x33ff33;
			int32_t rgbNoteOverlap = i == 0 ? 0x0000ff : 0xff00ff;
			clip_notes_t& notes = i == 0 ? notesView : notesPlay;
			if (!notes.empty()) {
				note_t minN = notesView.minNote;
				note_t maxN = notesView.maxNote;
				int32_t numNotes = max((int32_t)8, maxN.pitch - minN.pitch);
				float scale = sizeContents.y / (float) numNotes;
				std::vector<const note_t*> notesClipped;
				for (const note_t& note : notes.m_list) {
					tick_t noteTime = note.time;
					if (noteTime >= clipLen) {
						notesClipped.push_back(&note);
						continue;
					}
					if (noteTime < 0) {
						notesClipped.push_back(&note);
						continue;
					}
					float objPosNote = noteTime /(float) TICKS_BAR;
		//			assert(objPosNote >= 0 && objPosNote < numBars);
					float objLenNote = note.len /(float) TICKS_BAR;
		//			assert(objPosNote+objLenNote >= 0);
					float ny = noteToScreen(note.pitch-minN.pitch, scale, 0, sizeContents.y);
					float nx = max(0.0f, objPosNote * barSize);
					float nw = min(objLenNote * barSize, sizeContents.x-nx);
					float nh = scale;
					float insetx = calcInset(1, nw);
					float insety = calcInset(1, nh);
					nvgRect(vg, nx+insetx, ny+insety, nw-insetx*2, nh-insety*2);
				}
				nvgFillColor(vg, rgbToNvg(rgbNote));
				nvgFill(vg);
				if (!notesClipped.empty()) {
					nvgBeginPath(vg);
					for (const note_t* noteClipped : notesClipped) {
						const note_t& note = *noteClipped;
						tick_t noteTime = note.time;
			//			assert(objPosNote >= 0 && objPosNote < numBars);
			//			assert(objPosNote+objLenNote >= 0);

						float objPosNote = noteTime /(float) TICKS_BAR;
						float objLenNote = note.len /(float) TICKS_BAR;
						float ny = noteToScreen(note.pitch-minN.pitch, scale, 0, sizeContents.y);
						float nx = objPosNote * barSize;
						float nw = objLenNote * barSize;
						float nh = scale;
						float insetx = calcInset(1, nw);
						float insety = calcInset(1, nh);
						nvgRect(vg, nx+insetx, ny+insety, nw-insetx*2, nh-insety*2);
					}
					nvgFillColor(vg, rgbToNvg(rgbNoteOverlap));
					nvgFill(vg);
				}
			}
		}
		nvgRestore(vg);
	}
	if (cl->loopEnabled) {
		tick_t posLoopIndicator = cl->getLoopBegin();
		nvgBeginPath(vg);
		while (posLoopIndicator < clipLen) {
			if (posLoopIndicator >= 0) {
				float objPos = posLoopIndicator /(float) TICKS_BAR;
				float nx = barSize*objPos;
				nvgMoveTo(vg, pos.x+nx, pos.y);
				nvgLineTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE/4);
				nvgMoveTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE*3/4);
				nvgLineTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE);
			}
			posLoopIndicator += cl->loopLen;
		}
		nvgStrokeColor(vg, GUI_COLOR(G_S2));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
	}
}

void gui_track::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}

void gui_clip::trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
	view->dragSelectionBegin(this, evt);
}
void gui_clip::trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {
	view->dragSelectionMove(this, evt);
}
void gui_clip::trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
	view->dragSelectionRelease(this, evt);
	//!CLIP COULD BE DELETED AT THIS POINT
}

class gui_track_audiochain : public gui_track {
public:
	gui_track_audiochain(track_t* _track) : gui_track(_track) {

	}
};


class gui_track_midi : public gui_track {
public:
	trackdata_midi_t& midi;
	gui_track_midi(track_t* _track)
		: gui_track(_track),
		midi(m_track->getMidi()) {
	}
	void render(NVGcontext* vg) {
		if (MainCtrl::get()->getSelectedTrack() == m_track) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
			nvgFill(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
//		nvgTranslate(vg, pos.x, pos.y);
		for (clip_t* clip : midi.clips) {
			if(!clip->gClip) {
				continue;
			}
			clip->gClip->render(vg);
		}
	}

	void updateVisibleTrackContents(scaled_grid& grid) {
		for (clip_t* clip : midi.clips) {
//			gui_clip* gClip = clip->gClip;
			if(!clip->gClip) {
				clip->gClip = new gui_clip(clip, m_track);
				add(clip->gClip);
			}
			clip->gClip->updatePosition(grid, size);
		}
	}
};

class gui_trackmeter: public guibase {
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

class gui_trackgain: public guibase {
	track_t* const m_track;
	bool bEnabled = false;
public:
	gui_trackgain(track_t* _track) :
		guibase(), m_track(_track) {
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
	void (*drawFn)(NVGcontext*,ivec2&, ivec2&, const NVGcolor&) = NULL;
	bool enabled() {
		return bEnabled;
	}
	void render(NVGcontext* vg) {
		renderWidgetBorder(vg, getStateFlags());
		if (drawFn) {
			drawFn(vg, pos, size, theme->getBgColor(getStateFlags()));
		}
		track_plugins_t* audio = m_track->audio;
		if (audio) {
			ivec2 insetP = pos+ivec2(1);
			ivec2 insetS = size-ivec2(2);
			float f = audio->mixer.gain;
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
				float f = audio->mixer.gain;
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
				audio->mixer.gain = fNew;
			}
		}
	}
	void handleDraggedRelease(MouseEvent& evt) {
	}
};


class guibutton_trackbypass : public guibutton {
	track_t* const m_track;
public:
	guibutton_trackbypass(track_t* _track) : guibutton(), m_track(_track) {
		setActiveRef(&m_track->enabled);
		setEnabledRef(&m_track->enabled);
	}
};
class gui_trackcontrols_mixer: public guictr_base {
	track_t* const m_track;
	gui_trackmeter meter;
public:
	gui_trackgain gain;
	guibutton_trackbypass btnBypass;
	gui_trackcontrols_mixer(track_t* _track) :
		guictr_base(), m_track(_track), meter(_track), gain(_track), btnBypass(_track) {
		padding = 0;
		btnBypass.setColor(GUI_COLOR_HEX(G_S1));
		btnBypass.drawFn = drawTextureSymbol;
		btnBypass.drawParm = ICON_BYPASS;
		add(&btnBypass);
		add(&gain);
		add(&meter);
	}
	~gui_trackcontrols_mixer() {
		remove(&meter);
		remove(&gain);
		remove(&btnBypass);
	}
	void buttonClicked(guibase* button) override {
		if (&btnBypass == button) {
			m_track->enabled = !m_track->enabled;
		}
	}
	void layout() {
		int32_t inset = 4;
		int32_t i2 = inset * 2;
		int32_t h = TRACK_HEIGHT_STEP-i2;

		int32_t mW = TRACK_HEIGHT_STEP;
		int32_t bW = size.x-mW;
		int32_t gW = size.x-mW;
		btnBypass.size = ivec2(bW - i2, h);
		gain.size = ivec2(gW - i2, h);
		btnBypass.pos = ivec2(inset, inset);
		gain.pos = ivec2(inset, TRACK_HEIGHT_STEP+inset);

		meter.size = ivec2(mW-i2, size.y-i2);
		meter.pos = ivec2(size.x - mW+inset, inset);

	}

	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		meter.render(vg);
		gain.render(vg);
		btnBypass.render(vg);
	}
	bool isStaticContainer() {
		return false;
	}
};


class guidropdown_popup : public guictxtmenu_base {
public:
	guidropdown& parent;
	guidropdown_popup(guidropdown& _parent) : parent(_parent) {
		this->size.x = 120;
		this->fontSize = FONT_SIZE_CTXT_SMALL;
		this->paddingV = 0;
	}
	void clicked(int _id) {

//		String newVal = parent.cur;
		if (_id >= 0) {
			parent.setCur(this->entries[_id]->title);
		}
//		parent->setValue(getValue());
		MainCtrl::get()->closeContextMenu();
	}
};
class gui_trackcontrols_title : public guictr_base {
	track_t* const m_track;
	guidropdown automationSelectDevice;
	guidropdown automationSelectParam;
public:
	gui_trackcontrols_title(track_t* _track) :
		guictr_base(), m_track(_track) {
		padding = 0;
		add(&automationSelectDevice);
		add(&automationSelectParam);
	}
	~gui_trackcontrols_title() {
		remove(&automationSelectParam);
		remove(&automationSelectDevice);
	}
	bool isStaticContainer() {
		return false;
	}
	void layout() {
		int32_t inset = 4;
		int32_t i2 = inset * 2;
		int32_t h = TRACK_HEIGHT_STEP-i2;
		automationSelectDevice.pos = ivec2(inset, TRACK_HEIGHT_STEP + inset);
		automationSelectDevice.size = ivec2(size.x - i2, h);
		automationSelectParam.pos = ivec2(inset, TRACK_HEIGHT_STEP*2 + inset);
		automationSelectParam.size = ivec2(size.x - i2, h);
	}
	void buttonClicked(guibase* button) override {
		guidropdown_popup *popup = NULL;
		if (button == &automationSelectDevice) {
			popup = new guidropdown_popup(automationSelectDevice);
			std::vector<automatable_t*> targets;
			m_track->audio->getAutomatableTargets(targets);
			int32_t idx = 0;
			for (auto t : targets) {
				popup->add(new ctxtmenu_entry(t->getAutomatableName(), idx));
				idx++;
			}
		}
		if (button == &automationSelectParam) {
			popup = new guidropdown_popup(automationSelectParam);
			popup->add(new ctxtmenu_entry("asdf", 0));
			popup->add(new ctxtmenu_splitter());
		}
		if (popup) {
			popup->layout();
			popup->size.x = button->size.x-2;
			MainCtrl::get()->openContextMenu(popup, button->toScreenSpace(ivec2(0, button->size.y))-popup->pos+ivec2(1));
		}
	}
	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		NVGcolor color = rgbToNvg(m_track->rgb);
		ivec2 titleSize(size.x, size.y);
		MainCtrl* ctrl = MainCtrl::get();
		if (ctrl->getSelectedTrack() == m_track) {
			color = G_BLACK;
		}
		const int titleHeight = min(HEIGHT_TRACK_TITLE, size.y);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, titleSize.x, titleHeight);
		nvgFillColor(vg, color);
		nvgFill(vg);
		setFont(vg, (int) (titleHeight * 0.8), getContrastFontColorNvg(color), G_TITLE_ALIGN);
		renderText(vg, 0 + INSET_TITLE, 0 + titleHeight / 2, titleSize.x, StringAsCStr(m_track->name));

		int32_t y = titleHeight + titleHeight/2;
		String curvalue = "";
		String target = "<NULL>";
		trackdata_automation_t& automation = this->m_track->getAutomation();
//		my_printf("PLUGIN %08X ON %08X\n", automation.plugin, &automation);
		if (automation.plugin) {
			target = StringFormat("%s %d", StringAsCStr(automation.plugin->sName), automation.paramIdx);
		}
		curvalue = StringFormat("%f", automation.getValueAt(ctrl->cursor.cursorPos));
		//debug
		setFont(vg, (int) (titleHeight * 0.6), G_WHITE, G_TITLE_ALIGN);
		renderText(vg, 0 + INSET_TITLE, y, titleSize.x, StringAsCStr(target));
		y+=titleHeight;
		renderText(vg, 0 + INSET_TITLE, y, titleSize.x, StringAsCStr(curvalue));
		for (auto g : guis) {
			g->render(vg);
		}
	}
};

gui_track_controls::gui_track_controls(track_t* _track)
	: guictr_base(),
	  m_track(_track),
	  title(new gui_trackcontrols_title(_track)),
	  mixer(new gui_trackcontrols_mixer(_track)) {
	add(title);
	add(mixer);
	padding = 0;
}
gui_track_controls::~gui_track_controls() {
	remove(mixer);
	remove(title);
}
void gui_track_controls::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, size.x, size.y);
	nvgFillColor(vg, g_guiColors[COL_BG_BRT]);
	nvgFill(vg);
	MainCtrl* ctrl = MainCtrl::get();
	if (ctrl->getSelectedTrack() == m_track) {
		nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
		nvgFill(vg);
	}

	for (guibase* g : guis) {
		//content
		nvgSave(vg);
		g->render(vg);
		nvgRestore(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, g->right()+1.5f, g->top());
		nvgLineTo(vg, g->right()+1.5f, g->bottom());
		nvgStrokeColor(vg, g_guiColors[COL_LINE_SEPERATOR]);
		nvgStrokeWidth(vg, 3);
		nvgStroke(vg);
	}

}
void gui_track_controls::layout() {
	int32_t mxW = 160;
	int32_t titleW = size.x - mxW;
	mixer->size = ivec2(mxW - TRACK_HEIGHT_SPACING, size.y);
	title->size = ivec2(titleW - TRACK_HEIGHT_SPACING, size.y);
	title->pos = ivec2(TRACK_HEIGHT_SPACING / 2, 0);
	mixer->pos = ivec2(size.x - mixer->size.x + TRACK_HEIGHT_SPACING / 2, 0);
	for (guibase* g : guis) {
		g->layout();
	}
}
//t->mixer = new gui_trackmixer(t);
gui_track_controls* createTrackGuiMixer(track_t* t) {
//	switch (t->type) {
//	case TRACK_TYPE_RETURN:
//	case TRACK_TYPE_MASTER:
//	case TRACK_TYPE_MIDI:
//		return new gui_trackmixer(t);
//	case TRACK_TYPE_AUTOMATION:
//		return new gui_trackcontrols_title(t);
//	}
//	assert(0&&"unhandled track type");
	return new gui_track_controls(t);
}
gui_track* createTrackGui(track_t* t) {
	switch (t->type) {
	case TRACK_TYPE_RETURN:
	case TRACK_TYPE_MASTER:
		return new gui_track_audiochain(t);
	case TRACK_TYPE_MIDI:
		return new gui_track_midi(t);
	case TRACK_TYPE_AUTOMATION:
		return new gui_track_automation(t);
	}
	assert(0&&"unhandled track type");
	return NULL;
}
void gui_track_controls::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_track(this->m_track->idx), evt.mousepos);
}
