#include <nanovg.h>
#include <GLFW/glfw3.h>
#include <time.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>

#include "mainctrl.h"
#include "math/seq_math.h"
#include "basectrl.h"
#include "window.h"
#include "platform.h"
#include "keyboard.h"
#include "commands.h"
#include "project.h"
#include "projectfile.h"
#include "grid.h"
#include "note.h"
#include "cursor.h"
#include "exceptions.h"
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "track.h"
#include "clip.h"
#include "fileloader.h"
#include "edithistory.h"
#include "logging.h"
#include "menu.h"
#include "msgbox.h"

#include "../gui/gui.h"
#include "../gui/guicontainer.h"
#include "../gui/button.h"
#include "../gui/splitter.h"
#include "../gui/guicontextmenu_base.h"
#include "../gui/tempocontrols.h"
#include "../gui/scrollbar.h"
#include "../gui/statusbar.h"
#include "../gui/pluginctr.h"
#include "../gui/clipeditor.h"
#include "../gui/trackctr.h"
#include "../gui/trackcontent.h"
#include "../gui/trackctr.h"
#include "../gui/list.h"
#include "../gui/pluginlist.h"
#include "../gui/guimenu.h"
#include "../gui/debugctr.h"
#include "../gui/drawwaveform.h"

#include "vst_host.h"
#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "track_impl.h"
#include "audiocache.h"
#include "seq_time.h"

#include "../gui/guiplugin.h"
#include "../threads/workerthread.h"
#include "../threads/playbackthread.h"

using namespace std;

void copyClipsInRange(trackdata_midi_t& in, track_clipboard_t& out, int32_t srcPos, int32_t dstPos, int32_t len) {

	auto it = in.clips.cbegin();
	while (it != in.clips.cend()) {
		const clip_t* c = *it;
		if (c->end() > srcPos && c->time < srcPos+len) {
			clip_t clone(*c);
			if (c->time < srcPos && c->end() > srcPos) {
				cutClipLeft(&clone, srcPos - c->time);
			}
			if (c->time < srcPos + len && c->end() > srcPos + len) {
				cutClipRight(&clone, (c->end()) - (srcPos+len));
			}
			out.clips.push_back(make_shared<clip_t>(move(clone)));
		}
		it++;
	}
	stable_sort(out.clips.begin(), out.clips.end(), [](
			shared_ptr<clip_t> const & a, shared_ptr<clip_t> const & b) {
		return a->time < b->time;
	});
//	if (clips.size() > 1) {
//		if (!(clips[0]->start() < clips[1]->start())) {
//			for (int i = 0; i < clips.size(); i++) {
//				my_printf("clip[%d] = %d\n", i, clips[i]->start());
//			}
//		}
//		dbgassert(clips[0]->start() < clips[1]->start());
//	}
//	out->sortClips();
}
void MainCtrl::pasteClipboard(clip_clipboard* clipboard, Cursor& cursor) {
	if (clipboard->type == clip_clipboard::ClipboardFull) {
		if (cursor.isSubtrackSelection())
			return;
		pasteClipboard(clipboard, cursor.getTrackBegin(), cursor.getTickBegin());
	} else  if (clipboard->type == clip_clipboard::ClipboardAutomation) {
		if (!cursor.isSubtrackSelection())
			return;
		int32_t tickBegin = cursor.getTickBegin();
		int32_t tickLen = clipboard->selRange;
		int32_t trackBegin = cursor.getTrackBegin();
		if (trackList.validTrackIdx(trackBegin)) {
			track_t* tr = trackList[trackBegin];
			int32_t subTrackOffset = cursor.getSubTrackBegin();
			for (int i = 0; i <= clipboard->selTrackRange; i++) {
				int32_t subTrackIdx = subTrackOffset + i;
				if (tr->validSubtrack(subTrackIdx)) {
					gui_track_subtrack* subtrack = tr->subtracks[subTrackIdx];
					std::vector<automation_point_t>& data = clipboard->automationLanes[i];
					if (data.size() && subtrack->at) {
						automation_t* automation = subtrack->at->getOrCreateAutomation(subtrack->param);
						if (automation) {
							automation->setRange(tickBegin, tickBegin+tickLen, data);
						}
					}
				}

			}
		}
	}
}
void MainCtrl::pasteClipboard(clip_clipboard* clipboard, int32_t track, tick_t tick) {
	tick_t tickOffset = tick - clipboard->srcPos;
	tick_t trackOffset = track;
	for (int i = 0; i <= clipboard->selTrackRange; i++) {
		track_clipboard_t* trClipboard = clipboard->tracks[i].get();
		if (!trackList.validTrackIdx(i + trackOffset)) {
			continue;
		}
		int32_t trackIdx = trackList.clampTrackIdx(i + trackOffset);
		track_t* tr = trackList[trackIdx];
//		if (tr->type == TRACK_TYPE_MIDI) {
			trackdata_midi_t& midi = tr->getMidi();
			for (auto it = trClipboard->clips.begin(); it != trClipboard->clips.end(); it++) {
				clip_t* cl = (*it).get();
				clip_t* cloned = cl->clone();
				cloned->time += tickOffset;
				cutIntersecting(tr, cloned);
				midi.addClip(cloned);
			}
			midi.sortClips();
//		}
	}

}
shared_ptr<clip_clipboard> MainCtrl::copySelection(const Cursor& _cursor) {
	int32_t tickBegin = _cursor.getTickBegin();
	int32_t tickEnd = _cursor.getTickEnd();
	int32_t trackBegin = _cursor.getTrackBegin();
	int32_t trackEnd = _cursor.getTrackEnd();
	int32_t trackSubBegin = _cursor.getSubTrackBegin();
	int32_t trackSubEnd = _cursor.getSubTrackEnd();
	shared_ptr<clip_clipboard> clipboard = make_shared<clip_clipboard>();
	clipboard->srcPos = tickBegin;
	clipboard->srcTrack = trackBegin;
	clipboard->selRange = tickEnd - tickBegin;
	if (_cursor.isSubtrackSelection()) {
		clipboard->selTrackRange = trackSubEnd - trackSubBegin;
		clipboard->type = clip_clipboard::ClipboardAutomation;
		if (trackList.validTrackIdx(trackBegin)) {
			track_t* tr = trackList[trackBegin];
			for (int i = trackSubBegin; i <= trackSubEnd; i++) {
				if (tr->validSubtrack(i)) {
					gui_track_subtrack* subtrack = tr->subtracks[i];
					automatable_t* automatable = subtrack->at;
					automation_t* automation = NULL;
					if (automatable) {
						automation = automatable->getRegisteredAutomation(subtrack->param);
					}

					std::vector<automation_point_t> data;
					if (automation) {
						automation->copyRange(tickBegin, tickEnd, data);
					}
					clipboard->automationLanes.push_back(std::move(data));

				}
			}
		}
	} else {
		clipboard->selTrackRange = trackEnd - trackBegin;
		clipboard->selRange = tickEnd - tickBegin;
		clipboard->type = clip_clipboard::ClipboardFull;
		for (int i = 0; i <= clipboard->selTrackRange; i++) {
			track_clipboard_t trackClipboard;
			if (trackList.validTrackIdx(trackBegin + i)) {
				track_t* tr = trackList[trackBegin + i];
//				if (tr->type == TRACK_TYPE_MIDI) {
					copyClipsInRange(tr->getMidi(), trackClipboard, clipboard->srcPos, 0, clipboard->selRange);
//				}
			}
			clipboard->tracks.push_back(make_shared<track_clipboard_t>(move(trackClipboard)));
		}
	}
	return clipboard;
}


