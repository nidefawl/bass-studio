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
		int32_t nHeight = math::min(TRACK_MAX_HEIGHT_SUB, math::max(TRACK_MIN_HEIGHT_SUB, t2->height+offset));
		changed = nHeight != t2->height;
		t2->height = nHeight;
	}
	if (offset < 0 && !changed) {
		int32_t nHeight = math::min(TRACK_MAX_HEIGHT_SUB, math::max(2, tr->height+offset));
		changed |= nHeight != tr->height;
		tr->height = nHeight;
	}
	return changed;
}
template<typename T, int minHeight=TRACK_MIN_HEIGHT_SUB, int maxHeight=TRACK_MAX_HEIGHT_SUB>
void resize(track_t* m_track, T* al, int32_t mouseDragDist, int32_t heightStep) {

	if (m_track->type < TRACK_TYPE_MIDI) {
		//resize content-lane on bottom-sticked tracks
		int32_t adjustedHeightSteps = math::min(128, math::max(1, (mouseDragDist) / heightStep));
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

		int32_t totalHeightSteps = math::min(maxHeight, math::max(minHeight, (mouseDragDist) / heightStep));
		al->height = totalHeightSteps;
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
		table.rows.push_back({{tblstr{"Latency"}, tblint{(int32_t)audio->getLatency()}}});
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
class gui_trackgain: public guibase {
	automatable_t* paramAutomatable = nullptr;
	int32_t paramIdx = -1;
public:
	gui_trackgain() : guibase() {
		setCanMouseHit(true);
	}
	void setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx) {
		this->paramAutomatable = _paramAutomatable;
		this->paramIdx = _paramIdx;
	}
	void handleRightClick(MouseEvent& evt) override {
		dbgassert(paramAutomatable && paramIdx > -1 && paramAutomatable->getParam(paramIdx));
		MainCtrl::get()->openContextMenu(new guictxtmenu_at_param(paramAutomatable, paramIdx), evt.mousepos);
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
			setFont(vg, 20, theme->getContrastColor(valColor), NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
			String strLvl = StringFormat("%.2f", dsp_util::dBFSClampInf6(gainDb));
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
			if (paramAutomatable && paramIdx > -1) {
				float fGain = dsp_util::linScaleToGain(paramAutomatable->getParamValue(paramIdx));
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
				paramAutomatable->deactivateAutomation(paramIdx);
				paramAutomatable->getParam(paramIdx)->value = dsp_util::gainToLinScale(fNew);
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
		MainCtrl::get()->openContextMenu(new guictxtmenu_at_param(&m_track->audio->mixer, PARAM_ENABLE), evt.mousepos);
	}
};

class guibutton_track_solo : public guibutton {
	track_t* const m_track;
public:
	guibutton_track_solo(track_t* _track) : guibutton(), m_track(_track) {
		setText("S");
	}
	NVGcolor getBackgroundColor(int stateflags) const override {
//		int fl = FLG_HAS_COLOR_BG|FLG_ENBL;
		int fl = FLG_ENBL;
		if ((stateflags&fl) == fl) {
			return theme->getColor(GuiColor::COL_BTN_SOLO_BG_ENABLED);
		}
//		return theme->getColor(GuiColor::COL_BTN_SOLO_BG_DISABLED);
		return theme->getBgColor(stateflags);
	}
	bool isEnabled() const override {
		return m_track->audio && static_cast<bool>(m_track->audio->flags & audiostageflags_t::SOLO);
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
		setFont(vg, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(title), NULL);
//					if (channel.idx > -1) {
//						auto* stream = audiohost::getInstance()->getStream(0);
//						if (stream) {
//
//							auto& allMeters = isInput ? stream->metersInput : stream->metersOutput;
//							int32_t nChannels = AudioIO::getNumChannelsTrackType(channel.type);
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
	const bool isInput;

	explicit ctxtmenu_entry_external_channel(int32_t _id, const AudioIO::io_cfg_channel& _channel, bool _isInput)
	: ctxtmenu_entry_endpoint(_id, _channel.name), channel(_channel), isInput(_isInput) {
	}
	explicit ctxtmenu_entry_external_channel(int32_t _id, String name, bool _isInput)
	: ctxtmenu_entry_endpoint(_id, name), channel(), isInput(_isInput) {
	}
	void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
		if (contains(ctxtSize, mouse)) {
			nvgBeginPath(vg);
			nvgRect(vg, 0, y, ctxtSize.x, height);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
			nvgFill(vg);
		}
		setFont(vg, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(title), NULL);
		if (channel.idx > -1) {
			auto* stream = audiohost::getInstance()->getStream(0);
			if (stream) {

				auto& allMeters = isInput ? stream->metersInput : stream->metersOutput;
				int32_t nChannels = AudioIO::getNumChannelsTrackType(channel.type);
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
		return DAW::ChannelAudioInput(channel.idx, channel.channelOffset, AudioIO::getTrackNameShort(channel.type, channel.idx, isInput), channel.type);
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
		setFont(vg, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
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
				return DAW::ChannelStage(trImpl, endpoint.isInput);

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
		setFont(vg, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
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
		if (!_dstStage.isInput) {
			addEntry(new ctxtmenu_entry_stage_channel(idx++, "Input", audio_channel_ref_t{_busStage, true}));
		} else {
			addEntry(new ctxtmenu_entry_stage_channel(idx++, "Output", audio_channel_ref_t{_busStage, false}));
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
		auto& list = stageEndpoint.isInput ? cfg.input : cfg.output;
		for (auto& channel : list) {
			addEntry(new ctxtmenu_entry_external_channel(idx, channel, _dstStage.isInput));
			idx++;
		}
	}
	guidropdown_select_bus_ctxt(audio_channel_ref_t _stageEndpoint, int lvl = 0)
		: busStage(AudioStageRefNULL()), stageEndpoint(_stageEndpoint) {
		int32_t idx = 0;
		String inputName = stageEndpoint.isInput ? "External input" : "External output";
		addEntry(new ctxtmenu_entry_stage_channel(idx++, "None", AudioChannelRefNULL()));
		addEntry(new ctxtmenu_entry_default_channel(idx++, "Default"));
		addEntry(new ctxtmenu_entry_bus_external(idx++, inputName, stageEndpoint));
//				auto& cfg = settings.iosettings.getChannelConfig(settings.iosettings.device_api);
//				auto& list = isInput ? cfg.input : cfg.output;
		project_controller_t* project = project_controller_t::get();
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
		if (stageEndpoint.isInput) {
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
	guidropdown_select_bus(track_t* const _track, const bool _isInput) : guidropdownbase(), track(_track), isInput(_isInput) {
	}
	String getString() {
		vsthost* const host = vsthost::getInstance();
		project_controller_t* const project = project_controller_t::get();

		track_impl_t* trImpl = track->audio;
		dbgassert(trImpl);
		if (!trImpl)
			return "<Invalid Track>";
		auto& channel = isInput ? trImpl->inputChannel : trImpl->outputChannel;
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
		return channel.name;
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		track_impl_t* trImpl = track->audio;
		dbgassert(trImpl);
		if (!trImpl)
			return;
		guictxtmenu_base *popup = new guidropdown_select_bus_ctxt(audio_channel_ref_t{trImpl->toRef(), isInput});
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
	gui_trackcontrols_io(track_t* _track) :
		guictr_base(),/* m_track(_track), */selectInput(_track, true), selectOutput(_track, false) {
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
	gui_trackmeter<16000,2> meter;
public:
	gui_trackgain gain;
	guibutton_trackbypass btnBypass;
	guibutton_track_solo btnSolo;
	guibutton btnActivate;
	guibutton btnShowSubtrack;
	std::vector<gui_trackgain*> sendGains;
	gui_trackcontrols_mixer(track_t* _track) :
		guictr_base(), m_track(_track), meter(&_track->audio->meter), btnBypass(_track), btnSolo(_track) {
		gain.setAutomationRef(&_track->audio->mixer, PARAM_TRACK_GAIN);
		padding = 0;
//		btnBypass.setTint(nvgToRGB(theme->getFrameColorOutline()));
		btnBypass.drawFn = drawTextureSymbol;
		btnBypass.drawParm = ICON_BYPASS;
		btnActivate.setButtonColor(GuiColor::COL_BTN_LOAD_DEF_PLUGINS);
		gain.setLabel("Gain Level");
		btnActivate.setLabel("Load plugins");
		btnShowSubtrack.setLabel("Add audio subtrack");
		add(&btnBypass);
		add(&btnSolo);
		add(&btnActivate);
		add(&gain);
		add(&meter);
		add(&btnShowSubtrack);
		if (m_track->type != TRACK_TYPE_MASTER && m_track->type != TRACK_TYPE_RETURN) {
			sendGains.resize(MAX_SEND_CHANNELS);
			for (int i = 0; i < MAX_SEND_CHANNELS; i++) {
				sendGains[i] = new gui_trackgain();
				sendGains[i]->setVisible(false);
				sendGains[i]->setAutomationRef(&_track->audio->mixer, PARAM_OFFSET_SEND + i);
				sendGains[i]->setLabel(_track->audio->mixer.getParamName(PARAM_OFFSET_SEND + i));
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
		remove(&btnShowSubtrack);
	}
	void buttonClicked(guibase* button) override {
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		if (&btnSolo == button) {
			m_track->audio->flags ^= audiostageflags_t::SOLO;
		}
		if (&btnBypass == button) {
			track_params_t& trackParams = m_track->audio->mixer;
			trackParams.deactivateAutomation(PARAM_ENABLE);
			trackParams.setParamValue(PARAM_ENABLE, trackParams.isEnabled() ? 0.0f : 1.0f, 0);
		}
		if (&btnShowSubtrack == button) {
			auto gui = makeGuiSubtrack(MainCtrl::get(), m_track, gui_track_subtrack::SUBTRACK_TYPE_WAVE);
			MainCtrl::getGuiTrackCtr()->addSubTrack(m_track, gui, true);
		}
		if (&btnActivate == button) {
			vsthost* host = vsthost::getInstance();
			std::vector<effectbase*> effects = m_track->audio->deferredEffects;
			for (auto eff : effects) {
				host->activateDeferred(eff);
			}
#ifndef NDEBUG
			log_printf("deferredEffects post activateDeferred on track %s: %d\n", m_track->szName, m_track->audio->deferredEffects.size());
#endif	
		}
	}
	void layout() {
		const int32_t CONST_LAYOUT_MARGIN = math::min(6, theme->get(GuiConstant::CONST_LAYOUT_MARGIN));
		const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		int32_t inset = CONST_LAYOUT_MARGIN;
		int32_t i2 = inset * 2;
		int32_t h = TRACK_HEIGHT_STEP-i2;

		int32_t mW = TRACK_HEIGHT_STEP;
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
		btnShowSubtrack.pos = {btnActivate.right()+inset*2, gain.bottom()+i2};
		btnShowSubtrack.setFontSize(h-2);
		btnShowSubtrack.size = { h, h };

		meter.size = ivec2(mW-i2, size.y-i2);
		meter.pos = ivec2(size.x - mW+inset, inset);
		if (sendGains.size()) {
			const int32_t HEIGHT_SEND_GAIN = h;
			const int32_t SEND_PER_ROW = 1;
			ivec2 sendPos = {inset, btnShowSubtrack.bottom()+i2 };
			auto project = project_controller_t::get();
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
		int n = m_track->audio->deferredEffects.size();
		btnActivate.setEnabled(n>0);
		btnActivate.setText(n>9?"9+":(StringFormat("%d", n)));
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
				auto* atDevice = targets[_id];
				int32_t numParams = atDevice->getNumParameters();
				m_track->audio->selectedAutomationCtr = atDevice;
				m_track->audio->selectedAutomationParam = numParams?0:-1;
			}
		}
		MainCtrl::get()->updateVisibleTrackContents();
		closeContextMenu();
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
		m_track->audio->selectedAutomationParam = -1;
		if (_id > 0) {
			automatable_t* autom = m_track->audio->selectedAutomationCtr;
			if (autom) {
				const int32_t paramIdx = _id - 1;
				dbgassert(autom->getParam(paramIdx));
				m_track->audio->selectedAutomationParam = paramIdx;
			}
		}
		MainCtrl::get()->updateVisibleTrackContents();
		closeContextMenu();
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
		automationSelectParam(_track) {
		setCanMouseHit(true);
		hideTrack.setRadius(12);
		hideAutomation.setRadius(10);
		addAutomationLane.setRadius(10);

		hideTrack.state = &m_track->hideTrack;
		hideAutomation.state = &m_track->hideSubtracks;
		padding = 0;
		hideTrack.getIcon = [this]{return m_track->hideTrack?ICON_ARR_RIGHT:ICON_ARR_DOWN;};
		hideAutomation.getIcon = [this]{return m_track->hideSubtracks?ICON_ARR_RIGHT:ICON_ARR_DOWN;};
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
		removeUNCHECKED(&automationSelectParam);
		removeUNCHECKED(&automationSelectDevice);
		removeUNCHECKED(&hideAutomation);

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
			MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
		}
		if (button == &hideAutomation) {
			m_track->hideSubtracks = !m_track->hideSubtracks;
		}
		if (button == &addAutomationLane) {
			m_track->hideTrack = false;
			m_track->hideSubtracks = false;
			MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
		}
		m_track->audio->updateStoreLoadSubtracks();
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
		if (ctrl->getSelectedTrack() == m_track) {
			NVGcolor color2 = theme->getColor(GuiColor::COL_BG_SELECTEDTRACK_TITLE);
			int right = hideTrack.right() + (hideTrack.pos.x)/*inset*/;
			nvgBeginPath(vg);
			nvgRect(vg, 0, 0, right, rectHeight);
			nvgFillColor(vg, color2);
			nvgFill(vg);
		}
		setFont(vg, (int) (titleHeight * 0.9), getContrastFontColorNvg(color), G_TITLE_ALIGN);
		renderText(vg, hideTrack.right() + INSET_TITLE*2, 0 + titleHeight / 2, titleSize.x-hideTrack.right(), StringAsCStr(m_track->name));

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
public:
	gui_track_subtrack* const subtrack;
private:
	guibuttontoggle removeLane;
	int dragMode = -1;
public:
	gui_track_subtrack_mixer(track_t* _track, gui_track_subtrack* _subtrack) :
		guictr_base(), m_track(_track), subtrack(_subtrack) {
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
		int32_t insetBtn2 = (TRACK_HEIGHT_STEP-removeLane.size.y)/2;
		removeLane.pos = ivec2(size.x-removeLane.size.x-insetBtn2, insetBtn2);
	}
	void buttonClicked(guibase* button) override {
		if (button == &removeLane) {
			DAW::Cursor& cursor = MainCtrl::get()->cursor;
			int32_t laneIdx = this->subtrack->idx;
			if (cursor.inSubTrack(m_track->idx, laneIdx)) {
				fixCursorSubRange(cursor, m_track->subtracks.size()-1);
			}
//			MainCtrl::getGuiTrackCtr()->removeSubtrack(this->al);
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
		MainCtrl::get()->setSelectedTrack(m_track);
		if (isResize(evt.relMousepos+this->pos)) {
			dragMode = DRAG_RESIZE;
		}
	}

	void handleDraggedMove(MouseEvent& evt) {
		if (dragMode == DRAG_RESIZE) {
			int32_t mouseDragDist = evt.relMousepos.y;
			int32_t heightStep = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
			resize(m_track, subtrack, mouseDragDist, heightStep);
			this->parent->onChildLayoutChanged(this);
		}
	}
	void handleRightClick(MouseEvent& evt) override {
		if (subtrack->at) {
			MainCtrl::get()->openContextMenu(new guictxtmenu_at_param(subtrack->at, subtrack->param), evt.mousepos);
		}

	}
};
gui_track_controls::gui_track_controls(track_t* _track)
	: guictr_base(),
	  m_track(_track),
	  title(new gui_trackcontrols_title(_track)),
	  mixer(new gui_trackcontrols_mixer(_track)),
	  io(new gui_trackcontrols_io(_track)) {
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
void gui_track_controls::addSubtrackMixer(track_t* t, gui_track_subtrack* al) {
	gui_track_subtrack_mixer* al_ctrl = new gui_track_subtrack_mixer(t, al);
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
bool canResizeTitleBar(track_t* tr) {
	return !tr->hideTrack&&!tr->hideSubtracks&&tr->subtracks.size();
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
namespace GuiConstant {
GuiConstant::constant_t CONST_MIXER_WIDTH("CONST_MIXER_WIDTH", 160);
GuiConstant::constant_t CONST_TRACK_IO_WIDTH("CONST_TRACK_IO_WIDTH", 180);
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
	int32_t trH = m_track->hideTrack ? 1 : m_track->height;
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
		if (m_track->hideTrack && totalHeightSteps > TRACK_MIN_HEIGHT) {
			m_track->hideTrack = false;
			m_track->audio->updateStoreLoadSubtracks();
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
			m_track->audio->updateStoreLoadSubtracks();
		}
		this->parent->onChildLayoutChanged(this);
		MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
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
		addEntry(new ctxtmenu_entry("Save track", 3));
		addEntry(new ctxtmenu_entry("Add child MIDI Track", 4));
		addEntry(new ctxtmenu_splitter());
		addEntry(sel);
		track_t* tr = MainCtrl::get()->getTrackId(trackid);
		MainCtrl::get()->setSelectedTrack(tr);
	}
	~guictxtmenu_track() {
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
				tr->hideSubtracks = false;
				tr->audio->updateStoreLoadSubtracks();
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
				track_t* newTrack = MainCtrl::get()->createNewTrack(tr->type);
				String strNewName = StringFormat("%s copy", StringAsCStr(tr->name));

				track_snapshot_t trSnap(tr, true);
				*newTrack = trSnap;
				MainCtrl::get()->addTrackImpl(tr->localIdxFlat+1, newTrack, FLG_TRK_CHANGE_USER);
				trSnap.stageId = static_cast<int32_t>(newTrack->audio->stageId);
				newTrack->loadSnapshot(trSnap);
				newTrack->name = makeUniqueTrackName(strNewName);

				MainCtrl::getGuiTrackCtr()->layout();
				MainCtrl::get()->updateVisibleTrackContents();
				MainCtrl::getGuiTrackCtr()->scrollTo(newTrack->content);
			}
		} else if (_id == 2) {
			MainCtrl::get()->removeTrackId(trackid);
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
			track_t* newTrack = MainCtrl::get()->createNewTrack(tr->type);
			tr->addChild(newTrack);
			MainCtrl::get()->addTrackImpl(0, newTrack, FLG_TRK_CHANGE_USER);
			newTrack->name = makeUniqueTrackName(tr->name);

			MainCtrl::getGuiTrackCtr()->layout();
			MainCtrl::get()->updateVisibleTrackContents();
			MainCtrl::getGuiTrackCtr()->scrollTo(newTrack->content);
			closeContextMenu(); // deletes this
			return;
		}
		closeContextMenu();
	}
};
void gui_track_controls::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_track(this->m_track->idx), evt.mousepos);
}
gui_track_controls* createTrackGuiMixer(track_t* t) {
	return new gui_track_controls(t);
}
