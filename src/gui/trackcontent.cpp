
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include <memory>
#include <numeric>


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
#include "../host/vst_host.h"
#include "table.h"
#include "guitooltip.h"

#include "leak_detect.h"

#include "guicontextmenu_daw.h"

using glm::vec2;
using glm::ivec2;

void gui_midi_clip::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
void gui_audio_clip::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
void gui_audio_clip::releaseRendered() {
//	my_printf("release %012x from releaseRendered()\n", &m_clip->audio.waveformRef);
	waveformrender::getInstance()->release(&m_clip->audio.waveformRef);
//	m_clip->audio.waveformRef.fbId = -1;
	m_clip->audio.waveformRef.rendered = false;
}

inline bool isEqualWaveform2(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs){
	return (lhs.sampleBeginOffset - lhs.sampleBegin) == (rhs.sampleBeginOffset - rhs.sampleBegin) &&
			(lhs.sampleEnd - lhs.sampleBegin) == (rhs.sampleEnd - rhs.sampleBegin) &&
			lhs.startOffset == rhs.startOffset &&
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
			return lhs.startOffset == rhs.startOffset &&
//			lhs.size == rhs.size &&
//			lhs.samplesPerPx == rhs.samplesPerPx &&
//			lhs.scale == rhs.scale &&
			lhs.clipped == rhs.clipped &&
//			lhs.scaleX == rhs.scaleX &&
			lhs.audioId == rhs.audioId && lhs.quality == rhs.quality && lhs.method == rhs.method;


}
inline bool isEqualWaveform3(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs){
	if ((lhs.sampleBeginOffset - lhs.sampleBegin) != (rhs.sampleBeginOffset - rhs.sampleBegin)) {
		return false;
	}
	if ((lhs.sampleEnd - lhs.sampleBegin) != (rhs.sampleEnd - rhs.sampleBegin)) {
		return false;
	}
//			(lhs.sampleEnd - lhs.sampleBegin) == (rhs.sampleEnd - rhs.sampleBegin) &&
			return lhs.startOffset == rhs.startOffset &&
//			lhs.size == rhs.size &&
//			lhs.samplesPerPx == rhs.samplesPerPx &&
//			lhs.scale == rhs.scale &&
			lhs.clipped == rhs.clipped &&
//			lhs.scaleX == rhs.scaleX &&
			lhs.audioId == rhs.audioId && lhs.quality == rhs.quality && lhs.method == rhs.method;
}
ivec2 maxvec2(const ivec2& a, const ivec2& b) {
	return {std::max(a.x, b.x), std::max(a.x, b.x)};
}
ivec2 absvec2(const ivec2 a) {
	return {std::abs(a.x), std::abs(a.y)};
}
void gui_audio_clip::updatePosition(project_t& project, scaled_grid& grid, ivec2& trackSize) {
	size = this->parent->size;
	culled = !getClipPosition(grid, trackSize, m_clip, pos, size, 0);
	cachedaudio_t* audio = audiocache::getInstance()->get(m_clip->audio.id);
	if (culled || !audio) {
//		my_printf("release %012x from updatePosition() (culled)\n", &m_clip->audio.waveformRef);
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
		assert(size.x > 0);
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
					m_clip->audio.waveformRef.waveform = waveform;
					this->updatedWaveform = waveform;
				} else {
					bool equal = ((waveform.size.y > 0) == (m_clip->audio.waveformRef.waveform.size.y > 0)) && isEqualWaveform3(waveform, m_clip->audio.waveformRef.waveform);

					bool canQueue = waveformrender::getInstance()->canQueueUpdate();
					ivec2 sizeDiff = absvec2(waveform.size-m_clip->audio.waveformRef.waveform.size);
					ivec2 limit = maxvec2(ivec2(1), ivec2(waveform.size.x/4, 16));
					if (!canQueue) {
						limit.x = waveform.size.x/4;
					}
					if (waveform.clipped || !MainCtrl::get()->isZooming()) {
						limit = {0,0};
					}
					if (!equal || (sizeDiff.x > limit.x || sizeDiff.y > limit.y)) {
						if (!equal)
							my_printf("unequal\n",0);
						else
							my_printf("sizeDiff %d / %d (canQueue %d)\n",sizeDiff,limit*limit, canQueue);
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
void gui_audio_clip::prerender(NVGcontext* vg) {
	cachedaudio_t* audio = audiocache::getInstance()->get(m_clip->audio.id);
	if (!m_clip->audio.waveformRef.queued) {
		if (!audio || this->updatedWaveform.size.x < 1 || this->updatedWaveform.size.y < 1) {
			return;
		}
		if (!culled && (!m_clip->audio.waveformRef.rendered || (this->updatedWaveform != m_clip->audio.waveformRef.waveform))) {
			if (!m_clip->audio.waveformRef.queued) {
				releaseRendered();
				m_clip->audio.waveformRef.waveform = this->updatedWaveform;
				assert(!m_clip->audio.waveformRef.queued);
				assert(m_clip->audio.waveformRef.waveform.size.x > 0 && m_clip->audio.waveformRef.waveform.size.y > 0);
				waveformrender::getInstance()->queueUpdate(audio, &m_clip->audio.waveformRef);
			}

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
		cachedaudio_t* c = audiocache::getInstance()->get(ptr->audio.id);

		String path;
		if (c) {
			path = StringFormat("%s.%s", StringAsCStr(c->name), StringAsCStr(c->ext));
		} else {
			path = "<MISSING SAMPLE>";
		}
		my_printf("path %s\n", StringAsCStr(path));
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
	{
		audioclip_texture_t waveform = ptr->audio.waveformRef.waveform;
		gui_waveform_texture_ref& waveformRef = ptr->audio.waveformRef;

//		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform", FONT_SIZE_TOOLTIP_BIG}, tblint{waveform.quality}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform samplesPerPx"}, tblfloat{(float)waveform.samplesPerPx}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform pos"}, waveform.pos}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform size"}, waveform.size}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform startOffset"}, waveform.startOffset}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform clipped"}, tblstr{(waveform.clipped?"yes":"no")}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform quality"}, tblint{waveform.quality}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform scaleX"}, tblfloat{waveform.scaleX}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveformRef atlasId"}, tblint{waveformRef.atlasId}}}});
		table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveformRef atlasEntryId"}, tblint{waveformRef.atlasEntryId}}}});
	}
	Table::AdjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* gui_audio_clip::getTooltip(AppCtrl* appctrl) {
	auto tooltip = new guitooltip<clip_t>(this->m_clip); //why does casting m_clip to (clip_t*) break the ptr?
	return tooltip;
//	appctrl->openContextMenu(tooltip, appctrl->m_mousePos);
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

gui_track::gui_track(track_t* _track, scaled_grid& _grid)
  : guictr_base(), m_track(_track), midi(_track->getMidi()), automation(_track, _grid, m_track->audio->selectedAutomationCtr, m_track->audio->selectedAutomationParam, subtrackIdx)
{
	padding = 0;
}

gui_track* createTrackGui(track_t* t, scaled_grid& grid) {
	return new gui_track(t, grid);
}

void gui_track::updateVisibleTrackContents(project_t& project, scaled_grid& grid) {
	automation.setData();
	automation.updateVisibleTrackContents(grid);
	for (clip_t* clip : midi.clips) {
		if (!clip->gClip) {
			if (clip->clipType == CLIP_MIDI) {
				clip->gClip = new gui_midi_clip(m_track, clip);
			} else {
				clip->gClip = new gui_audio_clip(m_track, clip);
			}
			add(clip->gClip);
		}
		clip->gClip->updatePosition(project, grid, size);
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
			MainCtrl* ctrl = MainCtrl::get();
			scaled_grid& grid = ctrl->getGrid();
			tick_t tick = grid.screenToTickSnap(mpos.x, SNAP_OFF);
			if (ctrl->cursor.contains(this->m_track->idx, tick)) {
				evt.requestFocus(this);
				return true;
			}
		}
		// tracks need to always cancel further mouse tests for z-order to work in parent container
		return true;
	}
	return false;
}

void gui_track_automationlane::updateVisibleTrackContents(scaled_grid& grid) {
	automation.setData();
	automation.updateVisibleTrackContents(grid);
}

gui_track_automationlane::gui_track_automationlane(track_t* _track, scaled_grid& _grid, automatable_t* _at, int32_t _param)
  : guictr_base(), m_track(_track), automation(_track, _grid, at, param, idx), at(_at), param(_param)
{
	padding = 0;
}

class guictxtmenu_trackcontent : public guictxtmenu {
public:
	int32_t trackid;
	guictxtmenu_trackcontent(int32_t _trackid) {
		this->trackid = _trackid;
		this->size.x = 320;
		auto newClip = new ctxtmenu_entry("Create clip", 20);
		addEntry(newClip);
		addEntry(new ctxtmenu_splitter());
		scaled_grid& grid = MainCtrl::get()->getGrid();
		auto adaptive = new ctxtmenu_time_select(grid, "Adaptive Grid", 0);
		adaptive->initAdaptive();
		addEntry(adaptive);
		auto fixed = new ctxtmenu_time_select(grid, "Fixed Grid", 0);
		fixed->initFixed();
		addEntry(fixed);
	}
	void clicked(int _id) {
		MainCtrl* ctrl = MainCtrl::get();
		scaled_grid& grid = ctrl->getGrid();
		if (_id == 20) {
			Cursor cursor = MainCtrl::get()->cursor.getLeftAligned();
			if (cursor.selRange) {
				track_t* tr = ctrl->getTrackId(this->trackid);
				if (tr && tr->type == TRACK_TYPE_MIDI) {
					clip_t* cl = new clip_t(CLIP_MIDI, StringFormat("%s Clip", StringAsCStr(tr->name)));
					cl->time = cursor.cursorPos;
					cl->setLen(cursor.selRange);
					cl->loopStart = 0;
					cl->loopLen = cl->getLen();
					tr->getMidi().addClipSort(cl);
				}
				if (tr && tr->type == TRACK_TYPE_AUDIO) {
					clip_t* cl = new clip_t(CLIP_AUDIO, StringFormat("%s", StringAsCStr(tr->name)));
					cl->time = cursor.cursorPos;
					cl->setLen(cursor.selRange);
					cl->loopStart = 0;
					cl->loopLen = cl->getLen();
					tr->getMidi().addClipSort(cl);
				}
			}
		}
		else if (_id == 110+9) { // OFF
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
		parentCtrl->closePopup();
	}
};
void gui_track_automationlane::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}
void gui_track::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}
void guitrack_editor::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(-1), evt.mousepos);
}
