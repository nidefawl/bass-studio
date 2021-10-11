#include "trackcontrols.h"

#include "math/seq_math.h"
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
#include "textfield.h"
#include "button.h"
#include "event.h"
#include "renderresources.h"
#include "trackautomation.h"
#include "trackcontent.h"
#include "dropdown.h"
#include "dropdown_generic.h"
#include "dsp_util.h"
#include "str_util.h"
#include "guimeter.h"
#include "color_util.h"
#include "automation.h"
#include "automatable.h"
#include "subtrack.h"
#include "meter.h"
#include "guitooltip.h"
#include "appsettings.h"
#include "host/audio_host.h"
#include "host/projectcontroller.h"
#include "guimeter_render.h"
#include "trackctr_types.h"

const int resizeHitY = 8;
const int DRAG_RESIZE = 1;

int trackHeight(track_gui_entry_t* const m_trackentry) {
	int trackheight = m_trackentry->layout.height;
	for (auto t2 : m_trackentry->subtracks) {
		trackheight += t2->height;
	}
	return trackheight;
}
bool addTrHeight(track_gui_entry_t* const m_trackentry, int32_t offset) {

	bool changed = false;
	int maxHeight = m_trackentry->subtracks.size() ? 4 : TRACK_MAX_HEIGHT;
	if (offset > 0 && m_trackentry->layout.height < maxHeight) {
		m_trackentry->layout.height++;
		return true;
	}
	for (auto t2 : m_trackentry->subtracks) {
		int32_t nHeight = math::min(TRACK_MAX_HEIGHT_SUB, math::max(TRACK_MIN_HEIGHT_SUB, t2->height+offset));
		changed = nHeight != t2->height;
		t2->height = nHeight;
	}
	if (offset < 0 && !changed) {
		int32_t nHeight = math::min(TRACK_MAX_HEIGHT_SUB, math::max(2, m_trackentry->layout.height+offset));
		changed |= nHeight != m_trackentry->layout.height;
		m_trackentry->layout.height = nHeight;
	}
	return changed;
}
template<typename T, int minHeight=TRACK_MIN_HEIGHT_SUB, int maxHeight=TRACK_MAX_HEIGHT_SUB>
void resize(track_gui_entry_t* const m_trackentry, T* al, int32_t mouseDragDist, int32_t heightStep) {

	if (m_trackentry->track->type < TRACK_TYPE_MIDI) {
		//resize content-lane on bottom-sticked tracks
		int32_t adjustedHeightSteps = math::min(128, math::max(1, (mouseDragDist) / heightStep));
		if (!m_trackentry->subtracks.empty()) {
			int32_t curHeightSteps = trackHeight(m_trackentry);
			int32_t distSteps = adjustedHeightSteps - al->layout.height;
			if (distSteps && curHeightSteps != al->layout.height) {
				while (distSteps) {
					int32_t distStepsBef = distSteps;
					for (auto t2 : m_trackentry->subtracks) {
						if (distSteps > 0 && t2->height > TRACK_MIN_HEIGHT_SUB && al->layout.height < maxHeight) {
							al->layout.height++;
							t2->height--;
							distSteps--;
						}
						if (distSteps < 0 && t2->height < TRACK_MAX_HEIGHT_SUB && al->layout.height > minHeight) {
							al->layout.height--;
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

		int32_t totalHeightSteps = math::min(maxHeight, math::max(minHeight, (mouseDragDist) / heightStep));
		al->layout.height = totalHeightSteps;
	}
}
struct audio_info_t {
	String name;
	track_impl_t* audio;
};
template <>
void guitooltip<audio_info_t>::layout()  {

	using Table::tbl;
	using Table::tbl_row_t;
	using Table::table_entry_t;
	using Table::tblint;
	using Table::tblfloat;
	using Table::tblstr;
	using Table::tblString;
	size.x = 250;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
	{
		table.rows.push_back({{tblstr{"track"}, tblString{ptr->name}}});
		auto audio = ptr->audio;
		table.rows.push_back({{tblstr{"stageId"}, tblint{static_cast<int32_t>(ptr->audio->stageId.stageId)}}});
		table.rows.push_back({{tblstr{"inputStageId"}, tblint{static_cast<int32_t>(ptr->audio->stageId.inputStageId)}}});
		table.rows.push_back({{tblstr{"outputStageId"}, tblint{static_cast<int32_t>(ptr->audio->stageId.outputStageId)}}});
		table.rows.push_back({{tblstr{"outputPostStageId"}, tblint{static_cast<int32_t>(ptr->audio->stageId.outputPostStageId)}}});
		table.rows.push_back({{tblstr{"latency input "}, tblint{(int32_t)audio->getInputLatency()}}});
		table.rows.push_back({{tblstr{"latency intern"}, tblint{(int32_t)audio->getInternalLatency()}}});
		table.rows.push_back({{tblstr{"latency output"}, tblint{(int32_t)audio->getOutputLatency()}}});
		table.rows.push_back({{tblstr{"delayToPreReturn"}, tblint{audio->latencyInfo.delayToPreReturn}}});
		table.rows.push_back({{tblstr{"delayToPostReturn"}, tblint{audio->latencyInfo.delayToPostReturn}}});
		table.rows.push_back({{tblstr{"sampleRate"}, tblint{audio->sampleFormat.sampleRate}}});
	}
	Table::AdjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}
template <>
guitooltip<audio_info_t>::~guitooltip()  {
	removeGuis();
	delete ptr;
}
class gui_trackgain: public gui_textfield {
	automatable_t* paramAutomatable = nullptr;
	int32_t paramIdx = -1;
public:
	gui_trackgain() : gui_textfield() {
		setCanMouseHit(true);
		setAlignment(gui_textfield::Alignment::Center);
		setFontSize(20);
		mReturnCommits = true;
	}
	void setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx) {
		this->paramAutomatable = _paramAutomatable;
		this->paramIdx = _paramIdx;
	}
	void handleRightClick(MouseEvent& evt) override {
		dbgassert(paramAutomatable && paramIdx > -1 && paramAutomatable->getParam(paramIdx));
		parentCtrl->openContextMenu(new guictxtmenu_at_param(paramAutomatable, paramIdx), evt.mousepos);
	}
	bool isAutomated() {
		dbgassert(paramAutomatable && paramIdx > -1 && paramAutomatable->getParam(paramIdx));
		auto at = paramAutomatable->getRegisteredAutomation(paramIdx);
		return at && at->isAutomated();
	}
	void render(NVGcontext* vg) {
		renderWidgetBorder(vg, getStateFlags());
		GuiColor::constant_t valColor;
		GuiColor::constant_t indColor;
		if (isAutomated()) {
			valColor = GuiColor::COL_AUTOMATED;
			indColor = GuiColor::COL_AUTOMATED;
		} else {
			indColor = GuiColor::COL_KNOB_IND;
			valColor = GuiColor::COL_KNOB;
		}
		if (paramAutomatable && paramIdx > -1) {
			ivec2 insetP = pos+ivec2(1);
			ivec2 insetS = size-ivec2(2);
			float gainDb = dsp_util::linScaleToGain(paramAutomatable->getParamValue(paramIdx));
			float f2 = (gainDb-dsp_util::GAIN_DBFLOOR) / (dsp_util::GAIN_DB6-dsp_util::GAIN_DBFLOOR);
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
			setFont(vg, fontSize(), theme->getContrastColor(valColor), NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
			String strLvl = getValueAsString();
			nvgText(vg, insetP.x + insetS.x / 2.0f, insetP.y + G_FONT_MIDDLE_OFFSET(insetS.y), StringAsCStr(strLvl), NULL);
		}
		if (!mCommitted) {
			gui_textfield::render(vg);
		}
	}
	String getValueAsString() {
		float gainDb = dsp_util::linScaleToGain(paramAutomatable->getParamValue(paramIdx));
		return StringFormat("%.2f", dsp_util::dBFSClampInf6(gainDb));
	}
	bool focusEvent(MouseHitEvt& evt, bool focused)
	{
		if (!focused) {
			gui_textfield::focusEvent(evt, focused);
		}
		return true;
	}
	bool handleCharInput(unsigned int codepoint) override {
	    if (mCommitted && codepoint >= 0 && codepoint < 0xFF) {
	    	char keyChar = (char)codepoint;
	    	if ((keyChar >= '0' && keyChar <= '9')
	    		|| (keyChar == '-')) {
				MouseHitEvt evt(MouseHitType::MOUSE_LEFT, 0);
				gui_textfield::setValue(getValueAsString());
				gui_textfield::focusEvent(evt, true);
				gui_textfield::setSelectionRange(-1, -1);
			}
	    }
	    if (!mCommitted) {
	    	return gui_textfield::handleCharInput(codepoint);
        }
	    return false;
	}
	bool keyboardEvent(int key, int scancode, KeyEventType action, int modifiers) {

	    if (action == KeyEventType::K_PRESS && mCommitted) {
        	if ((key == KEY_ENTER || key == KEY_KP_ENTER)) {
    			MouseHitEvt evt(MouseHitType::MOUSE_LEFT, 0);
    			gui_textfield::setValue(getValueAsString());
    			gui_textfield::focusEvent(evt, true);
    			gui_textfield::setSelectionRange(-1, -1);
        	}
	    }

	    if (!mCommitted) {
	    	return gui_textfield::keyboardEvent(key, scancode, action, modifiers);
        }
        if (action == KeyEventType::K_PRESS || action == KeyEventType::K_REPEAT) {
            if (key == KEY_UP) {
            	float amt = -1.0f;
                if (modifiers == KB_MOD_SHIFT) {
                	amt *= 0.1f;
                }
				modifyGainLevel(amt, false);
                return true;
            } else if (key == KEY_DOWN) {
            	float amt = 1.0f;
                if (modifiers == KB_MOD_SHIFT) {
                	amt *= 0.1f;
                }
				modifyGainLevel(amt, false);
                return true;
            }
        }
	    return false;
    }
	void onTextEndEdit() override {
		String textFieldVal = value();
		float fTextFieldVal = atof(StringAsCStr(textFieldVal));
		float fGain = dsp_util::fromdBFSClampInf6(fTextFieldVal);
		if (fGain < dsp_util::GAIN_DBFLOOR) {
			fGain = dsp_util::GAIN_DBFLOOR;
		}
		float fNew = dsp_util::clampGain(fGain);
		paramAutomatable->deactivateAutomation(paramIdx);
		paramAutomatable->getParam(paramIdx)->value = dsp_util::gainToLinScale(fNew);
	}
	void handleDraggedBegin(MouseEvent& evt) {
		if (!mCommitted) {
			gui_textfield::handleDraggedBegin(evt);
			return;
		}
		if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
			MouseHitEvt evt(MouseHitType::MOUSE_LEFT, 0);
			gui_textfield::setValue(getValueAsString());
			gui_textfield::focusEvent(evt, true);
			gui_textfield::setSelectionRange(-1, -1);
			return;
		}
		if (evt.guiDragged == this) {
			parentCtrl->captureMouse(this);
		}
	}
	void modifyGainLevel(float amt, bool applyUserInputScaling) {
		float fGain = dsp_util::linScaleToGain(paramAutomatable->getParamValue(paramIdx));
		if (fGain < dsp_util::GAIN_DBFLOOR) {
			fGain = dsp_util::GAIN_DBFLOOR;
		}
		float dbfs = dsp_util::dBFS(fGain);
		float delta = 1.0f;
		if (applyUserInputScaling) {
			for (int i = 1; i < 4; i++) {
				if (dbfs < -12*i) {
					delta *= 2;
				}
			}
		}
		dbfs -= delta * amt;
		float f = dsp_util::fromdBFS(dbfs);
		float fNew = dsp_util::clampGain(f);
		paramAutomatable->deactivateAutomation(paramIdx);
		paramAutomatable->getParam(paramIdx)->value = dsp_util::gainToLinScale(fNew);
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (!mCommitted) {
			gui_textfield::handleDraggedMove(evt);
			return;
		}
		if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
			int scale = isCtrl(evt.kbmods) ? 15 : 2;
			int disty = (int)evt.dragDistance->y / scale;
			if (!disty)
				return;

			evt.dragDistance->y = 0;
			if (paramAutomatable && paramIdx > -1) {
				modifyGainLevel(disty * 0.1f, true);
			}
		}
	}
	void handleDraggedRelease(MouseEvent& evt) {
		if (!mCommitted) {
			gui_textfield::handleDraggedRelease(evt);
			return;
		}
	}
};


class guibutton_trackbypass : public guibuttonstate {
	track_t* const m_track;
	track_gui_entry_t* const m_trackentry;
public:
	guibutton_trackbypass(track_gui_entry_t* _entry) : guibuttonstate(), m_track(_entry->track), m_trackentry(_entry) {
	}
	bool trackenabled() const {
		return m_track->audio && m_track->audio->mixer.isEnabled();
	}
	bool getState() const override {
		return trackenabled();
	}
	void handleRightClick(MouseEvent& evt) override {
		parentCtrl->openContextMenu(new guictxtmenu_at_param(&m_track->audio->mixer, PARAM_ENABLE), evt.mousepos);
	}
};

class guibutton_track_solo : public guibuttonstate {
	track_t* const m_track;
	track_gui_entry_t* const m_trackentry;
public:
	guibutton_track_solo(track_gui_entry_t* _entry) : guibuttonstate(), m_track(_entry->track), m_trackentry(_entry) {
		setText("S");
	}
	NVGcolor getBackgroundColor(int stateflags) const override {
		if ((m_track->audio->flags & audiostageflags_t::SOLO) != audiostageflags_t::NONE) {
			return theme->getColor(GuiColor::COL_BTN_SOLO_BG_ENABLED);
		}
		if ((m_track->audio->flags & audiostageflags_t::SOLO_PARENT) != audiostageflags_t::NONE) {
			return theme->getColor(GuiColor::COL_BTN_SOLO_BG_PARENT);
		}
		return theme->getBgColor(stateflags);
	}
	bool getState() const override {
		if (m_track->audio) {
			return (m_track->audio->flags & (audiostageflags_t::SOLO|audiostageflags_t::SOLO_PARENT)) != audiostageflags_t::NONE;
		}
		return false;
	}
	void handleRightClick(MouseEvent& evt) override {
	}
};
namespace GuiColor {

constant_t COL_BTN_LOAD_DEF_PLUGINS("COL_BTN_LOAD_DEF_PLUGINS", 0xFFFFFFFF);
}
class gui_subtrack_waveview;

std::shared_ptr<guibase> getMeter(int32_t t, rmsmeter<16000>* meter);

/* track io menus */
class ctxtmenu_entry_track_io : public ctxtmenu_entry {
public:
	ctxtmenu_entry_track_io(int32_t _id, String name) : ctxtmenu_entry(name, _id) {

	}
	~ctxtmenu_entry_track_io() {
	}
	virtual bool isBus() = 0;
};
class ctxtmenu_entry_bus : public ctxtmenu_entry_track_io {
public:
	const DAW::bus_type busType;
	const String busName;
	const audio_channel_ref_t stageEndpoint;
	bool isMenuOpen = false;

	ctxtmenu_entry_bus(int32_t _id, String name, DAW::bus_type bustype, audio_channel_ref_t _stageEndpoint)
	: ctxtmenu_entry_track_io(_id, name), busType(bustype), busName(name), stageEndpoint(_stageEndpoint) {
	}
//				virtual ~ctxtmenu_entry_channel() {
//
//				}
	void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
		if (contains(ctxtSize, mouse)) {
			nvgBeginPath(vg);
			nvgRect(vg, 0, y, ctxtSize.x, height);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
			nvgFill(vg);
		}
		UTIL_setFont(vg, theme, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(title), NULL);
//					if (channel.idx > -1) {
//						auto* stream = audiohost::getInstance()->getStream(0);
//						if (stream) {
//
//							auto& allMeters = isInput ? stream->metersInput : stream->metersOutput;
//							int32_t nChannels = AudioIO::getNumChannelsFromTrackType(channel.type);
//							auto rmsMtr = rmsmeter<16000>(allMeters.channels+channel.channelOffset, nChannels);
//							ivec2 sizeMeter{height-2, height-2};
//							renderMeterAt(vg, theme, {width-sizeMeter.x+1, y+1}, sizeMeter, &rmsMtr);
//						}
//					}
	}
	bool isBus() override {
		return true;
	}
};
class ctxtmenu_entry_bus_external : public ctxtmenu_entry_bus {
	public:
		ctxtmenu_entry_bus_external(int32_t _id, String name, audio_channel_ref_t _stageEndpoint)
		: ctxtmenu_entry_bus(_id, name, DAW::bus_type::external, _stageEndpoint) {
		}

};
class ctxtmenu_entry_bus_internal : public ctxtmenu_entry_bus {
	const audio_stage_ref_t busStage;
	public:
		ctxtmenu_entry_bus_internal(int32_t _id, String name, audio_stage_ref_t _stageBus, audio_channel_ref_t _stageEndpoint)
		: ctxtmenu_entry_bus(_id, name, DAW::bus_type::internal, _stageEndpoint), busStage(_stageBus) {
		}
	audio_stage_ref_t getStageRef() {
		return busStage;
	}
};


class ctxtmenu_entry_endpoint : public ctxtmenu_entry_track_io {
public:
	ctxtmenu_entry_endpoint(int32_t _id, String name) : ctxtmenu_entry_track_io(_id, name) {

	}
	virtual DAW::channel_ref_t getEndpoint() = 0;

};
class ctxtmenu_entry_external_channel : public ctxtmenu_entry_endpoint {
public:
	const AudioIO::io_cfg_channel channel;
	const stagebuffer_point isInput;

	explicit ctxtmenu_entry_external_channel(int32_t _id, const AudioIO::io_cfg_channel& _channel, stagebuffer_point _isInput)
	: ctxtmenu_entry_endpoint(_id, _channel.name), channel(_channel), isInput(_isInput) {
	}
	explicit ctxtmenu_entry_external_channel(int32_t _id, String name, stagebuffer_point _isInput)
	: ctxtmenu_entry_endpoint(_id, name), channel(), isInput(_isInput) {
	}
	void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
		if (contains(ctxtSize, mouse)) {
			nvgBeginPath(vg);
			nvgRect(vg, 0, y, ctxtSize.x, height);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
			nvgFill(vg);
		}
		UTIL_setFont(vg, theme, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(title), NULL);
		if (channel.idx > -1) {
			auto* stream = audiohost::getInstance()->getStream(0);
			if (stream) {

				auto& allMeters = isInput == stagebuffer_point::INPUT ? stream->metersInput : stream->metersOutput;
				int32_t nChannels = AudioIO::getNumChannelsFromTrackType(channel.type);
				auto rmsMtr = rmsmeter<16000>(allMeters.channels+channel.channelOffset, nChannels);
				ivec2 sizeMeter{height-2, height-2};
				renderMeterAt(vg, theme, {width-sizeMeter.x+1, y+1}, sizeMeter, &rmsMtr);
			}
		}
	}
	bool isBus() override {
		return false;
	}
	DAW::channel_ref_t getEndpoint() override {
//		if (id == 0)
//			return ChannelNone();
		return DAW::ChannelAudioInput(channel.idx, channel.channelOffset, "External "+AudioIO::getTrackNameShort(channel.type, channel.idx, isInput), channel.type);
	}
};
class ctxtmenu_entry_stage_channel : public ctxtmenu_entry_endpoint {
public:
	const audio_channel_ref_t endpoint;

	ctxtmenu_entry_stage_channel(int32_t _id, String name, audio_channel_ref_t _endpoint)
	: ctxtmenu_entry_endpoint(_id, name), endpoint(_endpoint) {
	}
	void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
		if (contains(ctxtSize, mouse)) {
			nvgBeginPath(vg);
			nvgRect(vg, 0, y, ctxtSize.x, height);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
			nvgFill(vg);
		}
		UTIL_setFont(vg, theme, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(title), NULL);
		audio_stage_t* stage = vsthost::getInstance()->getAudioStage(endpoint.stageRef);
		if (stage) {
			track_impl_t* trImpl = dynamic_cast<track_impl_t*>(stage);
			dbgassert(trImpl);
			if (trImpl) {
				auto rmsMtr = rmsmeter<16000>(trImpl->meter.channels, trImpl->input.channels);
				ivec2 sizeMeter{height-2, height-2};
				renderMeterAt(vg, theme, {width-sizeMeter.x+1, y+1}, sizeMeter, &rmsMtr);
			}
		}

	}
	bool isBus() override {
		return false;
	}
	DAW::channel_ref_t getEndpoint() override {
		audio_stage_t* stage = vsthost::getInstance()->getAudioStage(endpoint.stageRef);
		if (stage) {
			track_impl_t* trImpl = dynamic_cast<track_impl_t*>(stage);
			dbgassert(trImpl);
			if (trImpl) {
				return DAW::ChannelStage(trImpl, endpoint.buffer);

			}
		}
		return DAW::ChannelNone();
	}
};
class ctxtmenu_entry_default_channel : public ctxtmenu_entry_endpoint {
public:
	ctxtmenu_entry_default_channel(int32_t _id, String name)
	: ctxtmenu_entry_endpoint(_id, name) {
	}
	void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
		if (contains(ctxtSize, mouse)) {
			nvgBeginPath(vg);
			nvgRect(vg, 0, y, ctxtSize.x, height);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
			nvgFill(vg);
		}
		UTIL_setFont(vg, theme, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(title), NULL);
		//TODO: resolve actual dst/src and show name and levels
//		audio_stage_t* stage = vsthost::getInstance()->getAudioStage(endpoint.stageRef);
//		if (stage) {
//			track_impl_t* trImpl = dynamic_cast<track_impl_t*>(stage);
//			dbgassert(trImpl);
//			if (trImpl) {
//				auto rmsMtr = rmsmeter<16000>(trImpl->meter.channels, trImpl->input.channels);
//				ivec2 sizeMeter{height-2, height-2};
//				renderMeterAt(vg, theme, {width-sizeMeter.x+1, y+1}, sizeMeter, &rmsMtr);
//			}
//		}

	}
	bool isBus() override {
		return false;
	}
	DAW::channel_ref_t getEndpoint() override {
		return DAW::ChannelDefaultNone();
	}
};

