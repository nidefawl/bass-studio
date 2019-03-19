#include "trackcontrols.h"
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
using glm::vec2;
using glm::ivec2;

#include "host/mainctrl.h"
#include "host/plugin/vst_plugin.h"
#include "gui.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "theme.h"
#include "track.h"
#include "track_impl.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu.h"
#include "guicontextmenu_daw.h"
#include "button.h"
#include "event.h"
#include "renderresources.h"
#include "trackautomation.h"
#include "trackcontent.h"
#include "dropdown.h"
#include "dsp_util.h"
#include "str_util.h"
#include "guimeter.h"
#include "color_util.h"
#include "leak_detect.h"
#include "automation.h"
#include "automatable.h"
#include "meter.h"

const int resizeHitY = 8;
const int DRAG_RESIZE = 1;

int trackHeight(track_t* tr) {

	int trackheight = tr->height;
	for (auto t2 : tr->subtracks) {
		trackheight += t2->height;
	}
	return trackheight;
}
bool addTrHeight(track_t* tr, int32_t offset) {

	bool changed = false;
	int maxHeight = tr->subtracks.size() ? 4 : TRACK_MAX_HEIGHT;
	if (offset > 0 && tr->height < maxHeight) {
		tr->height++;
		return true;
	}
	for (auto t2 : tr->subtracks) {
		int32_t nHeight = min(TRACK_MAX_HEIGHT_SUB, max(TRACK_MIN_HEIGHT_SUB, t2->height+offset));
		changed = nHeight != t2->height;
		t2->height = nHeight;
	}
	if (offset < 0 && !changed) {
		int32_t nHeight = min(TRACK_MAX_HEIGHT_SUB, max(2, tr->height+offset));
		changed |= nHeight != tr->height;
		tr->height = nHeight;
	}
	return changed;
}
template<typename T, int minHeight=TRACK_MIN_HEIGHT_SUB, int maxHeight=TRACK_MAX_HEIGHT_SUB>
void resize(track_t* m_track, T* al, int32_t mouseDragDist, int32_t heightStep) {

	if (m_track->type < TRACK_TYPE_MIDI) {
		//resize content-lane on bottom-sticked tracks
		int32_t adjustedHeightSteps = min(128, max(1, (mouseDragDist) / heightStep));
		if (!m_track->subtracks.empty()) {
			int32_t curHeightSteps = trackHeight(m_track);
			int32_t distSteps = adjustedHeightSteps - al->height;
			if (distSteps && curHeightSteps != al->height) {
				while (distSteps) {
					int32_t distStepsBef = distSteps;
					for (auto t2 : m_track->subtracks) {
						if (distSteps > 0 && t2->height > TRACK_MIN_HEIGHT_SUB && al->height < maxHeight) {
							al->height++;
							t2->height--;
							distSteps--;
						}
						if (distSteps < 0 && t2->height < TRACK_MAX_HEIGHT_SUB && al->height > minHeight) {
							al->height--;
							t2->height++;
							distSteps++;
						}
						if (!distSteps) {
							break;
						}
					}
					if (distStepsBef == distSteps) {
						break;
					}
				}
			}
		}
	} else {

		int32_t totalHeightSteps = min(maxHeight, max(minHeight, (mouseDragDist) / heightStep));
		al->height = totalHeightSteps;
	}
}



