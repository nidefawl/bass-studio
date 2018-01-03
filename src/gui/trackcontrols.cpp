#include "trackcontrols.h"
#include <glm/geometric.hpp>

#include "track.h"
#include "guicontextmenu.h"
#include "button.h"
#include "event.h"
#include "../host/vst_plugin.h"
#include "trackautomation.h"
#include "track_audiodata.h"
#include "dropdown.h"
#include "dsp_util.h"
#include "str_util.h"
#include "color_util.h"
#include "leak_detect.h"

#define MTR_FLOOR -48.0f
#define MTR_CEIL 6.0f

float inline scaledRange(float db, float lvlFloor, float lvlCeil) {
	if (db < dsp_util::DBFS_FLOOR)
		return 1.0f;
	float lvlRange = lvlFloor - lvlCeil;
	return (max(lvlFloor, min(db, lvlCeil)) - lvlCeil) / lvlRange;
}

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


class guidropdown_popup_sel_automation_device : public guictxtmenu_base {
	track_t* const m_track;
public:
	guidropdown_popup_sel_automation_device(track_t* _track) : m_track(_track) {
		this->size.x = 120;
		this->fontSize = FONT_SIZE_CTXT_SMALL;
		this->paddingV = 0;
		std::vector<automatable_t*> targets;
		m_track->audio->getAutomatableTargets(targets);
		int32_t idx = 0;
		add(new ctxtmenu_entry("None", idx));
		idx++;
		for (auto t : targets) {
			add(new ctxtmenu_entry(t->getAutomatableName(), idx));
			idx++;
		}
	}
	void clicked(int _id) {
		std::vector<automatable_t*> targets;
		m_track->audio->getAutomatableTargets(targets);
		if (_id == 0) {
			m_track->audio->selectedAutomationCtr = NULL;
		} else {
			_id--;
			if (_id >= 0 && _id < targets.size()) {
				String str = targets[_id]->getAutomatableName();
				m_track->audio->selectedAutomationCtr = targets[_id];
				int32_t numParams = targets[_id]->getNumParameters();
				m_track->audio->selectedAutomationParam = numParams?0:-1;
			}
		}
		MainCtrl::get()->closeContextMenu();
		MainCtrl::get()->updateVisibleTrackContents();
	}
};
class guidropdown_popup_sel_automation_param : public guictxtmenu_base {
	track_t* const m_track;
public:
	guidropdown_popup_sel_automation_param(track_t* _track) : m_track(_track) {
		this->size.x = 120;
		this->fontSize = FONT_SIZE_CTXT_SMALL;
		this->paddingV = 0;
		automatable_t* autom = m_track->audio->selectedAutomationCtr;
		if (autom) {
			add(new ctxtmenu_entry("None", 0));
			int32_t numParams = autom->getNumParameters();
			for (int i = 0; i < numParams; i++) {
				String paramName = autom->getParamName(i);
				add(new ctxtmenu_entry(paramName, i+1));
			}
		}
	}
	void clicked(int _id) {
		std::vector<automatable_t*> targets;
		m_track->audio->getAutomatableTargets(targets);
		if (_id == 0) {
			m_track->audio->selectedAutomationParam = -1;
		} else {
			automatable_t* autom = m_track->audio->selectedAutomationCtr;
			if (autom) {
				int32_t numParams = autom->getNumParameters();
				_id--;
				if (_id >= 0 && _id < numParams) {
					m_track->audio->selectedAutomationParam = _id;
				}
			}
		}
		MainCtrl::get()->closeContextMenu();
		MainCtrl::get()->updateVisibleTrackContents();
	}
};
class guidropdown_automation_device : public guidropdownbase {
	track_t* const m_track;
public:
	guidropdown_automation_device(track_t* _track) :
		guidropdownbase(), m_track(_track) {
	}
	String getString() {
		automatable_t* automatable = m_track->audio->selectedAutomationCtr;
		return !automatable ? "None" : automatable->getAutomatableName();
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		guictxtmenu_base *popup =new guidropdown_popup_sel_automation_device(m_track);
		popup->size.x = 250;
		MainCtrl::get()->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
	}
};
class guidropdown_automation_param : public guidropdownbase {
	track_t* const m_track;
public:
	guidropdown_automation_param(track_t* _track) :
		guidropdownbase(), m_track(_track) {
	}
	String getString() {
		automatable_t* automatable = m_track->audio->selectedAutomationCtr;
		int32_t paramIdx = m_track->audio->selectedAutomationParam;
		return !automatable || paramIdx < 0 ? "None" : automatable->getParamName(paramIdx);
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		guictxtmenu_base *popup =new guidropdown_popup_sel_automation_param(m_track);
		popup->size.x = 250;
		MainCtrl::get()->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
	}
};
class gui_trackcontrols_title : public guictr_base {
	track_t* const m_track;
	guidropdown_automation_device automationSelectDevice;
	guidropdown_automation_param automationSelectParam;
public:
	gui_trackcontrols_title(track_t* _track) :
		guictr_base(), m_track(_track), automationSelectDevice(_track), automationSelectParam(_track) {
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
//		guictxtmenu_base *popup = NULL;
//		if (button == &automationSelectDevice) {
//			popup = new guidropdown_popup(m_track, automationSelectDevice);
//		}
//		if (button == &automationSelectParam) {
//			popup = new guidropdown_popup2(automationSelectParam);
//			popup->add(new ctxtmenu_entry("asdf", 0));
//			popup->add(new ctxtmenu_splitter());
//		}
//		if (popup) {
//			popup->layout();
//			popup->size.x = button->size.x-2;
//			MainCtrl::get()->openContextMenu(popup, button->toScreenSpace(ivec2(0, button->size.y))-popup->pos+ivec2(1));
//		}
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

		for (auto g : guis) {
			g->render(vg);
		}

		int32_t y = titleHeight + titleHeight/2;
		String curvalue = "UNDEF";
		String target = "<NULL>";
		track_plugins_t* data=this->m_track->audio;
		automatable_t* ctr = data->selectedAutomationCtr;
		if (ctr) {
			target = StringFormat("%s %08X", StringAsCStr(ctr->getAutomatableName()), ctr);
			int32_t idx = data->selectedAutomationParam;
			if (idx >= 0) {
				automation_t* automation = ctr->getAutomation(idx);
				if (automation) {
					curvalue = StringFormat("%s (%d) %f", StringAsCStr(ctr->getParamName(idx)), idx, automation->getValueAt(ctrl->cursor.cursorPos));
				} else {
					curvalue = StringFormat("%s (%d) UNDEF", StringAsCStr(ctr->getParamName(idx)), idx);
				}
			} else {
				curvalue = StringFormat("<NULL> %d", idx);
			}
		}
		 y += titleHeight + titleHeight;
		//debug
		setFont(vg, (int) (titleHeight * 0.6), G_WHITE, G_TITLE_ALIGN);
		renderText(vg, 0 + INSET_TITLE, y, titleSize.x, StringAsCStr(target));
		y+=titleHeight;
		renderText(vg, 0 + INSET_TITLE, y, titleSize.x, StringAsCStr(curvalue));
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
	delete mixer;
	delete title;
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
void gui_track_controls::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_track(this->m_track->idx), evt.mousepos);
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