/* top select menu */
class guidropdown_select_bus_ctxt : public guictxtmenu {
	const audio_stage_ref_t busStage;
	const audio_channel_ref_t stageEndpoint;
	void init() {
		this->size.x = 120;
		this->fontSize = FONT_SIZE_CTXT_SMALL;
		this->paddingV = 0;
	}
public:
	guidropdown_select_bus_ctxt(audio_stage_ref_t _busStage, audio_channel_ref_t _dstStage)
		: busStage(_busStage), stageEndpoint(_dstStage) {

		int32_t idx = 0;
		if (_dstStage.buffer != stagebuffer_point::INPUT) {
			addEntry(new ctxtmenu_entry_stage_channel(idx++, "Input", audio_channel_ref_t{_busStage, stagebuffer_point::INPUT}));
		} else {
			addEntry(new ctxtmenu_entry_stage_channel(idx++, "Output", audio_channel_ref_t{_busStage, stagebuffer_point::OUTPUT_POST}));
		}
		audio_stage_t* stage = vsthost::getInstance()->getAudioStage(stageEndpoint.stageRef);
		if (stage) {
			track_impl_t* trImpl = dynamic_cast<track_impl_t*>(stage);
			dbgassert(trImpl);
			if (trImpl) {
				dbgassert(trImpl->getTrack());
				auto& childTracks = trImpl->getTrack()->children;
				for (track_t* childTrack : childTracks) {
					dbgassert(childTrack->audio);
					addEntry(new ctxtmenu_entry_bus_internal(idx, childTrack->name, childTrack->audio->toRef(), stageEndpoint));
					idx++;
				}

			}
		}

	}
	guidropdown_select_bus_ctxt(const AudioIO::io_cfg_tracks& cfg, audio_channel_ref_t _dstStage)
		: busStage(AudioStageRefNULL()), stageEndpoint(_dstStage) {
//		guidropdown_select_channel_ctxt
		int32_t idx = 0;
		auto& list = stageEndpoint.buffer == stagebuffer_point::INPUT ? cfg.input : cfg.output;
		for (auto& channel : list) {
			addEntry(new ctxtmenu_entry_external_channel(idx, channel, _dstStage.buffer));
			idx++;
		}
	}
	guidropdown_select_bus_ctxt(audio_channel_ref_t _stageEndpoint, int lvl = 0)
		: busStage(AudioStageRefNULL()), stageEndpoint(_stageEndpoint) {
		int32_t idx = 0;
		String inputName = stageEndpoint.buffer == stagebuffer_point::INPUT ? "External input" : "External output";
		addEntry(new ctxtmenu_entry_stage_channel(idx++, "None", AudioChannelRefNULL()));
		addEntry(new ctxtmenu_entry_default_channel(idx++, "Default"));
		addEntry(new ctxtmenu_entry_bus_external(idx++, inputName, stageEndpoint));
//				auto& cfg = settings.iosettings.getChannelConfig(settings.iosettings.device_api);
//				auto& list = isInput ? cfg.input : cfg.output;
		project_t* project = project_controller_t::get()->getProject();
		dbgassert(project);
		if (project) {
			auto& tracks = project->trackList;
			for (track_t* track : tracks) {
				dbgassert(track->audio);
				addEntry(new ctxtmenu_entry_bus_internal(idx, track->name, track->audio->toRef(), stageEndpoint));
				idx++;
			}
		}
	}
	void addEntry(ctxtmenu_entry* entry) = delete;
	void addEntry(ctxtmenu_entry_track_io* entry) {
		guictxtmenu::addEntry(entry);
	}
	virtual void clickedElement(ctxtmenu_entry* e, int _id) {
		auto ctxtEndpointEntry = static_cast<ctxtmenu_entry_track_io*>(e);
		if (ctxtEndpointEntry->isBus()) {
			return;
		}
		closeContextMenu();
		if (parentCtrl)
			parentCtrl->closeAllContextMenus();
		if (MainCtrl::get())
			MainCtrl::get()->closeAllContextMenus();
		dbgassert(dynamic_cast<ctxtmenu_entry_endpoint*>(e));
		auto entry = static_cast<ctxtmenu_entry_endpoint*>(e);
		audio_stage_t* stage = vsthost::getInstance()->getAudioStage(stageEndpoint.stageRef);
		if (!stage)
			return;

		track_impl_t* trImpl = dynamic_cast<track_impl_t*>(stage);
		dbgassert(trImpl);
		if (!trImpl)
		  return;
		if (stageEndpoint.buffer == stagebuffer_point::INPUT) {
			trImpl->inputChannel = entry->getEndpoint();
		} else {
			trImpl->outputChannel = entry->getEndpoint();
		}

	}
	void closeAllSubmenus() {
		BaseCtrl* appCtrlParent = getControl();
		bool anyOpen = false;
		for (ctxtmenu_entry* ctxtEntry : entries) {
			auto entry = dynamic_cast<ctxtmenu_entry_bus*>(ctxtEntry);
			if (entry) {
				anyOpen |= entry->isMenuOpen;
				entry->isMenuOpen = false;
			}
		}
		if (anyOpen) {
			//close all menus deeper than this menu
			appCtrlParent->closeAppMenusAtLvl(1);
		}
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			ctxtmenu_entry* entryHit = NULL;
			for (ctxtmenu_entry* e : entries) {
				int n = e->getClicked(size, localMouse);
				if (n >= 0) {
					entryHit = e;
					break;
				}
			}
			if (!entryHit) {
				//TODO: maybe defer closing for usability
				closeAllSubmenus();
			}

			auto entry = dynamic_cast<ctxtmenu_entry_bus*>(entryHit);
			if (entry && !entry->isMenuOpen) {
				//close other submenu at same level
				closeAllSubmenus();

				//and open new one
				guictxtmenu_base *popup = nullptr;
				if (entry->busType == DAW::bus_type::internal) {
					auto stageEntry = dynamic_cast<ctxtmenu_entry_bus_internal*>(entry);
					dbgassert(stageEntry);
					if (stageEntry) {
						popup = new guidropdown_select_bus_ctxt(stageEntry->getStageRef(), stageEndpoint);
					}

				}
				if (entry->busType == DAW::bus_type::external) {
					auto& cfg = settings.iosettings.getChannelConfig(settings.iosettings.device_api);
					popup = new guidropdown_select_bus_ctxt(cfg, stageEndpoint);
				}
				dbgassert(popup);
				if (popup) {
					popup->size = size;
					popup->setFontSize(entry->fontSize);
					popup->size.x = math::max(250, popup->size.x);
					ivec2 vPos(right() + 2, pos.y + entryHit->y);
					parentCtrl->openAppMenu(1, popup, vPos);
					entry->isMenuOpen = true;
				}
			}
			for (guibase* gui : guis) {
				if (!gui->isVisible())
					continue;
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
			if (canMouseHit()) {
				evt.requestFocus(this);
				return true;
			}
		}
		return false;
	}
};

