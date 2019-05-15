#include "trackctr.h"
#include <stdbool.h>
#include <stdint.h>
#include <vector>
#include "exceptions.h"
#include "seq_util.h"
#include "str_util.h"
#include "color_util.h"
#include "math/seq_math.h"
#include "track.h"
#include "clip.h"
#include "clipboard.h"
#include "cursor.h"
#include "edithistory.h"
#include "keyboard.h"
#include "basectrl.h"
#include "../host/mainctrl.h"
#include "grid.h"
#include "guicontainer.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "tracktimeline.h"
#include "theme.h"
#include "guicontextmenu.h"
#include "mouse.h"
#include "mousecursor.h"
#include "logging.h"
#include "audiocache.h"
#include "audiowaveform.h"
#include "drawwaveform.h"
#include "cliprenderer.h"
#include "logging.h"

class action_modify_track : public action_base {
protected:
	trackstate_t before;
	trackstate_t after;
public:
	action_modify_track() : action_base() {
	}
	action_modify_track(String description, trackstate_t&& _tracks) : action_base() {
		desc = description;
		before = std::move(_tracks);
	}

	void undo(MainCtrl* ctrl) {
		my_printf("action_modify_track undo, num tracks: %d\n", before.tracks.size());
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		bool initAfter = after.tracks.empty();
		if (initAfter) {
			after.cursor = ctrl->cursor;
		}
		trackallcontainer_t& trCtr = ctrl->getTracks();
		for (track_snapshot_t* trackStored : before.tracks) {
			my_printf("trackStored: %s %d\n", TrackTypeToName(trackStored->type), trackStored->localIdx);
			if (trCtr.validTrackTypeIdx(trackStored->type, trackStored->localIdx)) {
				track_t* track = trCtr.getTrackTypeIdx(trackStored->type, trackStored->localIdx);
				if (initAfter) {
					after.tracks.push_back(new track_snapshot_t(track, false));
				}
				track->getMidi().deleteClips(ctrl);
				track->releaseTrackContent();
//				if (track->type == TRACK_TYPE_MIDI)
				my_printf("TRACKBeforeUndo[%d] HAS %d clips\n", track->idx, track->getMidi().getConstClips().size());
				*track = *trackStored;
//				track->loadPluginAutomationParameters(trackStored->plugins);
//				if (track->type == TRACK_TYPE_MIDI)
				my_printf("TRACKAfterUndo[%d] HAS %d clips\n", track->idx, track->getMidi().getConstClips().size());
			} else {

				my_printf("idx is now invalid\n",0);
			}
		}
		ctrl->cursor = before.cursor;
	}
	void redo(MainCtrl* ctrl) {
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		trackallcontainer_t& trCtr = ctrl->getTracks();
		for (track_snapshot_t* trackStored : after.tracks) {
			if (trCtr.validTrackTypeIdx(trackStored->type, trackStored->localIdx)) {
				track_t* track = trCtr.getTrackTypeIdx(trackStored->type, trackStored->localIdx);
				track->getMidi().deleteClips(ctrl);
				track->releaseTrackContent();
				*track = *trackStored;
//				track->loadPluginAutomationParameters(trackStored->plugins);
//				if (track->type == TRACK_TYPE_MIDI)
				my_printf("TRACK[%d] HAS %d clips\n", track->idx, track->getMidi().getConstClips().size());
			}
		}
		ctrl->cursor = after.cursor;
	}
};