void cutIntersectingClips(trackdata_midi_t& midi, tick_t tickBegin, tick_t tickEnd, delete_cb *cb) {
	vector<clip_t*>::iterator it = midi.clips.begin();

	while (it != midi.clips.end()) {
		clip_t* c = *it;
		if (c->start() >= tickEnd || c->end() <= tickBegin) {
			it++;
			continue;
		}
		if (c->start() >= tickBegin && c->end() <= tickEnd) {
			it = midi.removeClip(c);
			releaseClipResources(c, cb);
			delete c;
			continue;
		} else if (c->time >= tickBegin) {
			//cut left
			cutClipLeft(c, tickEnd-c->time);
			c->setDirty();
		} else if (c->end() <= tickEnd) {
			//cut right
			cutClipRight(c, c->end() - tickBegin);
			c->setDirty();
		} else {
			clip_t* c2 = c->clone();
			cutClipRight(c, c->end() - tickBegin);
			cutClipLeft(c2, tickEnd-c->time);
			it = midi.clips.insert(it, c2);
			c->setDirty();
		}
		it++;
	}
	midi.sortClips();
}
void MainCtrl::cutIntersecting(track_t* tr, tick_t tickBegin, tick_t tickEnd) {
	cutIntersectingClips(tr->getMidi(), tickBegin, tickEnd, this);
}
void MainCtrl::cutIntersecting(track_t* tr, clip_t* mask) {
	tick_t tickBegin = mask->time;
	tick_t tickEnd = mask->end();
	cutIntersecting(tr, tickBegin, tickEnd);
}
void MainCtrl::cutSelection(const Cursor& _cursor) {
	int32_t tickBegin = _cursor.getTickBegin();
	int32_t tickEnd = _cursor.getTickEnd();
	int32_t trackBegin = _cursor.getTrackBegin();
	int32_t trackEnd = _cursor.getTrackEnd();
	if (!cursor.isSubtrackSelection()) {
		for (int i = trackBegin; i <= trackEnd; i++) {
			if (trackList.validTrackIdx(i)) {
				track_t* tr = trackList[i];
//				if (tr->type == TRACK_TYPE_MIDI) {
					cutIntersecting(tr, tickBegin, tickEnd);
//				}
			}
		}
	} else {
		int32_t trackSBegin = _cursor.getSubTrackBegin();
		int32_t trackSEnd = _cursor.getSubTrackEnd();
		if (trackList.validTrackIdx(trackBegin)) {
			track_t* tr = trackList[trackBegin];
			std::vector<automation_point_t> empty(0);
			for (int i = 0; i <= trackSEnd-trackSBegin; i++) {
				int32_t subTrackIdx = trackSBegin + i;
				if (tr->validSubtrack(subTrackIdx)) {
					gui_track_subtrack* subtrack = tr->subtracks[subTrackIdx];
					automation_t* automation = subtrack->getAutomation();
					if (automation) {
						automation->setRange(tickBegin, tickEnd, empty);
					}
				}

			}
		}
	}
}