class guidropdown_select_bus : public guidropdownbase {
	track_t* const track;
	const bool isInput;
public:
	guidropdown_select_bus(track_gui_entry_t* _entry, const bool _isInput) : guidropdownbase(), track(_entry->track), isInput(_isInput) {
	}
	String getString() {
		track_impl_t* trImpl = track->audio;
		dbgassert(trImpl);
		if (trImpl) {
			auto& channel = isInput ? trImpl->inputChannel : trImpl->outputChannel;
			project_t* project = project_controller_t::get()->getProject();
			dbgassert(project);
			if (project) {
				vsthost* const host = vsthost::getInstance();
				if (channel.type == DAW::channel_input_type::INPUT_DEFAULT) {
					DAW::channel_ref_t out;
					if (DAW::resolveDefaultConnection(host, project, trImpl, isInput, out)) {
						return out.name;
					}
		//			if (stageEndpoint.isInput) {
		//				return "Default";
		//			}
					return "Default";
				}
			}
			return channel.name;
		}
		return "<Invalid Track>";
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		track_impl_t* trImpl = track->audio;
		dbgassert(trImpl);
		if (!trImpl)
			return;
		guictxtmenu_base *popup = new guidropdown_select_bus_ctxt(audio_channel_ref_t{trImpl->toRef(), isInput ? stagebuffer_point::INPUT : stagebuffer_point::OUTPUT_POST});
		popup->size = size;
		popup->setFontSize(size.y);
		popup->size.x = math::max(250, popup->size.x);
		this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
	}
};
class gui_trackcontrols_io : public guictr_base {


/* previous dropdown version. external channels only! */
//	class guidropdown_select_channel : public guidropdownbase {
//		track_t* const m_track;
//		const bool isInput;
//	public:
//		guidropdown_select_channel(track_t* _track, bool _isInput) :
//			guidropdownbase(), m_track(_track), isInput(_isInput) {
//		}
//		String getString() {
//			return isInput ? m_track->audio->inputChannel.name : m_track->audio->outputChannel.name;
//		}
//		virtual void handleDraggedRelease(MouseEvent& evt) {
//			guictxtmenu_base *popup = new guidropdown_select_channel_ctxt(m_track, isInput);
//			popup->size = size;
//			popup->setFontSize(size.y);
//			popup->size.x = math::max(250, popup->size.x);
//			this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
//		}
//	};
	;
//	track_t* const m_track;
	guidropdown_select_bus selectInput;
	guidropdown_select_bus selectOutput;
public:
	gui_trackcontrols_io(track_gui_entry_t* _entry) :
		guictr_base(),/* m_track(_track), */selectInput(_entry, true), selectOutput(_entry, false) {
		add(&selectInput);
		add(&selectOutput);
		padding = 0;
	}
	~gui_trackcontrols_io() {
		remove(&selectOutput);
		remove(&selectInput);
	}
	void layout() {
		const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		const int32_t CONST_LAYOUT_MARGIN = math::min(6, theme->get(GuiConstant::CONST_LAYOUT_MARGIN));
		int32_t inset = CONST_LAYOUT_MARGIN;
		selectInput.pos = ivec2(inset, inset);
		selectOutput.pos = ivec2(inset, TRACK_HEIGHT_STEP+inset);
		selectInput.size = getSizeContent() - ivec2(inset*2);
		selectInput.size.y = TRACK_HEIGHT_STEP-inset*2;
		selectOutput.size = selectInput.size;
		for (auto gui : guis) {
			gui->layout();
		}
	}

	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		for (auto gui : guis) {
			if (gui->isVisible()) {
				gui->render(vg);
			}
		}
	}
	bool isStaticContainer() {
		return false;
	}
};
class gui_trackcontrols_mixer: public guictr_base {
	track_t* const m_track;
	track_gui_entry_t* const m_trackentry;
	gui_trackmeter<16000,2> meter;
public:
	gui_trackgain gain;
	guibutton_trackbypass btnBypass;
	guibutton_track_solo btnSolo;
	guibutton btnActivate;
	std::vector<gui_trackgain*> sendGains;
	gui_trackcontrols_mixer(track_gui_entry_t* _entry) :
		guictr_base(), m_track(_entry->track), m_trackentry(_entry), meter(&_entry->track->audio->meter), btnBypass(_entry), btnSolo(_entry) {
		gain.setAutomationRef(&m_track->audio->mixer, PARAM_TRACK_GAIN);
		padding = 0;
//		btnBypass.setTint(nvgToRGB(theme->getFrameColorOutline()));
		btnBypass.drawFn = drawTextureSymbol;
		btnBypass.drawParm = ICON_BYPASS;
		btnBypass.setFlag(FLG_RENDER_BUTTON_WITH_LED, true);
		btnActivate.setButtonColor(GuiColor::COL_PLUG_TITLE);
		gain.setLabel("Gain Level");
		btnActivate.setLabel("Load plugins");
		add(&btnBypass);
		add(&btnSolo);
		add(&btnActivate);
		add(&gain);
		add(&meter);
		if (m_track->type != TRACK_TYPE_MASTER && m_track->type != TRACK_TYPE_RETURN) {
			sendGains.resize(MAX_SEND_CHANNELS);
			for (int i = 0; i < MAX_SEND_CHANNELS; i++) {
				sendGains[i] = new gui_trackgain();
				sendGains[i]->setVisible(false);
				sendGains[i]->setAutomationRef(&m_track->audio->mixer, PARAM_OFFSET_SEND + i);
				sendGains[i]->setLabel(m_track->audio->mixer.getParamName(PARAM_OFFSET_SEND + i));
				add(sendGains[i]);
			}
		}
	}
	~gui_trackcontrols_mixer() {
		for (auto* sendGainCtrl : sendGains) {
			remove(sendGainCtrl);
			delete sendGainCtrl;
		}
		remove(&meter);
		remove(&gain);
		remove(&btnActivate);
		remove(&btnSolo);
		remove(&btnBypass);
	}
	void buttonClicked(guibase* button) override {
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		if (&btnSolo == button) {
			bool isSolo = (m_track->audio->flags & audiostageflags_t::SOLO) != audiostageflags_t::NONE;
			DawInstance::get()->setSoloState(m_track->audio->toRef(), !isSolo);
		}
		if (&btnBypass == button) {
			track_params_t& trackParams = m_track->audio->mixer;
			trackParams.deactivateAutomation(PARAM_ENABLE);
			trackParams.setParamValue(PARAM_ENABLE, trackParams.isEnabled() ? 0.0f : 1.0f, 0);
		}
		if (&btnActivate == button) {
			vsthost* host = vsthost::getInstance();
			std::vector<effectbase*> effects;
			m_track->audio->getDeferredEffects(effects);
			for (auto effect : effects) {
				host->activateDeferred(effect, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
			}
			host->postPluginLoaded(m_track->audio, nullptr);
			if (dawCtrl) dawCtrl->onPluginsChanged();

#ifndef NDEBUG
			log_printf("deferredEffects post activateDeferred on track %s: %d\n", m_track->szName, m_track->audio->deferredEffects.size());
#endif	
		}
	}
	void onTick(AppCtrl* ctrl) {
		for (guibase* gui : guis) {
			if (gui->isVisible()) {
				gui->onTick(ctrl);
			}
		}

		std::vector<effectbase*> effects;
		dbgassert(m_track->audio);
		m_track->audio->getDeferredEffects(effects);
		int nDefEffects = effects.size();
		btnActivate.setEnabled(nDefEffects>0);
		btnActivate.setText(nDefEffects>9?"9+":(StringFormat("%d", nDefEffects)));
	}
	void layout() {

		const int32_t CONST_LAYOUT_MARGIN = math::min(6, theme->get(GuiConstant::CONST_LAYOUT_MARGIN));
		const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		int32_t inset = CONST_LAYOUT_MARGIN;
		int32_t i2 = inset * 2;
		int32_t h = TRACK_HEIGHT_STEP-i2;

		int32_t mW = TRACK_HEIGHT_STEP*3;
		int32_t bW = size.x-mW;
		int32_t gW = size.x-mW;
		btnBypass.size = ivec2(bW - inset * 3 - h, h);
		gain.size = ivec2(gW - i2, h);
		btnBypass.pos = ivec2(inset, inset);
		btnSolo.pos = ivec2(bW - inset - h, inset);
		btnSolo.size = ivec2(h, h);
		gain.pos = ivec2(inset, TRACK_HEIGHT_STEP+inset);
		btnActivate.pos = {inset, gain.bottom()+i2};
		btnActivate.setFontSize(h-2);
		btnActivate.size = { h, h };

		meter.size = ivec2(mW-i2, size.y-i2);
		meter.pos = ivec2(size.x - mW+inset, inset);
		if (sendGains.size()) {
			const int32_t HEIGHT_SEND_GAIN = h;
			const int32_t SEND_PER_ROW = 1;
			ivec2 sendPos = {inset, btnActivate.bottom()+i2 };
			project_t* project = project_controller_t::get()->getProject();
			dbgassert(project);
			int32_t numReturnChannels = project->trackReturnCtr.size();
			int pos = 0;
			for (auto sendGainCtrl : sendGains) {
				sendGainCtrl->setVisible(pos < numReturnChannels);
				sendGainCtrl->pos = sendPos;
				sendGainCtrl->size = {gW/SEND_PER_ROW - i2, HEIGHT_SEND_GAIN};
				if (++pos%SEND_PER_ROW == 0) {
					sendPos.x = inset;
					sendPos.y += HEIGHT_SEND_GAIN + i2;
				} else {
					sendPos.x = sendGainCtrl->right() + i2;
				}
			}
		}
		for (auto gui : guis) {
			gui->layout();
		}
	}

	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		for (auto gui : guis) {
			if (gui->isVisible()) {
				gui->render(vg);
			}
		}
	}
	bool isStaticContainer() {
		return false;
	}
};