void resizeOtherClips(trackdata_midi_t& midi, clip_t* clip) {
	for (clip_t* c : midi.clips) {
		if (c == clip)
			continue;
		if (c->start() >= clip->end() || c->end() <= clip->start()) {
			continue;
		}
		if (c->start() >= clip->start() && c->end() <= clip->end()) {
			c->setLen(0);
		} else if (c->start() >= clip->start()) {
			cutClipLeft(c, clip->end()-c->start());
			c->setDirty();
		} else if (c->end() <= clip->end()) {
			cutClipRight(c, c->end()-clip->start());
			c->setDirty();
		} else {
			my_printf("WHUT!\n", 0);
		}
	}
}
bool guitrack_editor::handleKeyInput(KeyEvent& kevt) {
//	clip_t* clip = view.clip;
//	if (!clip) {
//		return false;
//	}
//	clip_notes_t& notes = clip->notes;
	if (kevt.type != STATE_REPEAT && isCtrlKey(kevt.keyCode)) {
		if ((action.dragtype == DRAG_CLIPS_MOVE || action.dragtype == DRAG_CLIPS_COPY)) {
			if ((action.dragtype == DRAG_CLIPS_COPY) != isCtrl(kevt.mods)) {
				if (action.dragtype == DRAG_CLIPS_MOVE) {
					action.dragtype = DRAG_CLIPS_COPY;
					MainCtrl::get()->cursorIcon = CURSOR_DUPLICATE;
				} else {
					action.dragtype = DRAG_CLIPS_MOVE;
					MainCtrl::get()->cursorIcon = CURSOR_DEFAULT;
				}
			}
			return false;
		}
	}
	if (kevt.type != STATE_REPEAT && isAltKey(kevt.keyCode)) {
//		if ((action.dragtype == DRAG_CLIPS_MOVE || action.dragtype == DRAG_CLIPS_COPY)) {
//			if ((action.dragtype == DRAG_CLIPS_COPY) != isCtrl(kevt.mods)) {
//				if (action.dragtype == DRAG_CLIPS_MOVE) {
//					action.dragtype = DRAG_CLIPS_COPY;
//					MainCtrl::get()->cursorIcon = CURSOR_DUPLICATE;
//				} else {
//					action.dragtype = DRAG_CLIPS_MOVE;
//					MainCtrl::get()->cursorIcon = CURSOR_DEFAULT;
//				}
//			}
//			return false;
//		}
		MainCtrl::get()->window->fireMouseMoved();
		return false;
	}
	if (action.dragtype) {
		return false;
	}
	if (kevt.type != K_RELEASE) {
		trackstate_t preModifyState;
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		bool modified = false;
		bool handledKeyinput = false;
		String desc = "???";
		if (kevt.type == K_PRESS) {
			if (isKC(KC_SELECTALL, kevt)) {
				tick_t evtMin = INVALID_TICK;
				tick_t evtMax = INVALID_TICK;
				track_t* trMin = NULL;
				track_t* trMax = NULL;
				int idx = 0;
				for (track_t* t: project.trackList) {
					auto minMax = t->getMinMaxEvents();
					if (minMax.min != INVALID_TICK) {
						evtMin = evtMin == INVALID_TICK ? minMax.min : math::min(evtMin, minMax.min);
						if (!trMin || trMin->idx > t->idx) {
							trMin = t;
						}
						evtMax = evtMax == INVALID_TICK ? minMax.max : math::max(evtMax, minMax.max);
						if (!trMax || trMax->idx < t->idx) {
							trMax = t;
						}
					}
					idx++;
				}
				if (evtMin != INVALID_TICK) {
					cursor.cursorPos = evtMin;
					cursor.selRange = evtMax-evtMin;
					cursor.cursorTrack = trMin->idx;
					cursor.selTrackRange = (trMax->idx - cursor.cursorTrack);
					cursor.cursorSubTrack = -1;
					cursor.selSubTrackRange = 0;
				}
				handledKeyinput = true;
			}
			if (isKC(KC_DELETE, kevt) && cursor.getRange()) {
//				for (track_t* t: trCtr) {
//					if (cursor.inTrackRange(t->idx)) {
//						ctrl->cutIntersecting(t, cursor.getTickBegin(), cursor.getTickEnd());
//					}
//				}.reserve(_tracks.size());
				project.trackList.copyTracks(cursor.getTrackBegin(), cursor.getTrackEnd(), preModifyState);
				preModifyState.cursor = cursor;
				MainCtrl::get()->cutSelection(cursor);
				handledKeyinput = true;
				modified = true;
				desc = "Delete clips";
			}
			else if (isKC(KC_CUT, kevt) && cursor.getRange()) {
				clipboard = MainCtrl::get()->copySelection(cursor);
				project.trackList.copyTracks(cursor.getTrackBegin(), cursor.getTrackEnd(), preModifyState);
				preModifyState.cursor = cursor;
				MainCtrl::get()->cutSelection(cursor);
				handledKeyinput = true;
				modified = true;
				desc = "Cut clips";
			}
			else if (isKC(KC_COPY, kevt) && cursor.getRange()) {
				clipboard = MainCtrl::get()->copySelection(cursor);
				handledKeyinput = true;
			}
			else if (isKC(KC_DUPLICATE, kevt) && cursor.getRange()) {
				project.trackList.copyTracks(cursor.getTrackBegin(), cursor.getTrackEnd(), preModifyState);
				preModifyState.cursor = cursor;
				/* maybe do    this->clipboard = copy */
				std::shared_ptr<clip_clipboard> newClipboard = MainCtrl::get()->copySelection(cursor);
				cursor.setLeftAligned();
				cursor.cursorPos += cursor.getRange();
				MainCtrl::get()->pasteClipboard(newClipboard.get(), cursor);
				grid.makeTickVisible(cursor.cursorPos+newClipboard->selRange);
				handledKeyinput = true;
				modified = true;
				desc = "Duplicate clips";
			}
			else if (isKC(KC_PASTE, kevt) && clipboard) {
				project.trackList.copyTracks(cursor.getTrackBegin(), cursor.getTrackBegin()+clipboard->selTrackRange, preModifyState);
				preModifyState.cursor = cursor;
				cursor.setLeftAligned();
				if (clipboard->type == clip_clipboard::ClipboardFull)
					MainCtrl::get()->cutSelection(cursor);
				MainCtrl::get()->pasteClipboard(clipboard.get(), cursor);
				cursor.selTrackRange = clipboard->selTrackRange;
				cursor.selRange = clipboard->selRange;
				grid.makeTickVisible(cursor.getTickEnd());
				handledKeyinput = true;
				modified = true;
				desc = "Paste clips";
			}
		} else {
		}
		if (isArrowKey(kevt.keyCode)) {
			ivec2 dir;
			arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
			if (dir.y) {
				if (isShift(kevt.mods)) {
					if (cursor.isSubtrackSelection()) {
						if (project.trackList.validTrackIdx(cursor.cursorTrack)) {
							cursor.selSubTrackRange += -dir.y;
							track_t* tr = project.trackList[cursor.cursorTrack];
							fixCursorSubRange(cursor, tr->subtracks.size());

						}
					} else {
						cursor.selTrackRange += -dir.y;
						fixCursorTrackRange(cursor, project.trackList.size());
					}
				} else {

					cursor.setLeftAligned();
					//				cursor.selRange = 0;
					//				cursor.selTrackRange = 0;
					auto moveMainCursor = [this, &dir](){
						cursor.cursorTrack = project.trackList.clampTrackIdx(cursor.cursorTrack - dir.y);
					};
					auto moveCursor = [this, &dir, &moveMainCursor](){
						if (cursor.isSubtrackSelection()) {
							if (!project.trackList.validTrackIdx(cursor.cursorTrack)) {
								cursor.cursorTrack = 0;
								cursor.cursorSubTrack = -1;
								cursor.selSubTrackRange = 0;
								return;
							}
							track_t* tr = project.trackList[cursor.cursorTrack];
							cursor.cursorSubTrack -= dir.y;
							fixCursorSubRange(cursor, tr->subtracks.size());
							return;
						}
						moveMainCursor();

					};
					moveCursor();
				}
			} else if (dir.x) {
				tick_t tickStBfr = cursor.getTickBegin();
				tick_t tickEndBfr = cursor.getTickEnd();
				tick_t timeOffset = dir.x*grid.getTickLength();
				if (isShift(kevt.mods)) {
					cursor.selRange += timeOffset;
				} else {
					cursor.selRange = 0;
					cursor.selTrackRange = 0;
					cursor.selSubTrackRange = 0;
					cursor.cursorPos = math::max(0, cursor.cursorPos + timeOffset);
				}
				if (tickStBfr != cursor.getTickBegin())
					grid.makeTickVisible(cursor.getTickBegin() + timeOffset);
				if (tickEndBfr != cursor.getTickEnd())
					grid.makeTickVisible(cursor.getTickEnd() + timeOffset);
			}
			handledKeyinput = true;
//			desc = "Move notes";
		}
		if (modified) {
			action_modify_track* track_action = new action_modify_track(desc, preModifyState.copy()); // could be more efficient
			MainCtrl::get()->pushHist(track_action);


		}
		if (handledKeyinput) {
			updateVisibleTrackContents();
		}
		return handledKeyinput;
	}
	return false;
}