class guictxtmenu_trackparam : public guictxtmenu {
	track_t* const track;
	automatable_t* const atl;
	int32_t const paramIdx;
public:
	guictxtmenu_trackparam(track_t* _track, automatable_t* _atl, int32_t _paramIdx) : track(_track), atl(_atl), paramIdx(_paramIdx)
	{
		this->size.x = 240;
		addContextEntriesAutomation(this, track, _atl, paramIdx);
	}
	void clicked(int _id) {
		handleAutomatbleContextMenu(track, atl, paramIdx, _id);
		MainCtrl::get()->closeContextMenu();
	}
};
class gui_trackgain: public guibase {
	track_t* const m_track;
	bool bEnabled = false;
public:
	gui_trackgain(track_t* _track) :
		guibase(), m_track(_track) {
	}
	void handleRightClick(MouseEvent& evt) override {
		MainCtrl::get()->openContextMenu(new guictxtmenu_trackparam(m_track, &m_track->audio->mixer, PARAM_TRACK_GAIN), evt.mousepos);
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
	bool isAutomated() {
		if (m_track->audio) {
			auto at = m_track->audio->mixer.getRegisteredAutomation(PARAM_TRACK_GAIN);
			return at && at->src.isAutomated();
		}
		return false;
	}
	void render(NVGcontext* vg) {
		renderWidgetBorder(vg, getStateFlags());
		if (drawFn) {
			drawFn(vg, pos, size, getBackgroundColor(getStateFlags()));
		}
		GuiColor::constant_t valColor;
		GuiColor::constant_t indColor;
		if (isAutomated()) {
			valColor = GuiColor::COL_AUTOMATED;
			indColor = GuiColor::COL_AUTOMATED;
		} else {
			indColor = GuiColor::COL_KNOB_IND;
			valColor = GuiColor::COL_KNOB;
		}
		track_impl_t* audio = m_track->audio;
		if (audio) {
			ivec2 insetP = pos+ivec2(1);
			ivec2 insetS = size-ivec2(2);
			float f = audio->mixer.getGain();
			float f2 = (f-dsp_util::GAIN_DBFLOOR) / (dsp_util::GAIN_DB6-dsp_util::GAIN_DBFLOOR);
			if (f2 <= 0) {
				f2 = 0;
			} else {
				f2 = pow(f2, 1/3.0f);
			}
			float x = insetP.x;
			float y = insetP.y;
			if (f2 > 0.01f) {
				float wVal = (f2) * insetS.x;
				nvgBeginPath(vg);
				nvgRect(vg, x, y, wVal, insetS.y);
				nvgFillColor(vg, theme->getColor(valColor));
				nvgFill(vg);
			}
			setFont(vg, 20, theme->getContrastColor(valColor), NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
			String strLvl = StringFormat("%.2f", dsp_util::dBFSClampInf6(audio->mixer.getGain()));
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
			int scale = isCtrl(evt.kbmods) ? 15 : 2;
			int disty = (int)evt.dragDistance->y / scale;
			if (!disty)
				return;
			evt.dragDistance->y = 0;
			track_impl_t* audio = m_track->audio;
			if (audio) {
				float fGain = audio->mixer.getGain();
				if (fGain < dsp_util::GAIN_DBFLOOR) {
					fGain = dsp_util::GAIN_DBFLOOR;
				}
				float dbfs = dsp_util::dBFS(fGain);
				float delta = 0.1f;
				for (int i = 1; i < 4; i++) {
					if (dbfs < -12*i) {
						delta *= 2;
					}
				}
				dbfs -= delta * disty;
				float f = dsp_util::fromdBFS(dbfs);
				float fNew = dsp_util::clampGain(f);
				audio->mixer.deactivateAutomation(1);
				audio->mixer.setGain(fNew);
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
	}
	bool trackenabled() const {
		return m_track->audio && m_track->audio->mixer.isEnabled();
	}
	bool isEnabled() const override {
		return trackenabled();
	}
	void handleRightClick(MouseEvent& evt) override {
		MainCtrl::get()->openContextMenu(new guictxtmenu_trackparam(m_track, &m_track->audio->mixer, PARAM_TRACK_ENABLED), evt.mousepos);
	}
};
class gui_trackcontrols_mixer: public guictr_base {
	track_t* const m_track;
	gui_trackmeter meter;
public:
	gui_trackgain gain;
	guibutton_trackbypass btnBypass;
	gui_trackcontrols_mixer(track_t* _track) :
		guictr_base(), m_track(_track), meter(&_track->audio->meter), gain(_track), btnBypass(_track) {
		padding = 0;
//		btnBypass.setTint(nvgToRGB(theme->getFrameColorOutline()));
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
			track_params_t& trackParams = m_track->audio->mixer;
			trackParams.deactivateAutomation(PARAM_ENABLE);
			trackParams.setParamValue(PARAM_ENABLE, trackParams.isEnabled() ? 0.0f : 1.0f, 0);
		}
	}
	void layout() {
		const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
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


class guidropdown_popup_sel_automation_device : public guictxtmenu {
	track_t* const m_track;
public:
	guidropdown_popup_sel_automation_device(track_t* _track) : m_track(_track) {
		this->size.x = 120;
		this->fontSize = FONT_SIZE_CTXT_SMALL;
		this->paddingV = 0;
		std::vector<automatable_t*> targets;
		m_track->audio->getAutomatableTrackTargets(targets);
		int32_t idx = 0;
		addEntry(new ctxtmenu_entry("None", idx));
		idx++;
		for (auto t : targets) {
			addEntry(new ctxtmenu_entry(t->getAutomatableName(), idx));
			idx++;
		}
	}
	void clicked(int _id) {
		std::vector<automatable_t*> targets;
		m_track->audio->getAutomatableTrackTargets(targets);
		if (_id == 0) {
			m_track->audio->selectedAutomationCtr = NULL;
		} else {
			_id--;
			if (_id >= 0 && _id < (int)targets.size()) {
				String str = targets[_id]->getAutomatableName();
				m_track->audio->selectedAutomationCtr = targets[_id];
				int32_t numParams = targets[_id]->getNumParameters();
				m_track->audio->selectedAutomationParam = numParams?0:-1;
			}
		}
		MainCtrl::get()->updateVisibleTrackContents();
		MainCtrl::get()->closeContextMenu();
	}
};
class guidropdown_popup_sel_automation_param : public guictxtmenu {
	track_t* const m_track;
public:
	guidropdown_popup_sel_automation_param(track_t* _track) : m_track(_track) {
		this->size.x = 120;
		this->fontSize = FONT_SIZE_CTXT_SMALL;
		this->paddingV = 0;
		automatable_t* autom = m_track->audio->selectedAutomationCtr;
		if (autom) {
			addEntry(new ctxtmenu_entry("None", 0));
			int32_t numParams = autom->getNumParameters();
			for (int i = 0; i < numParams; i++) {
				String paramName = autom->getParamName(i);
				addEntry(new ctxtmenu_entry(paramName, i+1));
			}
		}
	}
	void clicked(int _id) {
		std::vector<automatable_t*> targets;
		m_track->audio->getAutomatableTrackTargets(targets);
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
		MainCtrl::get()->updateVisibleTrackContents();
		MainCtrl::get()->closeContextMenu();
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
	guibuttontoggle hideTrack;
	guibuttontoggle hideAutomation;
	guibuttontoggle addAutomationLane;
	int dragMode = -1;
public:
	gui_trackcontrols_title(track_t* _track)
      :	guictr_base(), m_track(_track), automationSelectDevice(_track),
		automationSelectParam(_track), hideTrack(12), hideAutomation(10), addAutomationLane(10) {
		hideTrack.state = &m_track->hideTrack;
		hideAutomation.state = &m_track->hideAutomation;
		padding = 0;
		hideTrack.getIcon = [this]{return m_track->hideTrack?ICON_ARR_RIGHT:ICON_ARR_DOWN;};
		hideAutomation.getIcon = [this]{return m_track->hideAutomation?ICON_ARR_RIGHT:ICON_ARR_DOWN;};
		addAutomationLane.icon = ICON_PLUS;
		add(&hideTrack);
	}
	~gui_trackcontrols_title() {
		removeUNCHECKED(&hideAutomation);
		removeUNCHECKED(&hideTrack);
		removeUNCHECKED(&addAutomationLane);
		removeUNCHECKED(&automationSelectParam);
		removeUNCHECKED(&automationSelectDevice);
	}
	bool isStaticContainer() {
		return false;
	}
	void layout() {
		const int titleHeight = theme->get(GuiConstant::CONST_TRACK_HEIGHT_TITLE);
		const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		removeUNCHECKED(&automationSelectParam);
		removeUNCHECKED(&automationSelectDevice);
		removeUNCHECKED(&hideAutomation);

		int32_t inset = 4;
		int32_t i2 = inset * 2;
		int32_t h = TRACK_HEIGHT_STEP-i2;
		int32_t insetBtn = (TRACK_HEIGHT_STEP-hideTrack.size.y)/2;
		int32_t insetBtn2 = (TRACK_HEIGHT_STEP-hideAutomation.size.y)/2;
		const int32_t hideTrIns = (titleHeight-hideTrack.size.y)/2;
		hideTrack.pos = ivec2(hideTrIns, hideTrIns);

		automationSelectParam.size = ivec2(size.x - i2, h);
		automationSelectDevice.size = ivec2(size.x - i2, h);


		int32_t yCtrls = 0;
		int32_t hCtrls = size.y - TRACK_HEIGHT_STEP;
		if (hCtrls >= TRACK_HEIGHT_STEP*3) {
			yCtrls += TRACK_HEIGHT_STEP;
			addUNCHECKED(&automationSelectDevice);
			addUNCHECKED(&automationSelectParam);
			addUNCHECKED(&hideAutomation);
			addUNCHECKED(&addAutomationLane);
			automationSelectDevice.pos = ivec2(inset, yCtrls + inset);
			automationSelectParam.pos = ivec2(inset, yCtrls + TRACK_HEIGHT_STEP + inset);
			hideAutomation.pos = ivec2(inset, size.y-TRACK_HEIGHT_STEP+insetBtn2);
			addAutomationLane.pos = ivec2(size.x-inset-addAutomationLane.size.x, size.y-TRACK_HEIGHT_STEP+insetBtn2);
		} else if (hCtrls >= TRACK_HEIGHT_STEP*2) {
			yCtrls += TRACK_HEIGHT_STEP;
			addUNCHECKED(&automationSelectParam);
			addUNCHECKED(&hideAutomation);
			addUNCHECKED(&addAutomationLane);
			automationSelectParam.pos = ivec2(inset, yCtrls + inset);
			hideAutomation.pos = ivec2(inset, yCtrls + TRACK_HEIGHT_STEP + insetBtn2);
			addAutomationLane.pos = ivec2(size.x-inset-addAutomationLane.size.x, yCtrls + TRACK_HEIGHT_STEP + insetBtn2);
		} else if (hCtrls >= TRACK_HEIGHT_STEP) {
			yCtrls += TRACK_HEIGHT_STEP;
			addUNCHECKED(&automationSelectParam);
			automationSelectParam.pos = ivec2(inset, yCtrls + inset);
		}
		for (auto g : guis) {
			g->layout();
		}
	}
	bool isResize(ivec2 mpos) {
		int32_t resizeTopOrBottom = m_track->type < TRACK_TYPE_MIDI ? top() : bottom();
		return mpos.x >= left() && mpos.x < right() && mpos.y >= resizeTopOrBottom - resizeHitY
				&& mpos.y < resizeTopOrBottom + resizeHitY;
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (contains(mpos)) {
			ivec2 local = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(local, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void handleDraggedBegin(MouseEvent& evt) override {
		MainCtrl::get()->setSelectedTrack(m_track);
		if (isResize(evt.relMousepos+this->pos)) {
			dragMode = DRAG_RESIZE;
		}
	}

	void handleDraggedMove(MouseEvent& evt) override {
		if (dragMode == DRAG_RESIZE) {
			int32_t mouseDragDist = evt.relMousepos.y;
			int32_t heightStep = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
			resize<track_t, TRACK_MIN_HEIGHT, TRACK_MAX_HEIGHT>(m_track, m_track, mouseDragDist, heightStep);
			this->parent->onChildLayoutChanged(this);
		} else {
			parentCtrl->objectDragMove(this, evt);
		}
	}

	void handleDraggedRelease(MouseEvent& evt) override {
		if (dragMode == DRAG_RESIZE) {

		} else {
			parentCtrl->objectDragRelease(this, evt);
		}
		dragMode = -1;
	}
	void buttonClicked(guibase* button) override {
		if (button == &hideTrack) {
			m_track->hideTrack = !m_track->hideTrack;
		}
		if (button == &hideAutomation) {
			m_track->hideAutomation = !m_track->hideAutomation;
		}
		if (button == &addAutomationLane) {
			m_track->hideTrack = false;
			m_track->hideAutomation = false;
		}
		m_track->audio->showAutomationLanes();
		if (button == &addAutomationLane) {
			automatable_t* autom = m_track->audio->selectedAutomationCtr;
			int32_t param = m_track->audio->selectedAutomationParam;
			if (autom && param > -1) {
				MainCtrl::getGuiTrackCtr()->addAutomationLane(m_track, autom, param, true);
			}
		}
		MainCtrl::getGuiTrackCtr()->layout();
		MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
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
		const int titleHeight = theme->get(GuiConstant::CONST_TRACK_HEIGHT_TITLE);
		const int rectHeight = min(titleHeight, size.y);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, titleSize.x, rectHeight);
		nvgFillColor(vg, color);
		nvgFill(vg);
		setFont(vg, (int) (titleHeight * 0.8), getContrastFontColorNvg(color), G_TITLE_ALIGN);
		renderText(vg, hideTrack.right() + INSET_TITLE, 0 + titleHeight / 2, titleSize.x-hideTrack.right(), StringAsCStr(m_track->name));

		for (auto g : guis) {
			g->render(vg);
		}
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
		target->trackEntryDragMove(this->m_track->content, mousepos);
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
		target->trackEntryDragRelease(this->m_track->content, mousepos);
	}
	void handleRightClick(MouseEvent& evt) {
		parent->handleRightClick(evt);
	}
};

class gui_trackcontrols_automation : public guictr_base {
	track_t* const m_track;
public:
	gui_track_automationlane* const al;
private:
	guibuttontoggle removeLane;
	int dragMode = -1;
public:
	gui_trackcontrols_automation(track_t* _track, gui_track_automationlane* _al) :
		guictr_base(), m_track(_track), al(_al), removeLane(10) {
		padding = 0;
		removeLane.icon = ICON_MINUS;
		add(&removeLane);
	}
	~gui_trackcontrols_automation() {
		remove(&removeLane);
	}
	bool isStaticContainer() {
		return false;
	}
	void layout() {
		const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		int32_t insetBtn2 = (TRACK_HEIGHT_STEP-removeLane.size.y)/2;
		removeLane.pos = ivec2(size.x-removeLane.size.x-insetBtn2, insetBtn2);
	}
	void buttonClicked(guibase* button) override {
		if (button == &removeLane) {
			Cursor& cursor = MainCtrl::get()->cursor;
			int32_t laneIdx = this->al->idx;
			if (cursor.inSubTrack(m_track->idx, laneIdx)) {
				fixCursorSubRange(cursor, m_track->subtracks.size()-1);
			}
			MainCtrl::getGuiTrackCtr()->removeAutomationLane(this->al);
			MainCtrl::getGuiTrackCtr()->layout();
			MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
		}
	}
	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}

		for (auto g : guis) {
			g->render(vg);
		}

		MainCtrl* ctrl = MainCtrl::get();
		String curvalue = "UNDEF";
		String target = "<NULL>";
		automatable_t* ctr = al->at;
		if (ctr) {
			target = StringFormat("%s %08X", StringAsCStr(ctr->getAutomatableName()), ctr);
			int32_t idx = al->param;
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
		const int htt = theme->get(GuiConstant::CONST_TRACK_HEIGHT_TITLE);
		const int titleHeight = htt*4/5;
		const int fontSize = titleHeight-4;
		int32_t y = INSET_TITLE;
		//debug
		setFont(vg, fontSize, G_WHITE, G_TITLE_ALIGN);
		renderText(vg, 0 + INSET_TITLE, y+titleHeight / 2, size.x, StringAsCStr(target));
		y+=titleHeight;
		renderText(vg, 0 + INSET_TITLE, y+titleHeight / 2, size.x, StringAsCStr(curvalue));
	}
	bool isResize(ivec2 mpos) {
		int32_t resizeTopOrBottom = bottom();
		return mpos.x >= left() && mpos.x < right() && mpos.y >= resizeTopOrBottom - resizeHitY
				&& mpos.y < resizeTopOrBottom + resizeHitY;
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (isResize(mpos)) {
			evt.requestFocus(this);
			if (evt.type <= MouseHitType::MOUSE_RIGHT)
				evt.requestCursor(CURSOR_RESIZE_V);
			return true;
		}
		if (contains(mpos)) {
			ivec2 local = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(local, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true; // always need to return true if contained, parent has z-order
		}
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
			int32_t heightStep = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
			resize(m_track, al, mouseDragDist, heightStep);
			this->parent->onChildLayoutChanged(this);
		}
	}
	void handleRightClick(MouseEvent& evt) override {
		MainCtrl::get()->openContextMenu(new guictxtmenu_trackparam(m_track, al->at, al->param), evt.mousepos);
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
	for (gui_trackcontrols_automation* ctrl : automationLaneControls) {
		remove(ctrl);
		delete ctrl;
	}
	remove(mixer);
	remove(title);
	delete mixer;
	delete title;
}
void gui_track_controls::addAutomationLane(track_t* t, gui_track_automationlane* al) {
	gui_trackcontrols_automation* al_ctrl = new gui_trackcontrols_automation(t, al);
	automationLaneControls.push_back(al_ctrl);
	add(al_ctrl);
}
void gui_track_controls::removeAutomationLane(gui_track_automationlane* al) {
	auto& ctrls = automationLaneControls;
	auto it = std::find_if(ctrls.begin(), ctrls.end(), [al] (const gui_trackcontrols_automation* ref) {
		return ref->al == al;
	});
	assert(it != ctrls.end());
	remove(*it);
	delete (*it);
	ctrls.erase(it);
}
void gui_track_controls::removeAllAutomationLanes(automatable_t* at, int32_t paramIdx) {
	auto& ctrls = automationLaneControls;
	auto it = std::remove_if(ctrls.begin(), ctrls.end(), [this, at, paramIdx] (gui_trackcontrols_automation* ref) {
		if ((at == NULL || ref->al->at == at) && (paramIdx < 0 || ref->al->param == paramIdx)) {
			remove(ref);
			delete ref;
			return true;
		}
		return false;
	});
	ctrls.erase(it, ctrls.end());
}
void gui_track_controls::removeAllAutomationLanes(automatable_t* at) {
	removeAllAutomationLanes(at, -1);
}
void gui_track_controls::removeAllAutomationLanes() {
	removeAllAutomationLanes(NULL, -1);
}
void gui_track_controls::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, size.x, size.y);
	nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_BRT));
	nvgFill(vg);
	MainCtrl* ctrl = MainCtrl::get();
	if (ctrl->getSelectedTrack() == m_track) {
		nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_SELECTEDTRACK));
		nvgFill(vg);
	}

	for (guibase* g : guis) {
		//content
		nvgSave(vg);
		g->render(vg);
		nvgRestore(vg);
	}
	nvgBeginPath(vg);
	nvgMoveTo(vg, title->right(), 0);
	nvgLineTo(vg, title->right(), size.y);
	for (gui_trackcontrols_automation* g : automationLaneControls) {
		nvgMoveTo(vg, g->left(), g->top()-TRACK_HEIGHT_SPACING_HALF);
		nvgLineTo(vg, g->right(), g->top()-TRACK_HEIGHT_SPACING_HALF);
	}
	nvgStrokeColor(vg, theme->getColor(GuiColor::COL_LINE_SEPERATOR));
	nvgStrokeWidth(vg, 1);
	nvgStroke(vg);

}
bool canResizeTitleBar(track_t* tr) {
	return !tr->hideTrack&&!tr->hideAutomation&&tr->subtracks.size();
}
bool gui_track_controls::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	ivec2 local = this->toContainerSpace(mpos);
	bool contained = contains(mpos);
	if (contained) {
		for (guibase* gui : guis) {
			if (gui->mouseHitTest(local, evt)) {
				return true;
			}
		}
		evt.requestFocus(this);
	}
	if (evt.type <= MouseHitType::MOUSE_RIGHT) {
		guibase* g = NULL;
		if (m_track->type < TRACK_TYPE_MIDI) {
			if (isResize(mpos)) {
				g = this;
			} else if (canResizeTitleBar(m_track) && title->isResize(local)) {
				g = title;
			}
		} else {
			if (canResizeTitleBar(m_track) && title->isResize(local)) {
				g = title;
			} else if (isResize(mpos)) {
				g = this;
			}
		}
		if (g) {
			evt.requestFocus(g);
			evt.requestCursor(CURSOR_RESIZE_V);
			return true;
		}
	}
	return contained; // always need to return true if contained, parent has z-order
}
void gui_track_controls::layout() {
	const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
	int32_t mxW = 160;
	int32_t titleW = size.x - mxW;
	mixer->size = ivec2(mxW - TRACK_HEIGHT_SPACING, size.y);
	int32_t trH = m_track->hideTrack ? 1 : m_track->height;
	title->size = ivec2(titleW - TRACK_HEIGHT_SPACING, trH*TRACK_HEIGHT_STEP);
	title->pos = ivec2(TRACK_HEIGHT_SPACING_HALF, 0);
	mixer->pos = ivec2(size.x - mixer->size.x + TRACK_HEIGHT_SPACING_HALF, 0);
	for (gui_trackcontrols_automation* ctrl : automationLaneControls) {
		ctrl->pos = ivec2(title->pos.x, ctrl->al->pos.y-pos.y);
		ctrl->size = ivec2(title->size.x, ctrl->al->size.y);
	}
	for (guibase* g : guis) {
		g->layout();
	}
}

void gui_track_controls::handleDraggedMove(MouseEvent& evt) {
	const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
	if (dragMode == DRAG_RESIZE) {
		int32_t mouseDragDist = evt.relMousepos.y;
		bool resizeTop = m_track->type < TRACK_TYPE_MIDI;
		if (resizeTop) {
			mouseDragDist = -evt.relMousepos.y+size.y;
		}
		int32_t totalHeightSteps = min(128, max(1, (mouseDragDist) / TRACK_HEIGHT_STEP));
		if ( m_track->hideTrack && totalHeightSteps > TRACK_MIN_HEIGHT) {
			m_track->hideTrack = false;
			m_track->audio->showAutomationLanes();
		}
		int nChanged = 0;
		while (totalHeightSteps < trackHeight(m_track) && addTrHeight(m_track, -1)) {
			nChanged++;
		}
		while (totalHeightSteps > trackHeight(m_track) && addTrHeight(m_track, 1)) {
			nChanged++;
		}
		if (!nChanged && m_track->height == TRACK_MIN_HEIGHT && totalHeightSteps == TRACK_MIN_HEIGHT) {
			m_track->hideTrack = true;
			m_track->audio->showAutomationLanes();
		}
		this->parent->onChildLayoutChanged(this);
	}
}
String makeUniqueTrackName(const String& strNewName) {

	auto& trackCtr = MainCtrl::get()->getTracks();
	int offset = 0;
	while (offset < 100) {
		String test = strNewName;
		if (offset > 0) {
			test += StringFormat(" %d", offset);
		}
		auto it = std::find_if(trackCtr.begin(), trackCtr.end(), [&test](const track_t* tr) {
			return tr->name == test;
		});
		if (it == trackCtr.end())
			return test;
		offset++;
	}
	return strNewName;
}
class guictxtmenu_track : public guictxtmenu {
public:
	int32_t trackid;
	ctxtmenu_color_select* sel;
	guictxtmenu_track(int32_t _trackid) {
		this->trackid = _trackid;
		this->size.x = 120;
		sel = new ctxtmenu_color_select("Pick Color", 100);
		addEntry(new ctxtmenu_entry("Show all automation", 0));
		addEntry(new ctxtmenu_entry("Duplicate track", 1));
		addEntry(new ctxtmenu_entry("Delete track", 2));
		addEntry(new ctxtmenu_splitter());
		addEntry(sel);
		track_t* tr = MainCtrl::get()->getTrackId(trackid);
		MainCtrl::get()->setSelectedTrack(tr);
	}
	void clicked(int _id) {
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		track_t* tr = MainCtrl::get()->getTrackId(trackid);
		if (_id >= sel->id) {
			_id -= sel->id;
			if (tr) {
				int32_t col = colorPalette[_id];
				tr->rgb = col;
			}
		} else if (_id == 0) {
			gui_track_automationlane* gtr_at = NULL;
			if (tr) {
				tr->hideTrack = false;
				tr->hideAutomation = false;
				tr->audio->showAutomationLanes();
				auto trCtr = MainCtrl::getGuiTrackCtr();
				std::vector<automatable_t*> targets;
				tr->audio->getAutomatableTrackTargets(targets);
				for (automatable_t* atl : targets) {
					std::vector<int32_t> automated;
					atl->getAutomated(automated);
					for (int32_t param : automated) {
						gtr_at = trCtr->addAutomationLane(tr, atl, param, true);
					}
				}
			}
			if (gtr_at) {
				MainCtrl::getGuiTrackCtr()->layout();
				MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
				MainCtrl::getGuiTrackCtr()->scrollTo(gtr_at);
			}
		} else if (_id == 1) {
			if (tr) {
				track_t* track = MainCtrl::get()->insertNewTrack(tr->localIdx+1, tr->type);
				track->releaseTrackContent();
				String strNewName = StringFormat("%s copy", StringAsCStr(tr->name));

				track_snapshot_t trSnap(tr, true);
				*track = trSnap;
				track->loadSnapshot(trSnap);
				track->name = makeUniqueTrackName(strNewName);

				MainCtrl::getGuiTrackCtr()->layout();
				MainCtrl::get()->updateVisibleTrackContents();
				MainCtrl::getGuiTrackCtr()->scrollTo(track->content);
			}
		} else if (_id == 2) {
			MainCtrl::get()->removeTrackId(trackid);
		}
		MainCtrl::get()->closeContextMenu();
	}
};
void gui_track_controls::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_track(this->m_track->idx), evt.mousepos);
}
gui_track_controls* createTrackGuiMixer(track_t* t) {
	return new gui_track_controls(t);
}
