#include "trackctr.h"
#include <stdbool.h>
#include <stdint.h>
#include <vector>
#include "exceptions.h"
#include "seq_util.h"
#include "color_util.h"
#include "seq_math.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "guicontainer.h"
#include "trackcontent.h"
#include "tracktimeline.h"
#include "guicontextmenu.h"
#include "mouse.h"
#include "logging.h"

class action_modify_track : public action_base {
protected:
	trackstate_t before;
	trackstate_t after;
public:
	action_modify_track() : action_base() {
	}
	action_modify_track(String description, trackstate_t _tracks) : action_base() {
		desc = description;
		before = _tracks;//std::move(_tracks);
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
		for (track_t* trackStored : before.tracks) {
			my_printf("trackStored: %d\n", trackStored->idx);
			if (trCtr.validTrackIdx(trackStored->idx)) {
				track_t* track = trCtr[trackStored->idx];
				if (initAfter)
					after.tracks.push_back(new track_t(*track));
				track->releaseTrackContent();
				my_printf("TRACKBeforeUndo[%d] HAS %d clips\n", track->idx, track->clips.size());
				*track = *trackStored;
				my_printf("TRACKAfterUndo[%d] HAS %d clips\n", track->idx, track->clips.size());
			} else {

				my_printf("idx is now invalid\n",0);
			}
		}
		ctrl->cursor = before.cursor;
		ctrl->updateVisibleTrackContents();
	}
	void redo(MainCtrl* ctrl) {
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		trackallcontainer_t& trCtr = ctrl->getTracks();
		for (track_t* trackStored : after.tracks) {
			if (trCtr.validTrackIdx(trackStored->idx)) {
				track_t* track = trCtr[trackStored->idx];
				track->releaseTrackContent();
				*track = *trackStored;
				my_printf("TRACK[%d] HAS %d clips\n", track->idx, track->clips.size());
			}
		}
		ctrl->cursor = after.cursor;
		ctrl->updateVisibleTrackContents();
	}
};

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
	if (action.dragtype) {
		return false;
	}
	if (kevt.type != K_RELEASE) {
		MainCtrl* ctrl = MainCtrl::get();
		bool modified = false;
		bool handledKeyinput = false;
		String desc = "???";
		if (kevt.type == K_PRESS) {
			if (isKC(KC_SELECTALL, kevt)) {
				clip_t* min = NULL;
				clip_t* max = NULL;
				track_t* trMin = NULL;
				track_t* trMax = NULL;
				int idx = 0;
				for (track_t* t: project.trackList) {
					auto minMax = t->getMinMax();
					if (minMax.first)
						if (min == NULL || min->start() > minMax.first->start()) {
							min = minMax.first;
						}
					if (minMax.second)
						if (max == NULL || max->end() < minMax.second->end()) {
							max = minMax.second;
						}
					if (minMax.first) {// if any content
						if (!trMin || trMin->idx > t->idx) {
							trMin = t;
						}
						if (!trMax || trMax->idx < t->idx) {
							trMax = t;
						}
					}
					idx++;
				}
				if (min && max) {
					cursor.cursorPos = min->start();
					cursor.selRange = max->end()-cursor.cursorPos;
					cursor.cursorTrack = trMin->idx;
					cursor.selTrackRange = (trMax->idx - cursor.cursorTrack);
				}
				handledKeyinput = true;
			}
			if (isKC(KC_DELETE, kevt) && cursor.getRange()) {
//				for (track_t* t: trCtr) {
//					if (cursor.inTrackRange(t->idx)) {
//						ctrl->cutIntersecting(t, cursor.getTickBegin(), cursor.getTickEnd());
//					}
//				}.reserve(_tracks.size());
				project.trackList.copyTracks(cursor.getTrackBegin(), cursor.getTrackEnd(), resizePreModifyState);
				resizePreModifyState.cursor = cursor;
				MainCtrl::get()->cutSelection(cursor);
				handledKeyinput = true;
				modified = true;
				desc = "Delete clips";
			}
			else if (isKC(KC_CUT, kevt) && cursor.getRange()) {
				clipboard = MainCtrl::get()->copySelection(cursor);
				project.trackList.copyTracks(cursor.getTrackBegin(), cursor.getTrackEnd(), resizePreModifyState);
				resizePreModifyState.cursor = cursor;
				MainCtrl::get()->cutSelection(cursor);
				for (track_t* t: project.trackCtr) {
					if (cursor.inTrackRange(t->idx)) {
						ctrl->cutIntersecting(t, cursor.getTickBegin(), cursor.getTickEnd());
					}
				}
				handledKeyinput = true;
				modified = true;
				desc = "Cut clips";
			}
			else if (isKC(KC_COPY, kevt) && cursor.getRange()) {
				clipboard = MainCtrl::get()->copySelection(cursor);
				handledKeyinput = true;
			}
			else if (isKC(KC_DUPLICATE, kevt) && cursor.getRange()) {
				project.trackList.copyTracks(cursor.getTrackBegin(), cursor.getTrackEnd(), resizePreModifyState);
				resizePreModifyState.cursor = cursor;
				clip_clipboard* clipboard = MainCtrl::get()->copySelection(cursor);
				cursor.setLeftAligned();
				cursor.cursorPos += cursor.getRange();
				MainCtrl::get()->pasteClipboard(clipboard,
						cursor.cursorTrack,
						cursor.cursorPos);
				grid.makeTickVisible(cursor.cursorPos+clipboard->selRange);
				delete clipboard;
				handledKeyinput = true;
				modified = true;
				desc = "Duplicate clips";
			}
			else if (isKC(KC_PASTE, kevt) && clipboard) {
				project.trackList.copyTracks(clipboard->srcTrack, clipboard->srcTrack+clipboard->selTrackRange, resizePreModifyState);
				resizePreModifyState.cursor = cursor;
				cursor.setLeftAligned();
				MainCtrl::get()->cutSelection(cursor);
				MainCtrl::get()->pasteClipboard(clipboard,
						cursor.cursorTrack,
						cursor.getTickBegin());
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
				cursor.selRange = 0;
				cursor.selTrackRange = 0;
				cursor.cursorTrack = max(0, min((int32_t)(project.trackList.size() - 1), cursor.cursorTrack + dir.y));
			} else if (dir.x) {
				tick_t timeOffset = dir.x*grid.getTickLength();
				cursor.selRange = 0;
				cursor.selTrackRange = 0;
				cursor.cursorPos = max(0, cursor.cursorPos + timeOffset);
			}
			handledKeyinput = true;
//			desc = "Move notes";
		}
		if (modified) {
			action_modify_track* track_action = new action_modify_track(desc, resizePreModifyState.get());
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
	int32_t tick = grid.screenToTickSnap(local.x, SNAP_ON);
	trSelected = getTrackFromMouse(project, local, false);
	if (trSelected != NULL) {
		MainCtrl::get()->setSelectedTrack(trSelected);
		MainCtrl::get()->setEditClip(NULL);
		if (evt.guiDragged == this) { // cursor move / range select
			//beatbar16th_t songPos = MainCtrl::get()->toBeatBar16th(tick);
			//my_printf("Click at Track %d - %u = %u.%u.%u\n", trSelected->idx, tick, songPos.bar, songPos.beat, songPos.th);
			Cursor& c = MainCtrl::get()->cursor;
			c.selRange = 0;
			c.selTrackRange = 0;
			c.cursorPos = tick;
			c.cursorTrack = trSelected->idx;
		}
	}
}

void guitrack_editor::trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {
	if (trSelected != NULL) {
		ivec2 local = evt.relMousepos;
		track_t *trNxtSelected = getTrackFromMouse(project, local, true);
		if (!trNxtSelected)
			return;
		MainCtrl::get()->setSelectedTrack(trNxtSelected);
		int32_t tick = grid.screenToTickSnap(local.x, SNAP_ON);
		if (evt.guiDragged == this) { // cursor move / range select
			Cursor& c = MainCtrl::get()->cursor;

			c.selRange = tick - c.cursorPos;
			c.selTrackRange = (trNxtSelected->idx - trSelected->idx);
//			beatbar16th_t songPos = MainCtrl::get()->toBeatBar16th(tick);
			//my_printf("Select at Track %d - %d %d %d %d = %u.%u.%u\n", trSelected->idx, c.cursorPos, tick, c.selRange, local.x, songPos.bar, songPos.beat, songPos.th);

		}
	}
}
void guitrack_editor::trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
	trSelected = NULL;
}
void guitrack_editor::dragSelectionBegin(gui_clip* gClip, MouseEvent& evt) {
	selectionMoved = false;
	ivec2 local = evt.relMousepos;
	tick_t tickExact = grid.screenToTickSnap(local.x, SNAP_OFF);
	Cursor& cursor = MainCtrl::get()->cursor;
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
		setSelectionRange(clicked, clicked->tr);
		dragStartLayout = *(track_t*) clicked->tr;
		action.cursorBegin = cursor;
		resizePreModifyState.reset();
		project.trackList.copyTracks(cursor.getTrackBegin(), cursor.getTrackEnd(), resizePreModifyState);
		resizePreModifyState.cursor = cursor;
		return;
	}
	if (trackClicked != NULL) {
		if (!cursor.selRange
				|| tickExact < cursor.getTickBegin()
				|| tickExact >= cursor.getTickEnd()
				|| trackClicked->idx < cursor.getTrackBegin()
				|| trackClicked->idx > cursor.getTrackEnd()) {
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
void guitrack_editor::resizeOtherClips(track_t* tr, clip_t* clip) {
	for (clip_t* c : tr->clips) {
		if (c == clip)
			continue;
		if (c->start() >= clip->end() || c->end() <= clip->start()) {
			continue;
		}
		if (c->start() >= clip->start() && c->end() <= clip->end()) {
			c->len = 0;
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
void guitrack_editor::dragSelectionMove(gui_clip* gui, MouseEvent& evt) {
	if (action.dragtype) {
		if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT
				|| action.dragtype == DRAG_CLIPS_RESIZE_RIGHT) {
			clip_t* clip = gui->m_clip;
			dragStartLayout.apply(clip->tr);
			int32_t tick = grid.screenToTickSnap(evt.relMousepos.x, SNAP_ON);
			if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT) {
				if (clip->start() != tick) {
					tick_t offset = tick - clip->time;
					if (clip->len - offset > MIN_CLIPSIZE) {
						clip->adjustStartOffset(offset);
						clip->time += offset;
						clip->len -= offset;
					}
				}
			} else {
				if (clip->end() != tick) {
					tick_t offset = clip->end() - tick;
					if (clip->len - offset > MIN_CLIPSIZE && clip->len - offset >= grid.getTickLength()) {
						clip->len -= offset;
					}
				}
			}
			clip->setDirty();
			resizeOtherClips(clip->tr, clip);
			setSelectionRange(clip, clip->tr);
			updateVisibleTrackContents();
			return;
		}
	}
	dragClipboardMove(evt.relMousepos);
}

void guitrack_editor::dragClipboardMove(ivec2 local) {
	if (action.dragtype) {
		track_t *trNxtSelected = getTrackFromMouse(project, local, true);

		Cursor& cursor = MainCtrl::get()->cursor;
		const Cursor& cursorBegin = action.cursorBegin;
		tick_t dragMousePos = grid.screenToTick(local.x);
		tick_t dragMouseTicks = dragMousePos - dragStartTick;

		tick_t timeOffset = cursorBegin.getTickBegin();
		if (grid.grid_dens.getSnap() != SNAP_OFF) {
			if (dragMouseTicks) {
				tick_t tickendExact = cursorBegin.getTickBegin()+dragMouseTicks;
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
					return abs(tickendExact-t1) < abs(tickendExact-t2);
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
			DELETE_PTR(action.clipboard);
			if (selectionMoved) {
				Cursor target = cursor + cursorBegin;
				int32_t trackOffset = dragStartTrackIdx - cursorBegin.cursorTrack;
				tick_t dstPos = cursor.cursorPos;
				int32_t dstTrack = trNxtSelected->idx;

				int32_t minTrack = min (target.getTrackBegin(), dstTrack-trackOffset);
				int32_t maxTrack = max (target.getTrackEnd(), dstTrack-trackOffset+(target.getTrackEnd()-target.getTrackBegin()));
				//TODO: make this more efficient: dont use a bounding box on copy
				target.cursorPos = minTrack;
				target.selTrackRange = maxTrack - minTrack;
				trackstate_t resizePreModifyState;
				project.trackList.copyTracks(target.getTrackBegin(), target.getTrackEnd(), resizePreModifyState);
				resizePreModifyState.cursor = target;

				resizePreModifyState.cursor = cursorBegin;
				clip_clipboard* clipboard = MainCtrl::get()->copySelection(cursorBegin);
				if (!isCtrl(evt.kbmods)) {
					MainCtrl::get()->cutSelection(cursorBegin);
				}
				MainCtrl::get()->pasteClipboard(clipboard, dstTrack - trackOffset, dstPos);
				updateVisibleTrackContents();
				DELETE_PTR(clipboard);
				showclip = false;
				action_modify_track* track_action = new action_modify_track("Move clips", resizePreModifyState.get());
				MainCtrl::get()->pushHist(track_action);
			}
		} else if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT
				|| action.dragtype == DRAG_CLIPS_RESIZE_RIGHT) {
			clip_t* clipPtr = gui->m_clip;
			track_t* track = gui->m_clip->tr;
			track->deleteEmptyClips();
			if (!track->hasClip(clipPtr)) {
				gui = NULL;
				showclip = false;
			}

			if (dragStartLayout.diff(track)) {
				action_modify_track* track_action = new action_modify_track("Resize clips", resizePreModifyState.get());
				MainCtrl::get()->pushHist(track_action);
			}
		}
		action.dragtype = DRAG_NONE;
		if (gui && showclip)
			MainCtrl::get()->setEditClip(gui);
	}
}

bool guitrack_editor::clipDropBegin(dragdrop_midifile& clip, ivec2 mousepos) {
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

		trackcontents_t& trackcontent = clip.content;
		clip_clipboard* clipboard = new clip_clipboard();
		clipboard->srcPos = 0;
		clipboard->srcTrack = trackClicked->idx;
		clipboard->selRange = trackcontent.end()-trackcontent.start();
		clipboard->selTrackRange = 0;
		//Watch out!! clip.content will get freed from multiple places
		//TODO: copy the contents
		clipboard->tracks.push_back(&clip.content);

		action.dragtype = clip_dragtype_t::DROP_FILE_EXTERNAL;
		action.clipboard = clipboard;
		action.cursorBegin = dragCursor;
		clip.isValidTarget = true;//inform higher level that we accept and process this drop attempt
		return true;
	}
	return false;
}
bool guitrack_editor::clipDropMove(dragdrop_midifile& clip, ivec2 mousepos) {
	if (!action.dragtype) {
		if (!clipDropBegin(clip, mousepos))
			return false;
	}
	if (action.dragtype == clip_dragtype_t::DROP_FILE_EXTERNAL) {
		dragClipboardMove(mousepos);
		clip.isValidTarget = true;//inform higher level that we accept and process this drop attempt
		return true;
	}
	return false;
}
bool guitrack_editor::clipDropFinal(dragdrop_midifile& clip, ivec2 mousepos) {
	if (action.dragtype == clip_dragtype_t::DROP_FILE_EXTERNAL) {
//			dragClipboardMove(mousepos); //TODO: maybe call move again to set final pos?

		track_t *trNxtSelected = getTrackFromMouse(project, mousepos, true);
		int32_t tick = grid.screenToTickSnap(mousepos.x, SNAP_ON);
		tick_t dstPos = tick;
		int32_t dstTrack = trNxtSelected->idx;
		clip_clipboard* clipboard = action.clipboard;
		MainCtrl::get()->pasteClipboard(clipboard, dstTrack, dstPos);
		clipboard->tracks.clear();
		updateVisibleTrackContents();
		action.clipboard = NULL; // DONT FREE; we dont own!
		action.dragtype = DRAG_NONE;
		clip.isValidTarget = true;//inform higher level that we accept and process this drop attempt
		return true;
	}
	return false;
}

void guitrack_editor::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(-1), evt.mousepos);
}