void guitrack_editor::trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
	ivec2 local = evt.relMousepos;
	int32_t tick = grid.screenToTickSnap(local.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
	track_t* tr = getTrackFromMouse(project, local, false);
	gui_track_subtrack* subTr = getSubTrackFromMouse(project, local, false);
	if (subTr) {
		tr = subTr->m_track;
	}
	trSelected = tr;
	subTrSelected = subTr;
	if (trSelected != NULL) {
		MainCtrl::get()->setSelectedTrack(trSelected);
		MainCtrl::get()->setEditClip(NULL);
		if (evt.guiDragged == this) { // cursor move / range select
			Cursor& c = MainCtrl::get()->cursor;
			c.selRange = 0;
			c.selTrackRange = 0;
			c.cursorPos = tick;
			c.cursorTrack = trSelected->idx;
			c.cursorSubTrack = subTrSelected ? subTrSelected->idx : -1;
			c.selSubTrackRange = 0;
		}
	}
}

void guitrack_editor::trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {
	if (trSelected != NULL) {
		Cursor& c = MainCtrl::get()->cursor;
		ivec2 local = evt.relMousepos;
		track_t* trNxtSelected = NULL;
		gui_track_subtrack* subTr = getSubTrackFromMouse(project, local, true);
		if (subTrSelected) {
			if (subTr && subTr->m_track != subTrSelected->m_track) {
				subTr = NULL;
			}
			trNxtSelected = getTrackFromMouse(project, local, false);
			if (trNxtSelected && trNxtSelected->idx < subTrSelected->m_track->idx) {
				subTr = subTrSelected->m_track->subtracks.front();
			}
			if (trNxtSelected && trNxtSelected->idx > subTrSelected->m_track->idx) {
				subTr = subTrSelected->m_track->subtracks.back();
			}
		} else {
			trNxtSelected = getTrackFromMouse(project, local, true);

			MainCtrl::get()->setSelectedTrack(trNxtSelected);
			if (!trNxtSelected)
				return;
		}
		int32_t tick = grid.screenToTickSnap(local.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
		if (evt.guiDragged == this) { // cursor move / range select

			c.selRange = tick - c.cursorPos;
			if (c.isSubtrackSelection()) {
				//c.isSubtrackSelection() guarantees subTrSelected to be non-null
				dbgassert(subTrSelected);
				if (subTr) {
					c.selSubTrackRange = (subTr->idx - subTrSelected->idx);
					dbgassert (c.getSubTrackEnd() > -1);
					dbgassert (c.getSubTrackBegin() <= c.getSubTrackEnd());
					dbgassert (c.getSubTrackBegin() < (int)subTr->m_track->subtracks.size());
					dbgassert (c.getSubTrackEnd() < (int)subTr->m_track->subtracks.size());
				}

			} else {
				c.selTrackRange = (trNxtSelected->idx - trSelected->idx);
			}
//			beatbar16th_t songPos = MainCtrl::get()->toBeatBar16th(tick);
			//my_printf("Select at Track %d - %d %d %d %d = %u.%u.%u\n", trSelected->idx, c.cursorPos, tick, c.selRange, local.x, songPos.bar, songPos.beat, songPos.th);

		}
	}
}
void guitrack_editor::trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
	trSelected = NULL;
	subTrSelected = NULL;
}
void guitrack_editor::dragSelectionBegin(gui_clip* gClip, MouseEvent& evt) {
	selectionMoved = false;
	ivec2 local = evt.relMousepos;
	tick_t tickExact = grid.screenToTickSnap(local.x, SNAP_OFF);
	Cursor& cursor = MainCtrl::get()->cursor;
	track_t* track = gClip->m_track;
	clip_t* clicked = gClip->m_clip;
//		ghostCopy = new gui_clip(clip->m_clip->clone());
//		ghostCopy->m_clip->gClip = ghostCopy;
	track_t *trackClicked = getTrackFromMouse(project, local, false);
	if (trackClicked) {
		MainCtrl::get()->setSelectedTrack(trackClicked);
	}

	action.dragtype = DRAG_NONE;
	action.clipboard = NULL;
	if (evt.mousepos.x - gClip->toScreenSpace(ivec2(0)).x < DRAG_RANGE) {
		action.dragtype = DRAG_CLIPS_RESIZE_LEFT;
	} else if (gClip->toScreenSpace(ivec2(gClip->size.x, 0)).x - evt.mousepos.x < DRAG_RANGE) {
		action.dragtype = DRAG_CLIPS_RESIZE_RIGHT;
	}
	if (action.dragtype) {
		setSelectionRange(clicked, track);
		dragStartLayout = track->getMidi(); //copy
		action.cursorBegin = cursor;
		resizePreModifyState.reset();
		project.trackList.copyTracks(cursor.getTrackBegin(), cursor.getTrackEnd(), resizePreModifyState);
		resizePreModifyState.cursor = cursor;
		return;
	}
	if (trackClicked != NULL) {
		if (!cursor.selRange || cursor.isSubtrackSelection() || !cursor.contains(trackClicked->idx, tickExact)) {
			setSelectionRange(clicked, trackClicked);
		}
		cursor.setLeftAligned();
		dragStartTick = tickExact;
		dragStartTrackIdx = trackClicked->idx;
		if (isCtrl(evt.kbmods)) {
			action.dragtype = DRAG_CLIPS_COPY;
			MainCtrl::get()->cursorIcon = CURSOR_DUPLICATE;
		} else {
			action.dragtype = DRAG_CLIPS_MOVE;
		}
		action.cursorBegin = cursor;
		action.clipboard = MainCtrl::get()->copySelection(action.cursorBegin);
	}
}
void guitrack_editor::dragSelectionMove(gui_clip* gui, MouseEvent& evt) {
	if (action.dragtype) {
		if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT
				|| action.dragtype == DRAG_CLIPS_RESIZE_RIGHT) {
			ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			clip_t* clip = gui->m_clip;
			track_t* track = gui->m_track;
			dragStartLayout.apply(track);
			int32_t tick = grid.screenToTickSnap(evt.relMousepos.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
			if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT) {
				if (clip->start() != tick) {
					int32_t preLen = clip->getLen();
					my_printf("pre clip->getLen() %d len %d samples %d\n", clip->getLen(), clip->len, clip->lenSamples);
					tick_t offset = tick - clip->time;
					if (clip->getLen() - offset < MIN_CLIPSIZE) {
						offset = clip->getLen() - MIN_CLIPSIZE;
					}
					if (!(grid.grid_dens.getSnap() == SNAP_OFF || isAlt(evt.kbmods)) && clip->getLen() - offset < grid.getTickLength()) {
						offset = clip->getLen()-grid.getTickLength();
					}
					if (clip->clipType == CLIP_AUDIO) {
						tick_t sampleOffsetTicks = MainCtrl::get()->samplesToTicks(clip->offsetSamples);
						if (sampleOffsetTicks + offset < 0) {
							offset = -sampleOffsetTicks;
						}
					}
					clip->time += offset;
					clip->adjustLen(-offset);
					clip->adjustStartOffset(offset);
					my_printf("post clip->getLen() %d len %d samples %d\n", clip->getLen(), clip->len, clip->lenSamples);
					int32_t postLen = clip->getLen();
					dbgassert(postLen == preLen - offset);
				}
			} else {
				if (clip->end() != tick) {
					tick_t offset = clip->end() - tick;
					if (offset) {
						if (clip->getLen() - offset < MIN_CLIPSIZE) {
							offset = clip->getLen() - MIN_CLIPSIZE;
						}
						if (!(grid.grid_dens.getSnap() == SNAP_OFF || isAlt(evt.kbmods)) && clip->getLen() - offset < grid.getTickLength()) {
							offset = clip->getLen()-grid.getTickLength();
						}
						if (clip->clipType == CLIP_AUDIO) {
							tick_t sampleLen = MainCtrl::get()->samplesToTicks(clip->audio.lenSamples()-clip->offsetSamples);
							if (sampleLen > 0 && clip->getLen()-offset > sampleLen-clip->offsetStart) {
								offset = -(sampleLen-clip->offsetStart - clip->getLen());
							}
						}
						clip->adjustLen(-offset);
					}

				}
			}
			clip->setDirty();
			resizeOtherClips(track->getMidi(), clip);
			setSelectionRange(clip, track);
			updateVisibleTrackContents();
			return;
		}
	}
	dragClipboardMove(evt.relMousepos, evt.kbmods);
}

