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
#include "basectrl.h"
#include "host/mainctrl.h"
#include "host/vst_host.h"
#include "table.h"
#include "guitooltip.h"


#include "guicontextmenu_daw.h"

void gui_midi_clip::handleRightClick(MouseEvent& evt) {
	parentCtrl->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
void gui_audio_clip::handleRightClick(MouseEvent& evt) {
	parentCtrl->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
void gui_audio_clip::releaseRendered() {
//	my_printf("release %012x from releaseRendered()\n", &m_clip->audio.waveformRef);
	waveformrender::getInstance()->release(&m_clip->audio.waveformRef);
//	m_clip->audio.waveformRef.fbId = -1;
	m_clip->audio.waveformRef.rendered = false;
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
void gui_audio_clip::updatePosition(project_t& project, scaled_grid& grid, ivec2& trackSize) {
	size = this->parent->size;
	culled = !getClipPosition(grid, trackSize, m_clip, pos, size, 0);
	audiofile_t* audio = audiocache::getInstance()->get(m_clip->audio.id);
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
					m_clip->audio.waveformRef.waveform = waveform;
					this->updatedWaveform = waveform;
				} else {
					bool equal = ((waveform.size.y > 0) == (m_clip->audio.waveformRef.waveform.size.y > 0)) && isEqualWaveform3(waveform, m_clip->audio.waveformRef.waveform);

					bool canQueue = waveformrender::getInstance()->canQueueUpdate();
					ivec2 sizeDiff = math::absvec2(waveform.size-m_clip->audio.waveformRef.waveform.size);
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
void gui_audio_clip::prerender(NVGcontext* vg) {
	auto& clipAudio = m_clip->audio;
	audiofile_t* audio = audiocache::getInstance()->get(clipAudio.id);
	if (!clipAudio.waveformRef.queued) {
		if (!audio || this->updatedWaveform.size.x < 1 || this->updatedWaveform.size.y < 1) {
			return;
		}
		if (!culled && (!clipAudio.waveformRef.rendered || (this->updatedWaveform != clipAudio.waveformRef.waveform))) {
			releaseRendered();
			clipAudio.waveformRef.waveform = this->updatedWaveform;
			dbgassert(!clipAudio.waveformRef.queued);
			dbgassert(clipAudio.waveformRef.waveform.size.x > 0 && clipAudio.waveformRef.waveform.size.y > 0);
			waveformrender::getInstance()->queueUpdate(audio, &clipAudio.waveformRef);
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

gui_track::gui_track(track_t* _track, scaled_grid& _grid)
  : guictr_base(), m_track(_track), automation(_track, _grid, m_track->audio->selectedAutomationCtr, m_track->audio->selectedAutomationParam, subtrackIdx)
{
	padding = 0;
}

gui_track* createTrackGui(track_t* t, scaled_grid& grid) {
	return new gui_track(t, grid);
}

gui_clip* createClipGui(guictr_base* parent, track_t* track, clip_t* clip) {
	if (!clip->gClip) {
		if (clip->clipType == CLIP_MIDI) {
			clip->gClip = new gui_midi_clip(track, clip);
		} else {
			clip->gClip = new gui_audio_clip(track, clip);
		}
	}
	return clip->gClip;
}
void gui_track::updateVisibleTrackContents(project_t& project, scaled_grid& grid) {
	automation.setData();
	automation.updateVisibleTrackContents(grid);
	std::vector<clip_t*> clips = m_track->getMidi().getClips();
	for (clip_t* clip : clips) {
		auto* gui = createClipGui(this, m_track, clip);
		dbgassert(gui);
		if (gui->parent != this) {
			add(gui);
		}
		gui->updatePosition(project, grid, size);
	}
	for (gui_track_subtrack* gui : m_track->subtracks) {
		gui->updatePosition(project, grid, size);
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

void gui_track_subtrack::updateVisibleTrackContents(scaled_grid& grid) {
	automation.setData();
	automation.updateVisibleTrackContents(grid);
}
gui_track_automationlane::gui_track_automationlane(track_t* _track, scaled_grid& _grid, automatable_t* _at, int32_t _param)
  : gui_track_subtrack(_track, _grid, _at, _param)
{
}

gui_track_subtrack::gui_track_subtrack(track_t* _track, scaled_grid& _grid, automatable_t* _at, int32_t _param)
  : guictr_base(), m_track(_track), automation(_track, _grid, this->at, param, idx), at(_at), param(_param)
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
		auto newClip2 = new ctxtmenu_entry("Create test clip", 21);
		addEntry(newClip2);
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
		if (_id == 21) {

			Cursor cursor = MainCtrl::get()->cursor.getLeftAligned();
			track_t* tr = ctrl->getTrackId(this->trackid);
			if (tr) {
			clip_t* clip = new clip_t;
			clip->name = "Test Clip";
			clip->time = 11354255;
			clip->len = 12145;
			clip->offsetStart = 11272335;
			clip->loopLen = 11272192;
			clip->loopEnabled = false;

			note_t n;
			n = note_t{52, 37, 11232734, 6152, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11232734, 6152, 1};  clip->notes.add(n);
			n = note_t{74, 37, 11232734, 6152, 1};  clip->notes.add(n);
			n = note_t{86, 37, 11232734, 6152, 1};  clip->notes.add(n);
			n = note_t{70, 37, 11235903, 6152, 1};  clip->notes.add(n);
			n = note_t{82, 37, 11235903, 6152, 1};  clip->notes.add(n);
			n = note_t{70, 37, 11235903, 6152, 1};  clip->notes.add(n);
			n = note_t{82, 37, 11235903, 6152, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11238886, 6151, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11239072, 5965, 1};  clip->notes.add(n);
			n = note_t{74, 37, 11239072, 5965, 1};  clip->notes.add(n);
			n = note_t{86, 37, 11239072, 5965, 1};  clip->notes.add(n);
			n = note_t{70, 37, 11242055, 6151, 1};  clip->notes.add(n);
			n = note_t{82, 37, 11242055, 6151, 1};  clip->notes.add(n);
			n = note_t{94, 37, 11242055, 6151, 1};  clip->notes.add(n);
			n = note_t{106, 37, 11242055, 6151, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11245037, 6338, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11245037, 6338, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{74, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{86, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{98, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{110, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11245037, 6338, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11245037, 6338, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{74, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{86, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{98, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{110, 37, 11245224, 6151, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11248393, 5965, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11248393, 5965, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11248393, 5965, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11248393, 5965, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11248393, 5965, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{78, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{90, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{102, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{78, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{90, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{102, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11251375, 6152, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11254358, 6152, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11254544, 5966, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11254544, 5966, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11254544, 5966, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11254544, 6152, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11254544, 6152, 1};  clip->notes.add(n);
			n = note_t{80, 37, 11254731, 6151, 1};  clip->notes.add(n);
			n = note_t{92, 37, 11254731, 6151, 1};  clip->notes.add(n);
			n = note_t{104, 37, 11254731, 6151, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11257527, 6152, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11260696, 5965, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11260696, 5965, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11260696, 5965, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11260696, 5965, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11260696, 5965, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11263679, 6151, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11263679, 6151, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11263679, 6151, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11263679, 6151, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11263679, 6151, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11263679, 6151, 1};  clip->notes.add(n);
			n = note_t{81, 37, 11266661, 6152, 1};  clip->notes.add(n);
			n = note_t{93, 37, 11266661, 6152, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11266848, 5965, 1};  clip->notes.add(n);
			n = note_t{105, 37, 11266848, 5965, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11266848, 6151, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11266848, 6151, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11266848, 6151, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11266848, 6151, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11266848, 6151, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11266848, 6151, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11266848, 6151, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11266848, 6151, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11269830, 6152, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11269830, 6152, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11270017, 5965, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11270017, 5965, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11270017, 5965, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11270017, 5965, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11272999, 5966, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11272999, 5966, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11272999, 6152, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11272999, 6152, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11272999, 6152, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11272999, 6152, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11272999, 6152, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11272999, 6152, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11275982, 6152, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11275982, 6152, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11275982, 6152, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11275982, 6152, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11279151, 2983, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11279151, 2983, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11279151, 2983, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11279151, 2983, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11279151, 2983, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11276168, 6152, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11276168, 6152, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11282320, 5965, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11282320, 5965, 1};  clip->notes.add(n);
			n = note_t{74, 37, 11282320, 5965, 1};  clip->notes.add(n);
			n = note_t{86, 37, 11282320, 5965, 1};  clip->notes.add(n);
			n = note_t{70, 37, 11285303, 6151, 1};  clip->notes.add(n);
			n = note_t{82, 37, 11285303, 6151, 1};  clip->notes.add(n);
			n = note_t{70, 37, 11285303, 6151, 1};  clip->notes.add(n);
			n = note_t{82, 37, 11285303, 6151, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11288285, 6152, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11288285, 6152, 1};  clip->notes.add(n);
			n = note_t{74, 37, 11288285, 6152, 1};  clip->notes.add(n);
			n = note_t{86, 37, 11288285, 6338, 1};  clip->notes.add(n);
			n = note_t{86, 37, 11288285, 6338, 1};  clip->notes.add(n);
			n = note_t{70, 37, 11291454, 6152, 1};  clip->notes.add(n);
			n = note_t{82, 37, 11291454, 6152, 1};  clip->notes.add(n);
			n = note_t{94, 37, 11291454, 6152, 1};  clip->notes.add(n);
			n = note_t{106, 37, 11291454, 6152, 1};  clip->notes.add(n);
			n = note_t{70, 37, 11291454, 6152, 1};  clip->notes.add(n);
			n = note_t{82, 37, 11291454, 6152, 1};  clip->notes.add(n);
			n = note_t{94, 37, 11291454, 6152, 1};  clip->notes.add(n);
			n = note_t{106, 37, 11291454, 6152, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11294623, 5966, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11294623, 5966, 1};  clip->notes.add(n);
			n = note_t{74, 37, 11294623, 5966, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11294623, 5966, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11294623, 5966, 1};  clip->notes.add(n);
			n = note_t{98, 37, 11294623, 5966, 1};  clip->notes.add(n);
			n = note_t{110, 37, 11294623, 5966, 1};  clip->notes.add(n);
			n = note_t{86, 37, 11294623, 6152, 1};  clip->notes.add(n);
			n = note_t{86, 37, 11294623, 6152, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11297606, 6152, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11297606, 6152, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11297606, 6152, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11297606, 6152, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11297606, 6152, 1};  clip->notes.add(n);
			n = note_t{78, 37, 11300775, 5965, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{90, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{102, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{90, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{102, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11300775, 6152, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11303758, 6151, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11303758, 6151, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11303758, 6151, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11303758, 6151, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11303758, 6151, 1};  clip->notes.add(n);
			n = note_t{80, 37, 11304130, 6152, 1};  clip->notes.add(n);
			n = note_t{92, 37, 11304130, 6152, 1};  clip->notes.add(n);
			n = note_t{104, 37, 11304130, 6152, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11306927, 5965, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11306927, 6151, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11306927, 6151, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11306927, 6151, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11306927, 6151, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11306927, 6151, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11306927, 6151, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11306927, 6151, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11306927, 6151, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11306927, 6151, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11306927, 6151, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11309909, 6152, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11309909, 6152, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11309909, 6152, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11309909, 6152, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11309909, 6152, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11313078, 5965, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11313078, 5965, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11313078, 5965, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11313078, 5965, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11313078, 5965, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11313078, 6152, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11313078, 6152, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11316061, 6151, 1};  clip->notes.add(n);
			n = note_t{81, 37, 11316061, 6151, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11316061, 6151, 1};  clip->notes.add(n);
			n = note_t{93, 37, 11316061, 6151, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11316061, 6151, 1};  clip->notes.add(n);
			n = note_t{105, 37, 11316061, 6151, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11316061, 6151, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11316247, 5965, 1};  clip->notes.add(n);
			n = note_t{76, 37, 11319230, 5965, 1};  clip->notes.add(n);
			n = note_t{88, 37, 11319230, 5965, 1};  clip->notes.add(n);
			n = note_t{100, 37, 11319230, 5965, 1};  clip->notes.add(n);
			n = note_t{112, 37, 11319230, 5965, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11319230, 6152, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11319230, 6152, 1};  clip->notes.add(n);
			n = note_t{52, 37, 11319230, 6152, 1};  clip->notes.add(n);
			n = note_t{64, 37, 11319230, 6152, 1};  clip->notes.add(n);
			n = note_t{72, 37, 11322212, 6152, 1};  clip->notes.add(n);
			n = note_t{84, 37, 11322212, 6152, 1};  clip->notes.add(n);
			n = note_t{96, 37, 11322212, 6152, 1};  clip->notes.add(n);
			n = note_t{108, 37, 11322212, 6152, 1};  clip->notes.add(n);
			n = note_t{120, 37, 11322212, 6152, 1};  clip->notes.add(n);
			clip->setDirty();
			clip->notes.updateBounds();
			tr->getMidi().addClipSort(clip);

			}
		} else if (_id == 20) {
			Cursor cursor = MainCtrl::get()->cursor.getLeftAligned();
			if (cursor.selRange) {
				track_t* tr = ctrl->getTrackId(this->trackid);
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
		closeContextMenu();
	}
};
void gui_track_automationlane::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}
void gui_track_subtrack::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}
void gui_track::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}
void guitrack_editor::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(-1), evt.mousepos);
}


void gui_track_subtrack::renderMixerInfo(NVGcontext* vg) {

	MainCtrl* ctrl = MainCtrl::get();
	String curvalue = "UNDEF";
	String target = "<NULL>";
	automatable_t* ctr = at;
	if (ctr) {
		target = StringFormat("%s %08X", StringAsCStr(ctr->getAutomatableName()), ctr);
		int32_t idx = param;
		if (idx >= 0) {
			automation_t* automation = ctr->getRegisteredAutomation(idx);
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