void guitrack_editor::renderClip(NVGcontext* vg, track_t* tr, const clip_t* cl, tick_t offset) {
	ivec2 clipPos = ivec2();
	ivec2 clipSize = tr->content->size; //TODO: get rid of *tr here, figure out size before and add default fallback
	gui_clip::getClipPosition(grid, cl, clipPos, clipSize, offset);
	clipPos.y += tr->content->pos.y;
	gui_clip::renderClip(vg, cl, clipPos, clipSize);
}

void guitrack_editor::renderAction(NVGcontext* vg, clip_dragaction& action) {
	Cursor& cursor = MainCtrl::get()->cursor;
	clip_clipboard* _clipboard = action.clipboard;
	for (int i = 0; _clipboard && i <= _clipboard->selTrackRange; i++) {
		trackcontents_t* trClipboard = _clipboard->tracks[i];
		int32_t trackIdx = _clipboard->srcTrack + i + (cursor.cursorTrack-action.cursorBegin.cursorTrack);
		if (!project.trackList.validTrackIdx(trackIdx)) {
			continue;
		}
		trackIdx = project.trackList.clampTrackIdx(trackIdx);
		track_t* tr = project.trackList[trackIdx];
		if (tr->type == TRACK_TYPE_MIDI) {
			for (auto it = trClipboard->clips.begin(); it != trClipboard->clips.end(); it++) {
				clip_t* cl = *it;
				renderClip(vg, tr, cl, (cursor.cursorPos - _clipboard->srcPos));
			}
		}
	}
}
void guitrack_editor::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	float w = (float)size.x;
	float bgRepeat = grid.incr_bg*2.0f;
	float bgOffset = (float)std::fmod(grid.offset, bgRepeat);
	int steps_bg = (int)ceil((w + bgRepeat) / grid.incr_bg);
	float x = -bgOffset;

	//grid background
	//draw full width bright
	//then overdraw dark rects
	//drawing them zig-zag would give shimmering edges due to rounding errors
	nvgBeginPath(vg);
	nvgRect(vg, -2, 0, w+2, size.y);
	nvgFillColor(vg, g_guiColors[COL_GRID_BRT]);
	nvgFill(vg);
	for (int i = 0; i < steps_bg; i+=2)
	{
		nvgBeginPath(vg);
		nvgRect(vg, x, 0, grid.incr_bg, size.y);
		nvgFillColor(vg, g_guiColors[COL_GRID_DRK]);
		nvgFill(vg);
		x += grid.incr_bg*2.0f;
		if (x > w)
			break;
	}

	for (grid_div g : grid.gridList) {
		nvgBeginPath(vg);
		nvgMoveTo(vg, g.screenpos, 0);
		nvgLineTo(vg, g.screenpos, size.y);
		nvgStrokeColor(vg, g_guiColors[COL_LINE_BAR + g.color]);
		nvgStrokeWidth(vg, g.thickness);
		nvgStroke(vg);
	}
	ivec2 cs = getSizeContent();
	int ySplit = getPosYFirstReturnTrack(project);

	nvgSave(vg);
	nvgIntersectScissor(vg, 0, ySplit, cs.x, cs.y-ySplit);
	for (track_t* g : project.tracksBottom) {
		nvgSave(vg);
		g->content->render(vg);
		nvgRestore(vg);
	}
	nvgRestore(vg);

	bool restore = ySplit > 0;
	if (ySplit  > 0) {
		nvgSave(vg);
		nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
		for (track_t* g : project.trackCtr) {
			nvgSave(vg);
			//content
			g->content->render(vg);
			nvgRestore(vg);
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
			double tickBeginX = max(-2, (int) grid.tickToScreenD(tickBegin));
			double tickEndX = min(size.x + 2, (int) grid.tickToScreenD(tickEnd));

			float trackYMin = min(trB->content->top(), trE->content->top());
			float trackYMax = max(trB->content->bottom(), trE->content->bottom());

			if (tickEndX > -4.0f && tickBeginX < cs.x + 4.0f) {
				if (indexOf(project.tracksBottom, trE) > -1) {
					restore = false;
					nvgRestore(vg);
				}
				tickBeginX = CLAMP_I(tickBeginX, -4.0f, cs.x + 3.0f);
				tickEndX = CLAMP_I(tickEndX, -3.0f, cs.x + 4.0f);
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
				cursorScreenX+=0.5;
				NVGcolor cursorColor = getCursorColor();
				nvgBeginPath(vg);
				nvgMoveTo(vg, cursorScreenX, tr->content->pos.y+1);
				nvgLineTo(vg, cursorScreenX, tr->content->pos.y+tr->content->size.y-1);
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
		return firstReturn->content->top() - TRACK_HEIGHT_SPACING / 2;
	}
	if (lastMidi && lastMidi->content) {
		return lastMidi->content->bottom() + TRACK_HEIGHT_SPACING / 2;
	}
	return 0;
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
			minDist = min(abs(top - mouse.y), abs(bottom - mouse.y));
			tMin = tr;
		} else if (abs(top - mouse.y) < minDist) {
			minDist = abs(top - mouse.y);
			tMin = tr;
		} else if (abs(bottom - mouse.y) < minDist) {
			minDist = abs(bottom - mouse.y);
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