void guitrack_editor::dragClipboardMove(ivec2 local, int kbmods) {
	if (action.dragtype) {
		track_t *trNxtSelected = getTrackFromMouse(project, local, true);

		Cursor& cursor = MainCtrl::get()->cursor;
		const Cursor& cursorBegin = action.cursorBegin;
		tick_t dragMousePos = grid.screenToTick(local.x);
		tick_t dragMouseTicks = dragMousePos - dragStartTick;

		tick_t timeOffset = cursorBegin.getTickBegin();
		if (dragMouseTicks) {
			tick_t tickendExact = cursorBegin.getTickBegin()+dragMouseTicks;
			timeOffset = tickendExact;
			if (grid.grid_dens.getSnap() != SNAP_OFF && !isAlt(kbmods)) {
				std::vector<tick_t> snapPoints;
				snapPoints.reserve(5*2+1);
				tick_t len = grid.getTickLength();
				tick_t posSelStart = floor(tickendExact / (double)len);
				tick_t posOffsetSnap = floor(dragMouseTicks / (double)len);
				for (int i = -2; i <= 2; i++) {
					snapPoints.push_back(len*(posSelStart+i));
					snapPoints.push_back(cursorBegin.getTickBegin()+len*(posOffsetSnap+i));
				}
				snapPoints.push_back(cursorBegin.getTickBegin());
				std::sort(snapPoints.begin(), snapPoints.end(), [tickendExact](tick_t const &t1, tick_t const &t2) {
					return math::abs(tickendExact-t1) < math::abs(tickendExact-t2);
				});
				timeOffset = snapPoints[0];
			}
		}
		cursor.cursorPos = timeOffset;
		if (trNxtSelected) {
			cursor.cursorTrack = cursorBegin.cursorTrack + (trNxtSelected->idx - dragStartTrackIdx);
		}
	}
}
void guitrack_editor::dragSelectionRelease(gui_clip* gui, MouseEvent& evt) {
	if (action.dragtype) {
		bool showclip = true;
		if (action.dragtype == DRAG_CLIPS_MOVE || action.dragtype == DRAG_CLIPS_COPY) {
			const Cursor& cursor = MainCtrl::get()->cursor;
			const Cursor& cursorBegin = action.cursorBegin;
			selectionMoved |= cursorBegin.cursorPos != cursor.cursorPos;
			selectionMoved |= cursorBegin.cursorTrack != cursor.cursorTrack;
			ivec2 local = evt.relMousepos;
			track_t *trNxtSelected = getTrackFromMouse(project, local, true);
			if (trNxtSelected) {
				MainCtrl::get()->setSelectedTrack(trNxtSelected);
			}
			if (selectionMoved && trNxtSelected) {
				ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
				Cursor target = cursor + cursorBegin;
				int32_t trackOffset = dragStartTrackIdx - cursorBegin.cursorTrack;
				tick_t dstPos = cursor.cursorPos;
				int32_t dstTrack = trNxtSelected->idx;

				int32_t minTrack = math::min(target.getTrackBegin(), dstTrack-trackOffset);
				int32_t maxTrack = math::max(target.getTrackEnd(), dstTrack-trackOffset+(target.getTrackEnd()-target.getTrackBegin()));
				//TODO: make this more efficient: dont use a bounding box on copy
				target.cursorPos = minTrack;
				target.selTrackRange = maxTrack - minTrack;
				trackstate_t resizePreModifyState;
				project.trackList.copyTracks(target.getTrackBegin(), target.getTrackEnd(), resizePreModifyState);
				resizePreModifyState.cursor = target;

				resizePreModifyState.cursor = cursorBegin;
				std::shared_ptr<clip_clipboard> clipboard = MainCtrl::get()->copySelection(cursorBegin);
				if (!isCtrl(evt.kbmods)) {
					MainCtrl::get()->cutSelection(cursorBegin);
				}
				MainCtrl::get()->pasteClipboard(clipboard.get(), dstTrack - trackOffset, dstPos);
				updateVisibleTrackContents();
				showclip = false;
				action_modify_track* track_action = new action_modify_track("Move clips", std::move(resizePreModifyState));
				MainCtrl::get()->pushHist(track_action);
			}
		} else if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT
				|| action.dragtype == DRAG_CLIPS_RESIZE_RIGHT) {
			clip_t* clipPtr = gui->m_clip;
			ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			track_t* trackPtr = gui->m_track;
			trackdata_midi_t& midi = trackPtr->getMidi();
			midi.deleteEmptyClips(MainCtrl::get());
			if (!midi.hasClip(clipPtr)) {
				gui = NULL;
				showclip = false;
			}

			if (dragStartLayout.diff(trackPtr)) {
				action_modify_track* track_action = new action_modify_track("Resize clips", resizePreModifyState.copy());
				MainCtrl::get()->pushHist(track_action);
			}
		}
		action.dragtype = DRAG_NONE;
		if (gui && showclip)
			MainCtrl::get()->setEditClip(gui);
	}
}