class guidropdown_popup_sel_automation_device : public guictxtmenu {
	track_gui_entry_t* const m_trackentry;
public:
	guidropdown_popup_sel_automation_device(track_gui_entry_t* const trackentry) : m_trackentry(trackentry) {
		this->size.x = 120;
		this->fontSize = FONT_SIZE_CTXT_SMALL;
		this->paddingV = 0;
		std::vector<automatable_t*> targets;
		trackentry->track->getStage()->getAutomatableTrackTargets(targets);
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
		auto* trImpl = m_trackentry->track->getStage();
		trImpl->getAutomatableTrackTargets(targets);
		if (_id == 0) {
			m_trackentry->state.selectedAutomationCtr = NULL;
		} else {
			_id--;
			if (_id >= 0 && _id < (int)targets.size()) {
				auto* atDevice = targets[_id];
				int32_t numParams = atDevice->getNumParameters();
				m_trackentry->state.selectedAutomationCtr = atDevice;
				m_trackentry->state.selectedAutomationParam = numParams?0:-1;
			}
		}
		DawInstance::get()->updateVisibleTrackContents();
		closeContextMenu();
	}
};
class guidropdown_popup_sel_automation_param : public guictxtmenu {
	track_gui_entry_t* const m_trackentry;
public:
	guidropdown_popup_sel_automation_param(track_gui_entry_t* const trackentry) : m_trackentry(trackentry) {
		this->size.x = 120;
		this->fontSize = FONT_SIZE_CTXT_SMALL;
		this->paddingV = 0;
		automatable_t* autom = m_trackentry->state.selectedAutomationCtr;
		addEntry(new ctxtmenu_entry("None", 0));
		if (autom) {
			std::vector<automatable_param_t*> sortedParams;
			autom->getSortedParams(sortedParams);
		    std::for_each(sortedParams.begin(), sortedParams.end(), [this](automatable_param_t* param) {
				addEntry(new ctxtmenu_entry(param->label, 1+param->idx));
		    });
		}
	}
	void clickedElement(ctxtmenu_entry* e, int _id) override {
		m_trackentry->state.selectedAutomationParam = -1;
		if (_id > 0) {
			automatable_t* autom = m_trackentry->state.selectedAutomationCtr;
			if (autom) {
				const int32_t paramIdx = _id - 1;
				dbgassert(autom->getParam(paramIdx));
				m_trackentry->state.selectedAutomationParam = paramIdx;
			}
		}
		DawInstance::get()->updateVisibleTrackContents();
		closeContextMenu();
	}
};
class guidropdown_automation_device : public guidropdownbase {
	track_gui_entry_t* const m_trackentry;
public:
	guidropdown_automation_device(track_gui_entry_t* const trackentry) :
		guidropdownbase(), m_trackentry(trackentry) {
	}
	String getString() {
		automatable_t* automatable = m_trackentry->state.selectedAutomationCtr;
		return !automatable ? "None" : automatable->getAutomatableName();
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		guictxtmenu_base *popup =new guidropdown_popup_sel_automation_device(m_trackentry);
		popup->size.x = 250;
		m_trackentry->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
	}
};
class guidropdown_automation_param : public guidropdownbase {
	track_gui_entry_t* const m_trackentry;
public:
	guidropdown_automation_param(track_gui_entry_t* const trackentry) :
		guidropdownbase(), m_trackentry(trackentry) {
	}
	String getString() {
		automatable_t* automatable = m_trackentry->state.selectedAutomationCtr;
		int32_t paramIdx = m_trackentry->state.selectedAutomationParam;
		return !automatable || paramIdx < 0 ? "None" : automatable->getParamName(paramIdx);
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		guictxtmenu_base *popup =new guidropdown_popup_sel_automation_param(m_trackentry);
		popup->size.x = 250;
		m_trackentry->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
	}
};
class gui_trackcontrols_title : public guictr_base {
	track_t* const m_track;
	track_gui_entry_t* const m_trackentry;
	guidropdown_automation_device automationSelectDevice;
	guidropdown_automation_param automationSelectParam;
	guibuttontoggle hideTrack;
	guibuttontoggle hideAutomation;
	guibuttontoggle addAutomationLane;
	int dragMode = -1;
public:
	gui_trackcontrols_title(track_gui_entry_t* _entry)
      :	guictr_base(), m_track(_entry->track), m_trackentry(_entry), automationSelectDevice(_entry),
		automationSelectParam(_entry) {
		setCanMouseHit(true);
		hideTrack.setRadius(12);
		hideAutomation.setRadius(10);
		addAutomationLane.setRadius(10);

		hideTrack.setStateRef(&_entry->layout.hideTrack);
		hideAutomation.setStateRef(&_entry->layout.hideSubtracks);
		padding = 0;
		hideTrack.getIcon = [e=_entry]{return e->layout.hideTrack?ICON_ARR_RIGHT:ICON_ARR_DOWN;};
		hideAutomation.getIcon = [e=_entry]{return e->layout.hideSubtracks?ICON_ARR_RIGHT:ICON_ARR_DOWN;};
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
		const int32_t CONST_LAYOUT_MARGIN = math::min(6, theme->get(GuiConstant::CONST_LAYOUT_MARGIN));
		const int titleHeight = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		//TODO: this is not optimal!
		removeUNCHECKED(&automationSelectParam);
		removeUNCHECKED(&automationSelectDevice);
		removeUNCHECKED(&hideAutomation);
		const int buttonRadius = (TRACK_HEIGHT_STEP-INSET_TRACK_CONTENT*2)/2;
		hideTrack.setRadius(buttonRadius);
		hideAutomation.setRadius(buttonRadius-2);
		addAutomationLane.setRadius(buttonRadius-2);
		int32_t inset = CONST_LAYOUT_MARGIN;
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
	void handleDraggedBegin(MouseEvent& evt) override {
		DawInstance::get()->setSelectedTrack(m_track);
		if (isResize(evt.relMousepos+this->pos)) {
			dragMode = DRAG_RESIZE;
		}
	}

	void handleDraggedMove(MouseEvent& evt) override {
		if (dragMode == DRAG_RESIZE) {
			int32_t mouseDragDist = evt.relMousepos.y;
			int32_t heightStep = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
			resize<track_gui_entry_t, TRACK_MIN_HEIGHT, TRACK_MAX_HEIGHT>(m_trackentry, m_trackentry, mouseDragDist, heightStep);
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
			m_trackentry->layout.hideTrack = !m_trackentry->layout.hideTrack;
			DawInstance::get()->updateVisibleTrackContents();
		}
		if (button == &hideAutomation) {
			m_trackentry->layout.hideSubtracks = !m_trackentry->layout.hideSubtracks;
		}
		if (button == &addAutomationLane) {
			m_trackentry->layout.hideTrack = false;
			m_trackentry->layout.hideSubtracks = false;
			DawInstance::get()->updateVisibleTrackContents();
		}
		updateStoreLoadSubtracks(m_trackentry->parent, m_trackentry);
		if (button == &addAutomationLane) {
			automatable_t* autom = m_trackentry->state.selectedAutomationCtr;
			int32_t param = m_trackentry->state.selectedAutomationParam;
			if (autom && param > -1) {
				m_trackentry->parent->addAutomationLane(m_trackentry, autom, param, true);
			}
		}
		m_trackentry->parent->relayout();
	}
	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		NVGcolor color = rgbToNvg(m_track->rgb);
		ivec2 titleSize(size.x, size.y);
		DawInstance* daw = DawInstance::get();
		const int titleHeight = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		const int rectHeight = math::min(titleHeight, size.y);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, titleSize.x, rectHeight);
		nvgFillColor(vg, color);
		nvgFill(vg);
//		if (ctrl->getSelectedTrack() == m_track) {
//			color = theme->getColor(GuiColor::COL_BG_SELECTEDTRACK_TITLE);
//			int posX = hideTrack.right() + INSET_TITLE;
//			nvgBeginPath(vg);
//			nvgRect(vg, posX, 0, titleSize.x-posX, rectHeight);
//			nvgFillColor(vg, color);
//			nvgFill(vg);
//		}
		if (daw->getSelectedTrack() == m_track) {
			NVGcolor color2 = theme->getColor(GuiColor::COL_BG_SELECTEDTRACK_TITLE);
			int right = hideTrack.right() + (hideTrack.pos.x)/*inset*/;
			nvgBeginPath(vg);
			nvgRect(vg, 0, 0, right, rectHeight);
			nvgFillColor(vg, color2);
			nvgFill(vg);
		}
		setFont(vg, (int) (titleHeight * 0.9), getContrastFontColorNvg(color), G_TITLE_ALIGN);
		auto textWidth = titleSize.x-hideTrack.right();
		nvgText(vg, hideTrack.right() + INSET_TITLE*2, 0 + titleHeight / 2, StringAsCStr(m_track->name), nullptr);

