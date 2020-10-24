#include <glm/geometric.hpp>
#include <memory>
#include <numeric>
#include <vector>


#include "track.h"
#include "trackautomation.h"
#include "track_impl.h"

#include "event.h"
#include "button.h"
#include "dropdown.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "audiocache.h"
#include "drawwaveform.h"
#include "audiowaveform.h"
#include "samplerate.h"
#include "basectrl.h"
#include "host/mainctrl.h"
#include "host/vst_host.h"
#include "table.h"
#include "guitooltip.h"


#include "guicontextmenu_daw.h"
struct track_gui_entry_t;

gui_audio_clip::gui_audio_clip(track_gui_entry_t* _track, clip_t* _clip)
	: gui_clip(_track, _clip), waveformRef(new gui_waveform_texture_ref{}) {
};
gui_audio_clip::~gui_audio_clip() {
	delete waveformRef;
}

void gui_audio_clip::onRemove() {
	releaseRendered();
	dbgassert(STL_CONTAINS(m_clip->trackEntries, this->m_trackentry));
	removeEntry(this->m_clip->trackEntries, this->m_trackentry);
	auto it2 = m_trackentry->clipsGuis.find(m_clip);
	dbgassert(it2 != m_trackentry->clipsGuis.end());
	m_trackentry->clipsGuis.erase(it2);
}

void gui_midi_clip::onRemove() {
	dbgassert(STL_CONTAINS(m_clip->trackEntries, this->m_trackentry));
	removeEntry(this->m_clip->trackEntries, this->m_trackentry);
	auto it1 = m_trackentry->clipsGuis.find(m_clip);
	dbgassert(it1 != m_trackentry->clipsGuis.end());
	m_trackentry->clipsGuis.erase(it1);
}