bool guitrack_editor::clipDropBegin(dragdrop_midifile& clip, ivec2 mousepos, int kbmods) {
	tick_t tick = grid.screenToTickSnap(mousepos.x, SNAP_ON);
	tick_t tickExact = grid.screenToTickSnap(mousepos.x, SNAP_OFF);
	track_t *trackClicked = getTrackFromMouse(project, mousepos, false);
	if (trackClicked != NULL) {
		Cursor dragCursor;
		dragCursor.selRange = 0;
		dragCursor.selTrackRange = 0;
		dragCursor.cursorPos = tick;
		dragCursor.cursorTrack = trackClicked->idx;
		cursor = dragCursor;

		dragStartTick = tickExact;
		dragStartTrackIdx = trackClicked->idx;

		clip_clipboard* clipboard = clip.clipboard.get();
		clipboard->srcTrack = trackClicked->idx;

		action.dragtype = clip_dragtype_t::DROP_FILE_EXTERNAL;
		action.clipboard = clip.clipboard;
		action.cursorBegin = dragCursor;
		clip.isValidTarget = true;//inform higher level that we accept and process this drop attempt
		return true;
	}
	return false;
}
bool guitrack_editor::clipDropMove(dragdrop_midifile& clip, ivec2 mousepos, int kbmods) {
	if (!action.dragtype) {
		if (!clipDropBegin(clip, mousepos, kbmods))
			return false;
	}
	if (action.dragtype == clip_dragtype_t::DROP_FILE_EXTERNAL) {
		dragClipboardMove(mousepos, kbmods);
		clip.isValidTarget = true;//inform higher level that we accept and process this drop attempt
		return true;
	}
	return false;
}
bool guitrack_editor::clipDropFinal(dragdrop_midifile& clip, ivec2 mousepos, int kbmods) {
	if (action.dragtype == clip_dragtype_t::DROP_FILE_EXTERNAL) {
//			dragClipboardMove(mousepos); //TODO: maybe call move again to set final pos?

		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		track_t *trNxtSelected = getTrackFromMouse(project, mousepos, true);
		int32_t tick = grid.screenToTickSnap(mousepos.x, SNAP_ON);
		tick_t dstPos = tick;
		int32_t dstTrack = trNxtSelected->idx;
		MainCtrl::get()->pasteClipboard(action.clipboard.get(), dstTrack, dstPos);
		updateVisibleTrackContents();
		action.clipboard = NULL;
		action.dragtype = DRAG_NONE;
		clip.isValidTarget = true;//inform higher level that we accept and process this drop attempt
		return true;
	}
	return false;
}