		for (auto g : guis) {
			g->render(vg);
		}
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
		target->trackEntryDragMove(this->m_trackentry->content, toControlsObjectSpace(mousepos, target));
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
		target->trackEntryDragRelease(this->m_trackentry->content, toControlsObjectSpace(mousepos, target));
	}
	void handleRightClick(MouseEvent& evt) {
		parent->handleRightClick(evt);
	}
	guictxtmenu_base* getTooltip(AppCtrl* appctrl) {
		if (this->m_track->audio) {
			auto tooltip = new guitooltip<audio_info_t>(new audio_info_t{m_track->name, m_track->audio});
			return tooltip;
		}
		return nullptr;
	}
};

class gui_track_subtrack_mixer : public guictr_base {
	track_t* const m_track;
	track_gui_entry_t* const m_trackentry;
public:
	gui_track_subtrack* const subtrack;
private:
	guibuttontoggle removeLane;
	int dragMode = -1;
public:
	gui_track_subtrack_mixer(track_gui_entry_t* _entry, gui_track_subtrack* _subtrack) :
		guictr_base(), m_track(_entry->track), m_trackentry(_entry), subtrack(_subtrack) {
		removeLane.setRadius(10);
		padding = 0;
		removeLane.icon = ICON_MINUS;
		add(&removeLane);
	}
	~gui_track_subtrack_mixer() {
		remove(&removeLane);
	}
	bool isStaticContainer() {
		return false;
	}
	void layout() {
		const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		const int buttonRadius = (TRACK_HEIGHT_STEP-INSET_TRACK_CONTENT*2)/2;
		removeLane.setRadius(buttonRadius-2);
		int32_t insetBtn2 = (TRACK_HEIGHT_STEP-removeLane.size.y)/2;
		removeLane.pos = ivec2(size.x-removeLane.size.x-insetBtn2, insetBtn2);
	}
	void buttonClicked(guibase* button) override {
		if (button == &removeLane) {
			DAW::Cursor& cursor = m_trackentry->parentCtrl->getCursor();
			int32_t laneIdx = this->subtrack->idx;
			if (cursor.inSubTrack(m_trackentry->idx, laneIdx)) {
				fixCursorSubRange(cursor, m_trackentry->subtracks.size()-1);
			}
			MainCtrl::getGuiTrackCtr()->removeSubtrack(m_trackentry, subtrack);
			DawInstance::get()->layoutTrackEditors();
			DawInstance::get()->updateVisibleTrackContents();
		}
	}
	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}

		for (auto g : guis) {
			g->render(vg);
		}

		subtrack->renderMixerInfo(vg);
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
		DawInstance::get()->setSelectedTrack(m_track);
		if (isResize(evt.relMousepos+this->pos)) {
			dragMode = DRAG_RESIZE;
		}
	}

	void handleDraggedMove(MouseEvent& evt) {
		if (dragMode == DRAG_RESIZE) {
			int32_t mouseDragDist = evt.relMousepos.y;
			int32_t heightStep = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
//			resize(m_trackentry, subtrack, mouseDragDist, heightStep);
			this->parent->onChildLayoutChanged(this);
		}
	}
	void handleRightClick(MouseEvent& evt) override {
		if (subtrack->at) {
			parentCtrl->openContextMenu(new guictxtmenu_at_param(subtrack->at, subtrack->param), evt.mousepos);
		}

	}
};
gui_track_controls::gui_track_controls(track_gui_entry_t* _entry)
	: guictr_base(),
	  m_track(_entry->track),
	  m_trackentry(_entry),
	  title(new gui_trackcontrols_title(_entry)),
	  mixer(new gui_trackcontrols_mixer(_entry)),
	  io(new gui_trackcontrols_io(_entry)) {
	add(title);
	add(mixer);
	add(io);
	padding = 0;
}
gui_track_controls::~gui_track_controls() {
	for (gui_track_subtrack_mixer* ctrl : automationLaneControls) {
		remove(ctrl);
		delete ctrl;
	}
	remove(io);
	remove(mixer);
	remove(title);
	delete mixer;;
	delete io;
	delete title;
}
void gui_track_controls::addSubtrackMixer(track_gui_entry_t* entry, gui_track_subtrack* al) {
	gui_track_subtrack_mixer* al_ctrl = new gui_track_subtrack_mixer(entry, al);
	automationLaneControls.push_back(al_ctrl);
	add(al_ctrl);
}
void gui_track_controls::removeSubtrackMixer(gui_track_subtrack* al) {
	auto& ctrls = automationLaneControls;
	auto it = std::find_if(ctrls.begin(), ctrls.end(), [al] (const gui_track_subtrack_mixer* ref) {
		return ref->subtrack == al;
	});
	dbgassert(it != ctrls.end());
	remove(*it);
	delete (*it);
	ctrls.erase(it);
}
void gui_track_controls::removeAllAutomationLanes(automatable_t* at, int32_t paramIdx) {
	auto& ctrls = automationLaneControls;
	auto it = std::remove_if(ctrls.begin(), ctrls.end(), [this, at, paramIdx] (gui_track_subtrack_mixer* ref) {
		if ((at == NULL || ref->subtrack->at == at) && (paramIdx < 0 || ref->subtrack->param == paramIdx)) {
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
void gui_track_controls::removeAllSubtracks() {
	for (auto at : automationLaneControls) {
		remove(at);
		delete at;
	}
	automationLaneControls.clear();
}
void gui_track_controls::renderGroupHandle(NVGcontext* vg) { //TODO: make const, have fun
	auto lvl = m_track->getChildLvl();
	auto p = m_track->parent;
	while (p) {
		dbgassert(lvl);

		NVGcolor color = rgbToNvg(p->rgb);
	//	ivec2 titleSize(size.x, size.y);
	//	MainCtrl* ctrl = MainCtrl::get();
	//	const int titleHeight = theme->get(GuiConstant::CONST_TRACK_HEIGHT_TITLE);
	//	const int rectHeight = math::min(titleHeight, size.y);

		ivec2 inset{2, 0};
		int32_t width = 8*lvl;

		nvgBeginPath(vg);
		nvgRect(vg, lvl*8-8+inset.x, pos.y+inset.y, 8-inset.x*2, size.y-inset.y*2);
		nvgFillColor(vg, color);
		nvgFill(vg);
		p = p->parent;
		lvl--;
	}

}
void gui_track_controls::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	auto bgColor = theme->getColor(GuiColor::COL_BG_BRT);
	DawInstance* daw = DawInstance::get();
	if (daw->getSelectedTrack() == m_track) {
		bgColor = theme->getColor(GuiColor::COL_BG_SELECTEDTRACK);
	}
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, size.x, size.y);
	nvgFillColor(vg, bgColor);
	nvgFill(vg);

	for (guibase* g : guis) {
		//content
		nvgSave(vg);
		g->render(vg);
		nvgRestore(vg);
	}
	nvgBeginPath(vg);
	nvgMoveTo(vg, title->right(), 0);
	nvgLineTo(vg, title->right(), size.y);
	if (io->isVisible()) {
		nvgMoveTo(vg, io->right(), 0);
		nvgLineTo(vg, io->right(), size.y);
	}
	for (gui_track_subtrack_mixer* g : automationLaneControls) {
		nvgMoveTo(vg, g->left(), g->top()-TRACK_HEIGHT_SPACING_HALF);
		nvgLineTo(vg, g->right(), g->top()-TRACK_HEIGHT_SPACING_HALF);
	}
	nvgStrokeColor(vg, theme->getColor(GuiColor::COL_LINE_SEPERATOR));
	nvgStrokeWidth(vg, 1);
	nvgStroke(vg);

}
bool canResizeTitleBar(const track_gui_entry_t* const m_trackentry) {
	return !m_trackentry->layout.hideTrack&&!m_trackentry->layout.hideSubtracks&&m_trackentry->subtracks.size();
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
			} else if (canResizeTitleBar(m_trackentry) && title->isResize(local)) {
				g = title;
			}
		} else {
			if (canResizeTitleBar(m_trackentry) && title->isResize(local)) {
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
namespace GuiConstant {
GuiConstant::constant_t CONST_MIXER_WIDTH("CONST_MIXER_WIDTH", 160);
GuiConstant::constant_t CONST_TRACK_IO_WIDTH("CONST_TRACK_IO_WIDTH", 180);
}
guibase* gui_track_controls::getTitle() {
	return title;
}
void gui_track_controls::layout() {
	const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
	const int32_t TRACK_IO_WIDTH = theme->get(GuiConstant::CONST_TRACK_IO_WIDTH);
	const int32_t TRACK_MIXER_WIDTH = theme->get(GuiConstant::CONST_MIXER_WIDTH);
	int32_t titleW = size.x - TRACK_MIXER_WIDTH;
	if (io->isVisible()) {
		titleW -= TRACK_IO_WIDTH;
	}
	mixer->size = ivec2(TRACK_MIXER_WIDTH - TRACK_HEIGHT_SPACING, size.y);
	int32_t trH = m_trackentry->layout.hideTrack ? 1 : m_trackentry->layout.height;
	title->size = ivec2(titleW - TRACK_HEIGHT_SPACING, trH*TRACK_HEIGHT_STEP);
	title->pos = ivec2(TRACK_HEIGHT_SPACING_HALF, 0);
	mixer->pos = ivec2(size.x - TRACK_MIXER_WIDTH + TRACK_HEIGHT_SPACING_HALF, 0);

	io->size = ivec2(TRACK_IO_WIDTH - TRACK_HEIGHT_SPACING, size.y);
	io->pos = ivec2(size.x - TRACK_MIXER_WIDTH - TRACK_IO_WIDTH + TRACK_HEIGHT_SPACING_HALF, 0);
	for (gui_track_subtrack_mixer* ctrl : automationLaneControls) {
		ctrl->pos = ivec2(title->pos.x, ctrl->subtrack->pos.y-pos.y);
		ctrl->size = ivec2(title->size.x, ctrl->subtrack->size.y);
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
		int32_t totalHeightSteps = math::min(128, math::max(1, (mouseDragDist) / TRACK_HEIGHT_STEP));
		if (m_trackentry->layout.hideTrack && totalHeightSteps > TRACK_MIN_HEIGHT) {
			m_trackentry->layout.hideTrack = false;
			updateStoreLoadSubtracks(m_trackentry->parent, m_trackentry);
		}
		int nChanged = 0;
		while (totalHeightSteps < trackHeight(m_trackentry) && addTrHeight(m_trackentry, -1)) {
			nChanged++;
		}
		while (totalHeightSteps > trackHeight(m_trackentry) && addTrHeight(m_trackentry, 1)) {
			nChanged++;
		}
		if (!nChanged && m_trackentry->layout.height == TRACK_MIN_HEIGHT && totalHeightSteps == TRACK_MIN_HEIGHT) {
			m_trackentry->layout.hideTrack = true;
			updateStoreLoadSubtracks(m_trackentry->parent, m_trackentry);
		}
		//this->parent->onChildLayoutChanged(this);
		m_trackentry->parent->relayout();
		//m_trackentry->parent->updateVisibleTrackContents();
		//DawInstance::get()->updateVisibleTrackContents();
	}
}
String makeUniqueTrackName(const String& strNewName) {
	DawInstance* daw = DawInstance::get();
	auto& trackCtr = daw->getTracks();
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
	track_gui_entry_t* const m_trackentry;
public:
	ctxtmenu_color_select* sel;
	guictxtmenu_track(track_gui_entry_t* const trackentry) : guictxtmenu(), m_trackentry(trackentry) {
		this->size.x = 120;
		sel = new ctxtmenu_color_select("Pick Color", 100);
		addEntry(new ctxtmenu_entry("Duplicate track", 1));
		addEntry(new ctxtmenu_entry("Rename track", 6));
		addEntry(new ctxtmenu_splitter());
		addEntry(new ctxtmenu_entry("Show all automation", 0));
		addEntry(new ctxtmenu_entry("Show waveform", 5));
		addEntry(new ctxtmenu_entry("Add child MIDI Track", 4));
		addEntry(new ctxtmenu_splitter());
		addEntry(new ctxtmenu_entry("Save track", 3));
		addEntry(new ctxtmenu_entry("Delete track", 2));
		addEntry(new ctxtmenu_splitter());
		addEntry(sel);
		DawInstance::get()->setSelectedTrack(m_trackentry->track);
	}
	~guictxtmenu_track() {
	}
	void clicked(int _id) {
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		track_t* tr = m_trackentry->track;
		if (_id >= sel->id) {
			_id -= sel->id;
			if (tr) {
				int32_t col = colorPalette[_id];
				tr->rgb = col;
			}
		} else if (_id == 0) {
			gui_track_automationlane* gtr_at = NULL;
			if (tr) {
				m_trackentry->layout.hideTrack = false;
				m_trackentry->layout.hideSubtracks = false;
				updateStoreLoadSubtracks(m_trackentry->parent, m_trackentry);
				auto trCtr = m_trackentry->parent;
				std::vector<automatable_t*> targets;
				tr->audio->getAutomatableTrackTargets(targets);
				for (automatable_t* atl : targets) {
					std::vector<int32_t> automated;
					atl->getAutomated(automated);
					for (int32_t param : automated) {
						gtr_at = trCtr->addAutomationLane(m_trackentry, atl, param, true);
					}
				}
			}
			if (gtr_at) {
				m_trackentry->parent->layout();
				m_trackentry->parent->updateVisibleTrackContents();
				m_trackentry->parent->scrollTo(gtr_at);
			}
		} else if (_id == 1) {
			if (tr) {
				//TODO: generate unique stage ids and assign them to track audio stage and plugin instances
				track_t* newTrack = DawInstance::get()->createNewTrack(tr->type);
				String strNewName = StringFormat("%s copy", StringAsCStr(tr->name));

				track_snapshot_t trSnap(tr, true);
				trSnap.stageIds.inputStageId = -1;
				*newTrack = trSnap;
				DawInstance::get()->addTrackImpl(tr->localIdxFlat+1, newTrack, FLG_TRK_CHANGE_USER);
//				trSnap.stageId = static_cast<int32_t>(newTrack->audio->stageId);
				newTrack->loadSnapshot(trSnap);
				newTrack->name = makeUniqueTrackName(strNewName);
				//make stuff unique
				dbgassert(vsthost::getInstance()->validateIds());
				m_trackentry->parent->layout();
				DawInstance::get()->updateVisibleTrackContents();
				track_gui_entry_t* entry;
				if (m_trackentry->parent->getTrackEntry(tr, &entry)) {
					m_trackentry->parent->scrollTo(entry->content);
				}
			}
		} else if (_id == 2) {
			auto trackparent = m_trackentry->parent;
			DawInstance::get()->removeTrackId(m_trackentry->track->projectIdx);
			trackparent->layout();
			DawInstance::get()->updateVisibleTrackContents();
		} else if (_id == 3) {
			auto window = parentCtrl->window;

			track_snapshot_t snapshot(tr, true);
			trackcontainer_snapshot_t trackContainerSnapshot;
			trackContainerSnapshot.tracks.push_back(snapshot);
			// promptUserFilePath initiates a native dialog that would close this context menu
			// so we do it ourself controlled here
			closeContextMenu(); // deletes this
			// now we make sure not to access heap (this) after this point

			String path;
			if (promptUserFilePath(window, 1, vFILE_TYPES_TRACKSNAPSHOT, path)) {
				saveTrackContainer(trackContainerSnapshot, path);
			}
			return;
		} else if (_id == 4) {
			auto trackCtr = m_trackentry->parent;
			track_t* newTrack = DawInstance::get()->createNewTrack(tr->type);
			tr->addChild(newTrack);
			DawInstance::get()->addTrackImpl(0, newTrack, FLG_TRK_CHANGE_USER);
			newTrack->name = makeUniqueTrackName(tr->name);

			trackCtr->layout();
			DawInstance::get()->updateVisibleTrackContents();
			track_gui_entry_t* entry;
			if (trackCtr->getTrackEntry(newTrack, &entry)) {
				trackCtr->scrollTo(entry->content);
			}
			closeContextMenu(); // deletes this
			return;
		} else if (_id == 6) {
			const int titleHeight = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

			auto title = m_trackentry->mixer->getTitle();
			auto popupPos = title->toScreenSpace(ivec2(0));//+ivec2(title->hideTrack.right() + INSET_TITLE*2, 0);
			gui_textfield* field = new gui_textfield();
			field->size = title->size;
			field->size.y = titleHeight;
			field->pos = {0, 0};
			field->setFontSize((int)(titleHeight*0.9));
			field->mReturnCommits = true;

			guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
			ctxtMenu->size = field->size;
			ctxtMenu->add(field);
			ctxtMenu->layout();
			ctxtMenu->canTakeInputFocus = true;
			ctxtMenu->maxHeight = field->size.y;
			dbgassert(!ctxtMenu->isBackgroundRendered());
			ctxtMenu->setBackgroundRendered(false);
			auto cb = [trackEntry = m_trackentry, ctxtMenu](const std::string& str){
				trackEntry->track->name  = str;
				ctxtMenu->closeContextMenu();
				return true;
			};
			field->setEndEditCallback(cb);
			m_trackentry->parentCtrl->openContextMenu(ctxtMenu, popupPos);

			field->setValue(m_trackentry->track->name);
			field->setSelectionRange(-1, -1);
			field->parentCtrl->focusGui(field);
		} else if (_id == 5) {

			auto trackCtr = m_trackentry->parent;

			bool isShown = (tr->audio->flags & audiostageflags_t::CONVERT_OUTPUT) != audiostageflags_t::NONE;
			if (isShown) {
				tr->audio->flags &= ~(audiostageflags_t::CONVERT_OUTPUT | audiostageflags_t::WRITE_OUTPUT);
				std::vector<gui_track_subtrack*> subtracksVecCopy = m_trackentry->subtracks;
				for (auto subtrack : subtracksVecCopy) {
					if (subtrack->subtrackType() == gui_track_subtrack::SUBTRACK_TYPE_WAVE) {
						trackCtr->removeSubtrack(m_trackentry, subtrack);
					}
				}
			} else {
				tr->audio->flags |= audiostageflags_t::CONVERT_OUTPUT | audiostageflags_t::WRITE_OUTPUT;
				auto gui = makeGuiSubtrack(m_trackentry, MainCtrl::get(), gui_track_subtrack::SUBTRACK_TYPE_WAVE);
				trackCtr->addSubTrack(m_trackentry, gui, true);
			}

			trackCtr->layout();
			DawInstance::get()->updateVisibleTrackContents();

			closeContextMenu(); // deletes this
			return;
		}
		closeContextMenu();
	}
};
void gui_track_controls::handleRightClick(MouseEvent& evt) {
	m_trackentry->parentCtrl->openContextMenu(new guictxtmenu_track(this->m_trackentry), evt.mousepos);
}
gui_track_controls* createTrackGuiMixer(track_gui_entry_t* _entry) {
	gui_track_controls* const guicontrols = new gui_track_controls(_entry);
	guicontrols->setZOrder(_entry->track->type >= TRACK_TYPE_MIDI ? 0 : 1);
	return guicontrols;
}