void gui_audio_clip::render(NVGcontext* vg) {
	if (!culled) {
		ivec2 clipSize = ivec2(size.x, size.y - (HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT * 2));
		ivec2 posClipped = ivec2(pos.x, pos.y + (HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT * 2));
		ivec2 sizeClipped = clipSize;
		this->parent->scissorClip(posClipped, sizeClipped);
		sizeClipped.y = clipSize.y;
		renderAudioClip(vg, theme, m_track, m_clip, waveformRef, pos, size, posClipped, sizeClipped);
	}
}
void gui_midi_clip::handleRightClick(MouseEvent& evt) {
	parentCtrl->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
void gui_audio_clip::handleRightClick(MouseEvent& evt) {
	parentCtrl->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
void gui_audio_clip::releaseRendered() {
//	my_printf("release %012x from releaseRendered()\n", waveformRef);
	waveformrender::getInstance()->release(waveformRef);
//	m_clip->audio.waveformRef.fbId = -1;
	waveformRef->rendered = false;
}

bool isEqualWaveform2(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) {
	return (lhs.sampleBeginOffset - lhs.sampleBegin) == (rhs.sampleBeginOffset - rhs.sampleBegin) &&
			(lhs.sampleEnd - lhs.sampleBegin) == (rhs.sampleEnd - rhs.sampleBegin) &&
//			lhs.startOffset == rhs.startOffset &&
//			lhs.size == rhs.size &&
//			lhs.samplesPerPx == rhs.samplesPerPx &&
//			lhs.scale == rhs.scale &&
//			lhs.scaleX == rhs.scaleX &&
			lhs.audioId == rhs.audioId && lhs.quality == rhs.quality && lhs.method == rhs.method;
}
inline bool canReuse(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs){
	if (rhs.clipped)
		return true;
	if ((lhs.sampleBeginOffset - lhs.sampleBegin) != (rhs.sampleBeginOffset - rhs.sampleBegin)) {
		return false;
	}
	if ((lhs.sampleEnd - lhs.sampleBegin) != (rhs.sampleEnd - rhs.sampleBegin)) {
		return false;
	}
//			(lhs.sampleEnd - lhs.sampleBegin) == (rhs.sampleEnd - rhs.sampleBegin) &&
			return
//			lhs.startOffset == rhs.startOffset &&
//			lhs.size == rhs.size &&
//			lhs.samplesPerPx == rhs.samplesPerPx &&
//			lhs.scale == rhs.scale &&
			lhs.clipped == rhs.clipped &&
//			lhs.scaleX == rhs.scaleX &&
			lhs.audioId == rhs.audioId && lhs.quality == rhs.quality && lhs.method == rhs.method;


}
bool isEqualWaveform3(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) {
	if ((lhs.sampleBeginOffset - lhs.sampleBegin) != (rhs.sampleBeginOffset - rhs.sampleBegin)) {
		return false;
	}
	if ((lhs.sampleEnd - lhs.sampleBegin) != (rhs.sampleEnd - rhs.sampleBegin)) {
		return false;
	}
//			(lhs.sampleEnd - lhs.sampleBegin) == (rhs.sampleEnd - rhs.sampleBegin) &&
			return
//					lhs.startOffset == rhs.startOffset &&
//			lhs.size == rhs.size &&
//			lhs.samplesPerPx == rhs.samplesPerPx &&
//			lhs.scale == rhs.scale &&
			lhs.clipped == rhs.clipped &&
//			lhs.scaleX == rhs.scaleX &&
			lhs.audioId == rhs.audioId && lhs.quality == rhs.quality && lhs.method == rhs.method;
}
void gui_audio_clip::updateClipRenderCache(NVGcontext* vg) {

}
void gui_audio_clip::updatePosition(project_globals_t& project, scaled_grid& grid, ivec2& trackSize) {
	size = this->parent->size;
	culled = !getClipPosition(grid, trackSize, m_clip, pos, size, 0);
	audiofile_t* audio = audiocache::getInstance()->get(m_clip->audio.id);
	if (culled || !audio) {
//		my_printf("release %012x from updatePosition() (culled)\n", waveformRef);
		releaseRendered();
	}
//test clipping
//	ivec2 prevSize = size;
//	ivec4 clippedP = ivec4(pos, size);
//	this->parent->scissorClip(clippedP);
//	pos.x = clippedP.x;
//	pos.y = clippedP.y;
//	size.x = clippedP.z;
//	size.y = clippedP.w;
//	if (size != prevSize) {
//		my_printf("%d %d -> %d %d\n", prevSize.x, prevSize.y, size.x, size.y);
//	}
	if (!culled) {
		dbgassert(size.x > 0);
		if (audio) {
			ivec2 clipSize = ivec2(size.x, size.y-(HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT*2));
			ivec2 posClipped = pos;
			ivec2 sizeClipped = clipSize;
			this->parent->scissorClip(posClipped, sizeClipped);
			sizeClipped.y = clipSize.y;
			if (posClipped.x+sizeClipped.x <= 0 || sizeClipped.x <= 0) {
				culled = true;
			} else {
				auto waveform = makeWaveformFromClip(project, grid, trackSize, m_clip, pos, clipSize, posClipped, sizeClipped);
				if (waveform.size.x < 1 || waveform.size.y < 1) {
					releaseRendered();
					waveformRef->waveform = waveform;
					this->updatedWaveform = waveform;
				} else {
					bool equal = ((waveform.size.y > 0) == (waveformRef->waveform.size.y > 0)) && isEqualWaveform3(waveform, waveformRef->waveform);

					bool canQueue = waveformrender::getInstance()->canQueueUpdate();
					ivec2 sizeDiff = math::absvec2(waveform.size-waveformRef->waveform.size);
					ivec2 limit = math::maxvec2(ivec2(1), ivec2(waveform.size.x/4, 16));
					if (!canQueue) {
						limit.x = waveform.size.x/4;
					}
					if (waveform.clipped || (MainCtrl::get() && !MainCtrl::get()->isZooming())) {
						limit = {0,0};
					}
					if (!equal || (sizeDiff.x > limit.x || sizeDiff.y > limit.y)) {
//						if (!equal)
//							my_printf("unequal\n",0);
//						else {
//							my_printf("sizeDiff %d,%d / %d,%d (canQueue %d)\n",sizeDiff.x,sizeDiff.y,limit.x,limit.y, canQueue);
//						}
						this->updatedWaveform = waveform;
						if (sizeDiff.x > limit.x || sizeDiff.y > limit.y) {
							releaseRendered();
						}
					}
				}
			}
		}
	}
}

void gui_track::prerender(NVGcontext* vg) {
	nvgBeginFrame(vg, 1024, 1024, 1.0);
	nvgScale(vg, parentCtrl->m_scale, parentCtrl->m_scale);
	nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);
	nvgCachePath(vg, 1);
	for (auto& entry : m_trackentry->clipsGuis) {
		if(entry.second) {
			entry.second->updateClipRenderCache(vg);
		}
	}
	nvgCachePath(vg, 0);
	nvgEndFrame(vg);
	for (guibase* gui : guis) {
		gui->prerender(vg);
	}
}
void gui_audio_clip::prerender(NVGcontext* vg) {
	auto& clipAudio = m_clip->audio;
	audiofile_t* audio = audiocache::getInstance()->get(clipAudio.id);
	if (!waveformRef->queued) {
		if (!audio || this->updatedWaveform.size.x < 1 || this->updatedWaveform.size.y < 1) {
			return;
		}
		if (!culled && (!waveformRef->rendered || (this->updatedWaveform != waveformRef->waveform))) {
			releaseRendered();
			waveformRef->waveform = this->updatedWaveform;
			dbgassert(!waveformRef->queued);
			dbgassert(waveformRef->waveform.size.x > 0 && waveformRef->waveform.size.y > 0);
			waveformrender::getInstance()->queueUpdate(audio, waveformRef);
		}
	}
}
using Table::tbl;
using Table::tbl_row_t;
using Table::table_entry_t;
using Table::tblint;
using Table::tblfloat;
using Table::tblstr;
using Table::tblString;
template <>
void guitooltip<clip_t>::layout()  {
	size.x = 400;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
	using tbl_rows = std::vector<table_entry_t>;
	{
		audiofile_t* c = audiocache::getInstance()->get(ptr->audio.id);

		String path;
		if (c) {
			path = StringFormat("%s.%s", StringAsCStr(c->name), StringAsCStr(c->ext));
		} else {
			path = StringFormat("<MISSING SAMPLE %d>", ptr->audio.id);
		}
		tbl_rows vec{ tblString{StringFormat("Audio Clip (sample-id %d)", ptr->audio.id)}, tblString{path}};
		table.rows.push_back(tbl_row_t{vec});
	}
	{
		tbl_rows vec{tblstr{"num samples"}, tblint{ptr->getLenSamples()}};
		table.rows.push_back(tbl_row_t{vec});
	}
	{
		table.rows.push_back(tbl_row_t{ tbl_rows{{tblstr{"ticks start"}, tblint{ptr->start()}}}});
		table.rows.push_back(tbl_row_t{ tbl_rows{{tblstr{"ticks end"}, tblint{ptr->end()}}} });
		table.rows.push_back(tbl_row_t{ tbl_rows{{tblstr{"ticks length"}, tblint{ptr->getLen()}}} });
		table.rows.push_back(tbl_row_t{ tbl_rows{{tblstr{"color"}, tblint{ptr->rgb, "%08x"}}} });
	}
#ifdef TODO_PROPERTIES_TABLE_CLIP_WAVEFORM_PROPERTIES
	{
		audioclip_texture_t waveform = ptr->audio.waveformRef.waveform;
		gui_waveform_texture_ref& waveformRef = ptr->audio.waveformRef;

//		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform", FONT_SIZE_TOOLTIP_BIG}, tblint{waveform.quality}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform samplesPerPx"}, tblfloat{(float)waveform.samplesPerPx}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform pos"}, waveform.pos}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform size"}, waveform.size}}});
//		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform startOffset"}, waveform.startOffset}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform clipped"}, tblstr{(waveform.clipped?"yes":"no")}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform quality"}, tblint{waveform.quality}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform scaleX"}, tblfloat{waveform.scaleX}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveformRef atlasId"}, tblint{waveformRef.atlasId}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveformRef atlasEntryId"}, tblint{waveformRef.atlasEntryId}}}});
	}
#endif
	Table::AdjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* gui_audio_clip::getTooltip(AppCtrl* appctrl) {
	auto tooltip = new guitooltip<clip_t>(this->m_clip);
	return tooltip;
}
void gui_audio_clip::onIdle() {
//	cachedaudio_t* audio = audiocache::getInstance()->get(m_clip->audio.id);
//	if (!audio) {
//		return;
//	}
}
void gui_audio_clip::onTick(AppCtrl* appctrl) {

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

gui_track::gui_track(track_gui_entry_t* _entry, scaled_grid& _grid)
  : guictr_base(), m_track(_entry->track), m_trackentry(_entry), automation(_entry, _grid, _entry->state.selectedAutomationCtr, _entry->state.selectedAutomationParam, subtrackIdx)
{
	padding = 0;
}

gui_track* createTrackGui(track_gui_entry_t* _entry, scaled_grid& grid) {
	track_t* const t = _entry->track;
	gui_track * const guitrack = new gui_track(_entry, grid);
	guitrack->setZOrder(TRACKTYPE_TO_CTR(t->type) == TRACK_CTR_MIDIAUDIO ? 0 : 1);
	return guitrack;
}

gui_clip* createClipGui(guictr_base* parent, track_gui_entry_t* trackentry, clip_t* clip) {
	if (0 == trackentry->clipsGuis.count(clip)) {
		if (clip->clipType == CLIP_MIDI) {
			trackentry->clipsGuis[clip] = new gui_midi_clip(trackentry, clip);
		} else {
			trackentry->clipsGuis[clip] = new gui_audio_clip(trackentry, clip);
		}
		clip->trackEntries.push_back(trackentry);
	}
	return trackentry->clipsGuis[clip];
}
void gui_track::updateVisibleTrackContents(project_globals_t& project, scaled_grid& grid) {
	automation.setData();
	automation.updateVisibleTrackContents(grid);
	std::vector<clip_t*> clips = m_track->getMidi().getClips();
	for (clip_t* clip : clips) {
		auto* gui = createClipGui(this, m_trackentry, clip);
		dbgassert(gui);
		if (gui->parent != this) {
			add(gui);
		}
		gui->updatePosition(project, grid, size);
	}
	for (gui_track_subtrack* gui : m_trackentry->subtracks) {
		const bool throttleRefresh = m_trackentry->parentCtrl->isZooming();
		gui->updatePosition(project, grid, size, throttleRefresh);
	}
}
bool gui_track::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (automation.mouseHitTest(mpos, evt)) {
		return true;
	}
	if (this->contains(mpos)) {
		ivec2 localMouse = this->toContainerSpace(mpos);
		for (guibase* gui : guis) {
			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		if (evt.type == MouseHitType::MOUSE_RIGHT) { // righclick in selection (create clip etc.)
			scaled_grid& grid = m_trackentry->parentCtrl->getGrid();
			tick_t tick = grid.screenToTickSnap(mpos.x, SNAP_OFF);
			if (m_trackentry->parentCtrl->getCursor().contains(this->m_trackentry->idx, tick)) {
				evt.requestFocus(this);
				return true;
			}
		}
		// tracks need to always cancel further mouse tests for z-order to work in parent container
		return true;
	}
	return false;
}

void gui_track_subtrack::updateVisibleTrackContents(scaled_grid& grid) {
	automation.setData();
	automation.updateVisibleTrackContents(grid);
}
gui_track_automationlane::gui_track_automationlane(track_gui_entry_t* _entry, scaled_grid& _grid, automatable_t* _at, int32_t _param)
  : gui_track_subtrack(_entry, _grid, _at, _param)
{
}

gui_track_subtrack::gui_track_subtrack(track_gui_entry_t* _entry, scaled_grid& _grid, automatable_t* _at, int32_t _param)
  : guictr_base(), m_track(_entry->track), m_trackentry(_entry), automation(_entry, _grid, this->at, param, idx), at(_at), param(_param)
{
	padding = 0;
}

class guictxtmenu_trackcontent : public guictxtmenu {
	DawCtrl* const m_dawCtrl;
	track_gui_entry_t* const m_trackentry;
public:

	//TODO make this take a safe reference to a track
	guictxtmenu_trackcontent(DawCtrl* const _dawCtrl, track_gui_entry_t* const _trackentry)
	  : m_dawCtrl(_dawCtrl), m_trackentry(_trackentry) {
		this->size.x = 320;
		if (_trackentry) {

			auto newClip = new ctxtmenu_entry("Create empty clip", 20);
			addEntry(newClip);
			auto newClip2 = new ctxtmenu_entry("Consolidate selection", 21);
			addEntry(newClip2);
			addEntry(new ctxtmenu_splitter());
		}
		scaled_grid& grid = _dawCtrl->getGrid();
		auto adaptive = new ctxtmenu_time_select(grid, "Adaptive Grid", 0);
		adaptive->initAdaptive();
		addEntry(adaptive);
		auto fixed = new ctxtmenu_time_select(grid, "Fixed Grid", 0);
		fixed->initFixed();
		addEntry(fixed);
	}
	void clicked(int _id) {
		scaled_grid& grid = m_dawCtrl->getGrid();
		if (_id == 20) {
			dbgassert(m_trackentry);
			DAW::Cursor cursor = m_dawCtrl->getCursor().getLeftAligned();
			if (cursor.selRange) {
				track_t* tr = m_trackentry->track;
				clip_t* cl = nullptr;
				if (tr && tr->type == TRACK_TYPE_MIDI) {
					cl = new clip_t();
					cl->clipType = CLIP_MIDI;
				}
				if (tr && tr->type == TRACK_TYPE_AUDIO) {
					cl = new clip_t();
					cl->clipType = CLIP_AUDIO;
				}
				if (cl) {
					cl->name = StringFormat("%s Clip", StringAsCStr(tr->name));
					cl->time = cursor.cursorPos;
					cl->setLen(cursor.selRange);
					cl->loopStart = 0;
					cl->loopLen = cl->getLen();
					tr->getMidi().addClipSort(cl);
				}
			}
		}
		else if (_id == 21) {

		} else if (_id == 110+9) { // OFF
			grid.grid_dens.enabled = false;
		} else if (_id >= 110) {
			grid.grid_dens.enabled = true;
			grid.grid_dens.fixedBars = _id - 110;
			grid.grid_dens.isfixed = true;
		} else {
			grid.grid_dens.enabled = true;
			grid.grid_dens.dynamicDensity = _id - 100;
			grid.grid_dens.isfixed = false;
		}
		grid.notifyChange();
//		ctrl->updateVisibleTrackContents();
//		MainCtrl::get()->updateGrid();
		closeContextMenu();
	}
};
void gui_track_automationlane::handleRightClick(MouseEvent& evt) {
	parentCtrl->openContextMenu(new guictxtmenu_trackcontent(m_trackentry->parentCtrl, m_trackentry), evt.mousepos);
}
void gui_track_subtrack::handleRightClick(MouseEvent& evt) {
	parentCtrl->openContextMenu(new guictxtmenu_trackcontent(m_trackentry->parentCtrl, m_trackentry), evt.mousepos);
}
void gui_track::handleRightClick(MouseEvent& evt) {
	parentCtrl->openContextMenu(new guictxtmenu_trackcontent(m_trackentry->parentCtrl, m_trackentry), evt.mousepos);
}
void guitrack_editor::handleRightClick(MouseEvent& evt) {
	parentCtrl->openContextMenu(new guictxtmenu_trackcontent(dawCtrl, nullptr), evt.mousepos);
}


void gui_track_subtrack::renderMixerInfo(NVGcontext* vg) {

	DAW::Cursor& cursor = m_trackentry->parentCtrl->getCursor();
	String curvalue = "UNDEF";
	String target = "<NULL>";
	automatable_t* ctr = at;
	if (ctr) {
		target = StringFormat("%s %08X", StringAsCStr(ctr->getAutomatableName()), ctr);
		int32_t idx = param;
		if (idx >= 0) {
			automation_t* automation = ctr->getRegisteredAutomation(idx);
			if (automation) {
				curvalue = StringFormat("%s (%d) %f", StringAsCStr(ctr->getParamName(idx)), idx, automation->getValueAt(cursor.cursorPos));
			} else {
				curvalue = StringFormat("%s (%d) UNDEF", StringAsCStr(ctr->getParamName(idx)), idx);
			}
		} else {
			curvalue = StringFormat("<NULL> %d", idx);
		}
	}
	const int htt = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
	const int titleHeight = htt*4/5;
	const int fontSize = titleHeight-4;
	int32_t y = INSET_TITLE;
	//debug
	setFont(vg, fontSize, G_WHITE, G_TITLE_ALIGN);
	renderText(vg, 0 + INSET_TITLE, y+titleHeight / 2, size.x, StringAsCStr(target));
	y+=titleHeight;
	renderText(vg, 0 + INSET_TITLE, y+titleHeight / 2, size.x, StringAsCStr(curvalue));
}