void guitrack_editor::prerender(NVGcontext* vg) {
	for (guibase* gui : guis) {
		gui->prerender(vg);
	}
	if (action.dragtype) {

		Cursor& cursor = MainCtrl::get()->cursor;
		clip_clipboard* _clipboard = action.clipboard.get();
		for (int i = 0; _clipboard && i <= _clipboard->selTrackRange; i++) {
			track_clipboard_t* trClipboard = _clipboard->tracks[i].get();
			int32_t trackIdx = _clipboard->srcTrack + i + (cursor.cursorTrack-action.cursorBegin.cursorTrack);
			if (!project.trackList.validTrackIdx(trackIdx)) {
				continue;
			}
			trackIdx = project.trackList.clampTrackIdx(trackIdx);
			track_t* tr = project.trackList[trackIdx];
			for (auto it = trClipboard->clips.begin(); it != trClipboard->clips.end(); it++) {
				clip_t* cl = (*it).get();
				if (cl->clipType == CLIP_AUDIO) {

					ivec2 clipPos = ivec2();
					ivec2 clipSize = tr->content->size; //TODO: get rid of *tr here, figure out size before and add default fallback
					audiofile_t* audio = audiocache::getInstance()->get(cl->audio.id);
					if (!audio || !getClipPosition(grid, tr->content->size, cl, clipPos, clipSize, 0)) {
//						my_printf("release %012x from prerender() (clipped) \n", &cl->audio.waveformRef);
						waveformrender::getInstance()->release(&cl->audio.waveformRef);
//						cl->audio.waveformRef.fbId = -1;
//						cl->audio.waveformRef.rendered = false;
						continue;
					}

					clipSize.y -= (HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT * 2);
					ivec2 posClipped = clipPos;
					ivec2 sizeClipped = clipSize;
					tr->content->scissorClip(posClipped, sizeClipped);
					auto waveform = makeWaveformFromClip(project, grid, tr->content->size, cl, clipPos, clipSize, posClipped, sizeClipped);
					gui_waveform_texture_ref& waveformRef = cl->audio.waveformRef;
					if (!waveformRef.queued) {
						if (!waveformRef.rendered || waveform != waveformRef.waveform) {
							dbgassert(!waveformRef.queued);
	//						my_printf("release %012x from prerender() (refresh) \n", &waveformRef);
							waveformrender::getInstance()->release(&waveformRef);
							if (waveform.size.x > 0 && waveform.size.y > 0) {
								waveformRef.waveform = waveform;
								/*int ret = */waveformrender::getInstance()->queueUpdate(audio, &waveformRef);
							}

	//						waveformRef.fbId = ret;
	//						waveformRef.rendered = true;
						}
					}
				}

			}
		}
	}
}

void guitrack_editor::renderClip(NVGcontext* vg, track_t* tr, const clip_t* cl, tick_t offset) {
	ivec2 clipPos = ivec2();
	ivec2 clipSize = tr->content->size; //TODO: get rid of *tr here, figure out size before and add default fallback

	if (getClipPosition(grid, tr->content->size, cl, clipPos, clipSize, offset)) {
		clipPos.y += tr->content->pos.y;
		if (cl->clipType == CLIP_MIDI && tr->type == TRACK_TYPE_MIDI) {
			renderMidiClip(vg, theme, tr, cl, clipPos, clipSize);
		} else if (cl->clipType == CLIP_AUDIO && tr->type == TRACK_TYPE_AUDIO) {
			const gui_waveform_texture_ref * ptr = &cl->audio.waveformRef;
			renderAudioClip(vg, theme, tr, cl, ptr, clipPos, clipSize, clipPos, clipSize);
		}
	}
}

void guitrack_editor::renderAction(NVGcontext* vg, clip_dragaction& action) {
	Cursor& cursor = MainCtrl::get()->cursor;
	clip_clipboard* _clipboard = action.clipboard.get();
	for (int i = 0; _clipboard && i <= _clipboard->selTrackRange; i++) {
		track_clipboard_t* trClipboard = _clipboard->tracks[i].get();
		int32_t trackIdx = _clipboard->srcTrack + i + (cursor.cursorTrack-action.cursorBegin.cursorTrack);
		if (!project.trackList.validTrackIdx(trackIdx)) {
			continue;
		}
		trackIdx = project.trackList.clampTrackIdx(trackIdx);
		track_t* tr = project.trackList[trackIdx];
		for (auto it = trClipboard->clips.begin(); it != trClipboard->clips.end(); it++) {
			clip_t* cl = (*it).get();
			renderClip(vg, tr, cl, (cursor.cursorPos - _clipboard->srcPos));
		}
	}
}
void guitrack_editor::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	float w = (float)size.x;
	double bgRepeat = grid.incr_bg*2.0;
	float bgOffset = (float)fmod((double)grid.offset, bgRepeat);
	int steps_bg = (int)ceil((w + bgRepeat) / grid.incr_bg);
	float x = -bgOffset;

	//grid background
	//draw full width bright
	//then overdraw dark rects
	//drawing them zig-zag would give shimmering edges due to rounding errors
	nvgBeginPath(vg);
	nvgRect(vg, -2, 0, w+2, size.y);
	nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
	nvgFill(vg);
	for (int i = 0; i < steps_bg; i+=2)
	{
		nvgBeginPath(vg);
		nvgRect(vg, x, 0, grid.incr_bg, size.y);
		nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
		nvgFill(vg);
		x += grid.incr_bg*2.0f;
		if (x > w)
			break;
	}

	for (grid_div g : grid.gridList) {
		nvgBeginPath(vg);
		nvgMoveTo(vg, g.screenpos, 0);
		nvgLineTo(vg, g.screenpos, size.y);
		NVGcolor col;
		switch (g.color) {
		case 0:
			col = theme->getColor(GuiColor::COL_LINE_BAR);
			break;
		case 1:
			col = theme->getColor(GuiColor::COL_LINE_QRT);
			break;
		case 2:
		default:
			col = theme->getColor(GuiColor::COL_LINE_XTH);
			break;
		}
		nvgStrokeColor(vg, col);
		nvgStrokeWidth(vg, g.thickness);
		nvgStroke(vg);
	}
	ivec2 cs = getSizeContent();
	int ySplit = getPosYFirstReturnTrack(project);

	int32_t bottomHeight = cs.y-ySplit;
	if (bottomHeight > 0) {
		nvgSave(vg);
		nvgIntersectScissor(vg, 0, ySplit, cs.x, bottomHeight);
		for (track_t* g : project.tracksBottom) {
			nvgSave(vg);
			g->content->render(vg);
			nvgRestore(vg);
			for (gui_track_subtrack* g2 : g->subtracks) {
				nvgSave(vg);
				g2->render(vg);
				nvgRestore(vg);
				drawSeperator(vg, theme, g2->top()-TRACK_HEIGHT_SPACING_HALF, cs);
			}
		}
		nvgRestore(vg);
	}

	bool restore = ySplit > 0;
	if (ySplit  > 0) {
		nvgSave(vg);
		nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
		for (track_t* g : project.trackCtr) {
			nvgSave(vg);
			//content
			g->content->render(vg);
			nvgRestore(vg);
			for (gui_track_subtrack* g2 : g->subtracks) {
				nvgSave(vg);
				g2->render(vg);
				nvgRestore(vg);
				drawSeperator(vg, theme, g2->top()-TRACK_HEIGHT_SPACING_HALF, cs);
			}
		}

		if (action.dragtype) {
			nvgSave(vg);
			renderAction(vg, action);
			nvgRestore(vg);
		}
	}

//	if (dragdrop.isDragging&&dragdrop.isValidTarget) {
//		nvgSave(vg);
//		renderDragDropClip(vg, dragdrop);
//		nvgRestore(vg);
//	}

	Cursor& c = MainCtrl::get()->cursor;
	trackallcontainer_t& trackList = project.trackList;
	if (trackList.validTrackIdx(c.cursorTrack)) {
		track_t* tr = trackList[c.cursorTrack];
		if (c.selRange) {
			int32_t trackBegin = c.getTrackBegin();
			int32_t trackEnd = c.getTrackEnd();
			trackBegin = trackList.clampTrackIdx(trackBegin);
			trackEnd = trackList.clampTrackIdx(trackEnd);
			track_t* trB = trackList[trackBegin];
			track_t* trE = trackList[trackEnd];
			int32_t tickBegin = c.getTickBegin();
			int32_t tickEnd = c.getTickEnd();
//			double tickBeginX = max(-2, (int) grid.tickToScreenD(tickBegin));
//			double tickEndX = min(size.x + 2, (int) grid.tickToScreenD(tickEnd));
			double tickBeginX = grid.tickToScreenD(tickBegin);
			double tickEndX = grid.tickToScreenD(tickEnd);

			float trackYMin = math::min(trB->content->top(), trE->content->top());
			float trackYMax = math::max(trB->content->bottom(), trE->content->bottom());
			if (c.isSubtrackSelection()) {
				int32_t ssTrIdx = c.getSubTrackBegin();
				int32_t esTrIdx = c.getSubTrackEnd();
				if (trB->validSubtrack(ssTrIdx) && trB->validSubtrack(esTrIdx)) {
					trackYMin = trB->subtracks[ssTrIdx]->top();
					trackYMax = trB->subtracks[esTrIdx]->bottom();
				} else {
					dbgassert(0);
				}
			}

			if (tickEndX > -4.0 && tickBeginX < cs.x + 4.0) {
				if (indexOfCtr(project.tracksBottom, trE) > -1) {
					restore = false;
					nvgRestore(vg);
				}
				tickBeginX = CLAMP_I(tickBeginX, -4.0, cs.x + 3.0);
				tickEndX = CLAMP_I(tickEndX, -3.0, cs.x + 4.0);
				float width = (float) (tickEndX - tickBeginX);
				float height = trackYMax - trackYMin;

				nvgBeginPath(vg);
				nvgRect(vg, (float)tickBeginX, trackYMin, width, height);
				nvgFillColor(vg, G_SELECTION);
				nvgFill(vg);
			}


		} else  {
			float cursorScreenX = (float)grid.tickToScreenD(c.cursorPos);
			if (cursorScreenX >= -2 && cursorScreenX < size.x+2) {
				float trackYMin = tr->content->top();
				float trackYMax = tr->content->bottom();
				if (c.isSubtrackSelection()) {
					int32_t ssTrIdx = c.getSubTrackBegin();
					int32_t esTrIdx = c.getSubTrackEnd();
					if (tr->validSubtrack(ssTrIdx) && tr->validSubtrack(esTrIdx)) {
						trackYMin = tr->subtracks[ssTrIdx]->top();
						trackYMax = tr->subtracks[esTrIdx]->bottom();
					} else {
						dbgassert(0);
					}

				}
				cursorScreenX+=0.5;
				NVGcolor cursorColor = getCursorColor();
				nvgBeginPath(vg);
				nvgMoveTo(vg, cursorScreenX, trackYMin+1);
				nvgLineTo(vg, cursorScreenX, trackYMax-1);
				nvgStrokeColor(vg, cursorColor);
				nvgStrokeWidth(vg, 1.5f);
				nvgStroke(vg);
			}
		}
	}
	if (restore)
	nvgRestore(vg);
}
int32_t getPosYFirstReturnTrack(project_t& project) {
	track_t* lastMidi = project.trackCtr.size() ? project.trackCtr.back() : NULL;
	track_t* firstReturn = project.tracksBottom.size() ? project.tracksBottom.front() : NULL;
	if (firstReturn && firstReturn->content) {
		return firstReturn->content->top() - TRACK_HEIGHT_SPACING_HALF;
	}
	if (lastMidi && lastMidi->content) {
		return lastMidi->content->bottom() + TRACK_HEIGHT_SPACING_HALF;
	}
	return 0;
}

gui_track_subtrack *getSubTrackFromMouse(project_t& project, ivec2 mouse, bool isDragSnap) {
	int ySplit = getPosYFirstReturnTrack(project);
	const trackbasecontainer_t& tracks = mouse.y < ySplit ? project.trackCtr : project.tracksBottom;
	for (track_t *tr : tracks) {
		if (!tr->subtracks.size()) {
			continue;
		}
		for (gui_track_subtrack *atr : tr->subtracks) {
			int top = atr->top();
			int bottom = atr->bottom();
			if (mouse.y >= top && mouse.y < bottom) {
				return atr;
			}
		}
	}
	return NULL;
}
track_t *getTrackFromMouse(project_t& project, ivec2 mouse, bool isDragSnap) {
	int ySplit = getPosYFirstReturnTrack(project);
	track_t *t = NULL;
	track_t *tMin = NULL;
	double minDist = 0;
	const trackbasecontainer_t& tracks = mouse.y < ySplit ? project.trackCtr : project.tracksBottom;
	for (track_t *tr : tracks) {
		int top = tr->content->top();
		int bottom = tr->content->bottom();
		if (tMin == NULL) {
			minDist = math::min(math::abs(top - mouse.y), math::abs(bottom - mouse.y));
			tMin = tr;
		} else if (math::abs(top - mouse.y) < minDist) {
			minDist = math::abs(top - mouse.y);
			tMin = tr;
		} else if (math::abs(bottom - mouse.y) < minDist) {
			minDist = math::abs(bottom - mouse.y);
			tMin = tr;
		}
		if (mouse.y >= top && mouse.y < bottom) {
			t = tr;
			break;
		}
	}
	if (isDragSnap && (mouse.y < ySplit || !project.tracksBottom.size())) {
		if (t == NULL) {
			if (project.trackCtr.back()->content->bottom() < mouse.y) {
				t = project.trackCtr.back();
			} else if (project.trackCtr.front()->content->top() > mouse.y) {
				t = project.trackCtr.front();
			}
		}
		if (t == NULL) {
			t = tMin;
//				throw appexception("t == NULL");
		}
	}
	return t;
}

