#include "trackctr.h"
#include <utility>
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
#include "trackctr_types.h"
#include "trackcontent.h"
#include "tracktimeline.h"
#include "theme.h"
#include "guicontextmenu.h"
#include "mouse.h"
#include "mousecursor.h"
#include "logging.h"
#include "audiocache.h"
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
        desc   = std::move(description);
        before = std::move(_tracks);
    }

    void undo(DawInstance* daw) override {
        my_printf("action_modify_track undo, num tracks: %d\n", before.tracks.size());

        daw->resetMouseContext();
        daw->resetEditClip();
        bool initAfter = after.tracks.empty();
        if (initAfter) {
            after.cursor = MainCtrl::get()->getCursor();
        }
        trackallcontainer_t& trCtr = daw->getTracks();
        for (track_snapshot_t* trackStored : before.tracks) {
            my_printf("trackStored: %s %d\n", TrackTypeToName(trackStored->type), trackStored->localIdx);
            if (trCtr.validTrackTypeIdx(trackStored->type, trackStored->localIdx)) {
                track_t* track = trCtr.getTrackTypeIdx(trackStored->type, trackStored->localIdx);
                if (initAfter) {
                    after.tracks.push_back(new track_snapshot_t(track, false));
                }
                track->getMidi().deleteClips(daw);
                track->releaseTrackContent();
                my_printf("TRACKBeforeUndo[%d] HAS %d clips\n", track->projectIdx, track->getMidi().getConstClips().size());
                *track = *trackStored;
                my_printf("TRACKAfterUndo[%d] HAS %d clips\n", track->projectIdx, track->getMidi().getConstClips().size());
            } else {

                my_printf("idx is now invalid\n", 0);
            }
        }
        MainCtrl::get()->getCursor() = before.cursor;
    }
    void redo(DawInstance* daw) override {
        daw->resetMouseContext();
        daw->resetEditClip();
        trackallcontainer_t& trCtr = daw->getTracks();
        for (track_snapshot_t* trackStored : after.tracks) {
            if (trCtr.validTrackTypeIdx(trackStored->type, trackStored->localIdx)) {
                track_t* track = trCtr.getTrackTypeIdx(trackStored->type, trackStored->localIdx);
                track->getMidi().deleteClips(daw);
                track->releaseTrackContent();
                *track = *trackStored;
                my_printf("TRACK[%d] HAS %d clips\n", track->projectIdx, track->getMidi().getConstClips().size());
            }
        }
        MainCtrl::get()->getCursor() = after.cursor;
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
            cutClipLeft(c, clip->end() - c->start());
            c->setDirty();
        } else if (c->end() <= clip->end()) {
            cutClipRight(c, c->end() - clip->start());
            c->setDirty();
        } else {
            my_printf("WHUT!\n", 0);
        }
    }
}
bool guitrack_editor::handleKeyInput(KeyEvent& kevt) {
    if (kevt.type != STATE_REPEAT && isCtrlKey(kevt.keyCode)) {
        if ((action.dragtype == DRAG_CLIPS_MOVE || action.dragtype == DRAG_CLIPS_COPY)) {
            if ((action.dragtype == DRAG_CLIPS_COPY) != isCtrl(kevt.mods)) {
                if (action.dragtype == DRAG_CLIPS_MOVE) {
                    action.dragtype        = DRAG_CLIPS_COPY;
                    parentCtrl->cursorIcon = CURSOR_DUPLICATE;
                } else {
                    action.dragtype        = DRAG_CLIPS_MOVE;
                    parentCtrl->cursorIcon = CURSOR_DEFAULT;
                }
            }
            return false;
        }
    }
    if (kevt.type != STATE_REPEAT && isAltKey(kevt.keyCode)) {
//        if ((action.dragtype == DRAG_CLIPS_MOVE || action.dragtype == DRAG_CLIPS_COPY)) {
//            if ((action.dragtype == DRAG_CLIPS_COPY) != isCtrl(kevt.mods)) {
//                if (action.dragtype == DRAG_CLIPS_MOVE) {
//                    action.dragtype             = DRAG_CLIPS_COPY;
//                    MainCtrl::get()->cursorIcon = CURSOR_DUPLICATE;
//                } else {
//                    action.dragtype             = DRAG_CLIPS_MOVE;
//                    MainCtrl::get()->cursorIcon = CURSOR_DEFAULT;
//                }
//            }
//            return false;
//        }
        MainCtrl::get()->window->fireMouseMoved();
        return false;
    }
    if (action.dragtype) {
        return false;
    }
    if (kevt.type != K_RELEASE) {
        trackstate_t preModifyState;
        ThreadLock lock      = MainCtrl::getPlayThread()->lockThread();
        bool modified        = false;
        bool handledKeyinput = false;
        String desc          = "???";
        if (kevt.type == K_PRESS) {
            if (isKC(KC_SELECTALL, kevt)) {
                tick_t evtMin            = INVALID_TICK;
                tick_t evtMax            = INVALID_TICK;
                track_gui_entry_t* trMin = nullptr;
                track_gui_entry_t* trMax = nullptr;
                int idx                  = 0;
                for (track_gui_entry_t* t : iGuiMgr.getTracksVisibleFlat()) {
                    auto minMax = t->track->getMinMaxEvents();
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
                    cursor.selRange  = evtMax - evtMin;
                    cursor.setTrack(trMin->idx);
                    cursor.selTrackRange    = (trMax->idx - cursor.cursorTrack);
                    cursor.cursorSubTrack   = -1;
                    cursor.selSubTrackRange = 0;
                }
                handledKeyinput = true;
            }
            if (isKC(KC_DELETE, kevt) && cursor.getRange()) {
                int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
                int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
                project.trackList.copyTracks(idxBegin, idxEnd, preModifyState);
                preModifyState.cursor = cursor;
                DAW::cutSelection(iGuiMgr, cursor);
                handledKeyinput = true;
                modified        = true;
                desc            = "Delete clips";
            } else if (isKC(KC_CUT, kevt) && cursor.getRange()) {
                m_clipboard      = DAW::copySelection(iGuiMgr, cursor);
                int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
                int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
                project.trackList.copyTracks(idxBegin, idxEnd, preModifyState);
                preModifyState.cursor = cursor;
                DAW::cutSelection(iGuiMgr, cursor);
                handledKeyinput = true;
                modified        = true;
                desc            = "Cut clips";
            } else if (isKC(KC_MUTE, kevt) && cursor.getRange()) {
                m_clipboard      = DAW::copySelection(iGuiMgr, cursor);
                int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
                int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
                project.trackList.copyTracks(idxBegin, idxEnd, preModifyState);
                preModifyState.cursor = cursor;
                DAW::muteIntersecting(iGuiMgr, cursor);
                grid.makeTickVisible(cursor.cursorPos + cursor.selRange / 2);
                handledKeyinput = true;
                modified        = true;
                desc            = "Mute clips";
            } else if (isKC(KC_COPY, kevt) && cursor.getRange()) {
                m_clipboard     = DAW::copySelection(iGuiMgr, cursor);
                handledKeyinput = true;
            } else if (isKC(KC_CONSOLIDATE, kevt) && cursor.getRange() && !cursor.isSubtrackSelection()) {
                int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
                int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
                project.trackList.copyTracks(idxBegin, idxEnd, preModifyState);
                preModifyState.cursor = cursor;
                /* maybe do    this->clipboard = copy */
                std::shared_ptr<clip_clipboard> clipboardCopy         = DAW::copySelection(iGuiMgr, cursor);
                std::shared_ptr<clip_clipboard> clipboardConsolidated = DAW::consolidateClipboard(clipboardCopy, cursor);
                cursor.setLeftAligned();
                //cursor.cursorPos += cursor.getRange();
                DAW::cutSelection(iGuiMgr, cursor);
                DAW::pasteClipboard(iGuiMgr, clipboardConsolidated.get(), cursor);
                grid.makeTickVisible(cursor.cursorPos + clipboardConsolidated->selRange);
                handledKeyinput = true;
                modified        = true;
                desc            = "Consolidate selection";
            } else if (isKC(KC_DUPLICATE, kevt) && cursor.getRange()) {
                int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
                int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
                project.trackList.copyTracks(idxBegin, idxEnd, preModifyState);
                preModifyState.cursor = cursor;
                /* maybe do    this->clipboard = copy */
                std::shared_ptr<clip_clipboard> newClipboard = DAW::copySelection(iGuiMgr, cursor);
                cursor.setLeftAligned();
                cursor.cursorPos += cursor.getRange();
                DAW::pasteClipboard(iGuiMgr, newClipboard.get(), cursor);
                grid.makeTickVisible(cursor.cursorPos + newClipboard->selRange);
                handledKeyinput = true;
                modified        = true;
                desc            = "Duplicate clips";
            } else if (isKC(KC_PASTE, kevt) && m_clipboard) {
                DAW::Cursor pasteRange = cursor;
                track_selection_t pasteSelection;
                pasteRange.selTrackRange = m_clipboard->selTrackRange;
                iGuiMgr.getTrackSelection(pasteRange, pasteSelection);
                project.trackList.copyTracks(pasteSelection.trackIdxMin, pasteSelection.trackIdxMax, preModifyState);
                preModifyState.cursor = cursor;
                cursor.setLeftAligned();
                if (m_clipboard->type == clip_clipboard::ClipboardFull)
                    DAW::cutSelection(iGuiMgr, cursor);
                DAW::pasteClipboard(iGuiMgr, m_clipboard.get(), cursor);
                cursor.selTrackRange = m_clipboard->selTrackRange;
                cursor.selRange      = m_clipboard->selRange;
                grid.makeTickVisible(cursor.getTickEnd());
                handledKeyinput = true;
                modified        = true;
                desc            = "Paste clips";
            }
        } else {
        }
        if (isArrowKey(kevt.keyCode)) {
            ivec2 dir;
            arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
            if (dir.y) {
                if (isShift(kevt.mods)) {
                    if (cursor.isSubtrackSelection()) {
                        if (iGuiMgr.validTrackIdx(cursor.cursorTrack)) {
                            cursor.selSubTrackRange += -dir.y;
                            const track_gui_entry_t* tr = iGuiMgr.at(cursor.cursorTrack);
                            fixCursorSubRange(cursor, tr->subtracks.size());
                        }
                    } else {
                        cursor.selTrackRange += -dir.y;
                        fixCursorTrackRange(cursor, iGuiMgr.getTracksVisibleFlat().size());
                    }
                } else {

                    cursor.setLeftAligned();
                    auto moveMainCursor = [this, &dir]() {
                        cursor.setTrack(project.trackList.clampTrackIdx(cursor.cursorTrack - dir.y));
                    };
                    auto moveCursor = [this, &dir, &moveMainCursor]() {
                        if (cursor.isSubtrackSelection()) {
                            if (!iGuiMgr.validTrackIdx(cursor.cursorTrack)) {
                                cursor.setTrack(0);
                                cursor.cursorSubTrack   = -1;
                                cursor.selSubTrackRange = 0;
                                return;
                            }
                            const track_gui_entry_t* tr = iGuiMgr.at(cursor.cursorTrack);
                            cursor.cursorSubTrack -= dir.y;
                            fixCursorSubRange(cursor, tr->subtracks.size());
                            return;
                        }
                        moveMainCursor();
                    };
                    moveCursor();
                }
            } else if (dir.x) {
                tick_t tickStBfr  = cursor.getTickBegin();
                tick_t tickEndBfr = cursor.getTickEnd();
                tick_t timeOffset = dir.x * grid.getTickLength();
                if (isShift(kevt.mods)) {
                    cursor.selRange += timeOffset;
                } else {
                    cursor.selRange         = 0;
                    cursor.selTrackRange    = 0;
                    cursor.selSubTrackRange = 0;
                    cursor.cursorPos        = math::max(0, cursor.cursorPos + timeOffset);
                }
                if (tickStBfr != cursor.getTickBegin())
                    grid.makeTickVisible(cursor.getTickBegin() + timeOffset);
                if (tickEndBfr != cursor.getTickEnd())
                    grid.makeTickVisible(cursor.getTickEnd() + timeOffset);
            }
            handledKeyinput = true;
        }
        if (modified) {
            auto* track_action = new action_modify_track(desc, preModifyState.copy());// could be more efficient
            DawInstance::get()->pushHist(track_action);
        }
        if (handledKeyinput) {
            DawInstance::get()->updateVisibleTrackContents();
        }
        return handledKeyinput;
    }
    return false;
}


void guitrack_editor::trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
    ivec2 local               = evt.relMousepos;
    int32_t tick              = grid.screenToTickSnap(local.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
    track_gui_entry_t* tr     = getTrackFromMouse(iGuiMgr, local, false);
    gui_track_subtrack* subTr = getSubTrackFromMouse(iGuiMgr, local, false);
    if (subTr) {
        tr = subTr->m_trackentry;
    }
    trSelected    = tr;
    subTrSelected = subTr;
    if (trSelected != nullptr) {
        DawInstance::get()->setEditClip(nullptr);
        if (evt.guiDragged == this) {// cursor move / range select
            DAW::Cursor& c  = dawCtrl->getCursor();
            c.selRange      = 0;
            c.selTrackRange = 0;
            c.cursorPos     = tick;
            c.setTrack(trSelected->idx);
            c.cursorSubTrack   = subTrSelected ? subTrSelected->idx : -1;
            c.selSubTrackRange = 0;
        }
    }
}

void guitrack_editor::trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {
    if (trSelected != nullptr) {
        DAW::Cursor& c                   = dawCtrl->getCursor();
        ivec2 local                      = evt.relMousepos;
        track_gui_entry_t* trNxtSelected = nullptr;
        gui_track_subtrack* subTr        = getSubTrackFromMouse(iGuiMgr, local, true);
        if (subTrSelected) {
            if (subTr && subTr->m_track != subTrSelected->m_track) {
                subTr = nullptr;
            }
            trNxtSelected = getTrackFromMouse(iGuiMgr, local, false);
            if (trNxtSelected && trNxtSelected->idx < subTrSelected->m_trackentry->idx) {
                subTr = subTrSelected->m_trackentry->subtracks.front();
            }
            if (trNxtSelected && trNxtSelected->idx > subTrSelected->m_trackentry->idx) {
                subTr = subTrSelected->m_trackentry->subtracks.back();
            }
        } else {
            trNxtSelected = getTrackFromMouse(iGuiMgr, local, true);

            if (!trNxtSelected)
                return;
            //if track is folded get last child in linear layout
        }
        int32_t tick = grid.screenToTickSnap(local.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
        if (evt.guiDragged == this) {// cursor move / range select

            c.selRange = tick - c.cursorPos;
            if (c.isSubtrackSelection()) {
                //c.isSubtrackSelection() guarantees subTrSelected to be non-nullptr
                dbgassert(subTrSelected);
                if (subTr) {
                    c.selSubTrackRange = (subTr->idx - subTrSelected->idx);
                    dbgassert(c.getSubTrackEnd() > -1);
                    dbgassert(c.getSubTrackBegin() <= c.getSubTrackEnd());
                    dbgassert(c.getSubTrackBegin() < (int) subTr->m_trackentry->subtracks.size());
                    dbgassert(c.getSubTrackEnd() < (int) subTr->m_trackentry->subtracks.size());
                }

            } else {
                c.selTrackRange = (trNxtSelected->idx - trSelected->idx);
            }
//            beatbar16th_t songPos = MainCtrl::get()->toBeatBar16th(tick);
//            log_printf("Select at Track %d - %d %d %d %d = %u.%u.%u\n", trSelected->idx, c.cursorPos, tick, c.selRange, local.x, songPos.bar, songPos.beat, songPos.th);
        }
    }
}
void guitrack_editor::trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
    track_gui_entry_t* trNxtSelected = nullptr;
    ivec2 local                      = evt.relMousepos;
    trNxtSelected                    = getTrackFromMouse(iGuiMgr, local, true);
    DawInstance::get()->setSelectedTrackEntry(trNxtSelected);
    trSelected    = nullptr;
    subTrSelected = nullptr;
}
void guitrack_editor::dragSelectionBegin(gui_clip* gClip, MouseEvent& evt) {
    selectionMoved      = false;
    ivec2 local         = evt.relMousepos;
    tick_t tickExact    = grid.screenToTickSnap(local.x, SNAP_OFF);
    track_t* track      = gClip->m_track;
    clip_t* clicked     = gClip->m_clip;
    //ghostCopy = new gui_clip(clip->m_clip->clone());
    //ghostCopy->m_clip->gClip = ghostCopy;
    track_gui_entry_t* trackClicked = getTrackFromMouse(iGuiMgr, local, false);
    if (trackClicked) {
        DawInstance::get()->setSelectedTrackEntry(trackClicked);
    }

    action.dragtype  = DRAG_NONE;
    action.clipboard = nullptr;
    if (evt.mousepos.x - gClip->toScreenSpace(ivec2(0)).x < DRAG_RANGE) {
        action.dragtype = DRAG_CLIPS_RESIZE_LEFT;
    } else if (gClip->toScreenSpace(ivec2(gClip->size.x, 0)).x - evt.mousepos.x < DRAG_RANGE) {
        action.dragtype = DRAG_CLIPS_RESIZE_RIGHT;
    }
    if (action.dragtype) {
        setSelectionRange(clicked, gClip->m_trackentry);
        dragStartLayout    = track->getMidi();//copy
        action.cursorBegin = cursor;
        m_resizePreModifyState.reset();
        int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
        int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
        project.trackList.copyTracks(idxBegin, idxEnd, m_resizePreModifyState);
        m_resizePreModifyState.cursor = cursor;
        return;
    }
    if (trackClicked != nullptr) {
        if (!cursor.selRange || cursor.isSubtrackSelection() || !cursor.contains(trackClicked->idx, tickExact)) {
            setSelectionRange(clicked, trackClicked);
        }
        cursor.setLeftAligned();
        dragStartTick     = tickExact;
        dragStartTrackIdx = trackClicked->idx;
        if (isCtrl(evt.kbmods)) {
            action.dragtype        = DRAG_CLIPS_COPY;
            parentCtrl->cursorIcon = CURSOR_DUPLICATE;
        } else {
            action.dragtype = DRAG_CLIPS_MOVE;
        }
        action.cursorBegin = cursor;
        action.clipboard   = DAW::copySelection(iGuiMgr, action.cursorBegin);
    }
}
void guitrack_editor::dragSelectionMove(gui_clip* gui, MouseEvent& evt) {
    if (action.dragtype) {
        if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT || action.dragtype == DRAG_CLIPS_RESIZE_RIGHT) {
            ThreadLock lock               = MainCtrl::getPlayThread()->lockThread();
            clip_t* clip                  = gui->m_clip;
            track_gui_entry_t* trackentry = gui->m_trackentry;
            track_t* track                = trackentry->track;
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
                        offset = clip->getLen() - grid.getTickLength();
                    }
                    if (clip->clipType == CLIP_AUDIO) {
                        tick_t sampleOffsetTicks = DawInstance::get()->samplesToTicks(clip->offsetSamples);
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
                            offset = clip->getLen() - grid.getTickLength();
                        }
                        if (clip->clipType == CLIP_AUDIO) {
                            tick_t sampleLen = DawInstance::get()->samplesToTicks(clip->audio.lenSamples() - clip->offsetSamples);
                            if (sampleLen > 0 && clip->getLen() - offset > sampleLen - clip->offsetStart) {
                                offset = -(sampleLen - clip->offsetStart - clip->getLen());
                            }
                        }
                        clip->adjustLen(-offset);
                    }
                }
            }
            clip->setDirty();
            resizeOtherClips(track->getMidi(), clip);
            setSelectionRange(clip, trackentry);
            DawInstance::get()->updateVisibleTrackContents();
            return;
        }
    }
    dragClipboardMove(evt.relMousepos, evt.kbmods);
}

void guitrack_editor::dragClipboardMove(ivec2 local, int kbmods) {
    if (action.dragtype) {
        track_gui_entry_t* trNxtSelected = getTrackFromMouse(iGuiMgr, local, true);

        const DAW::Cursor& cursorBegin = action.cursorBegin;
        tick_t dragMousePos            = grid.screenToTick(local.x);
        tick_t dragMouseTicks          = dragMousePos - dragStartTick;

        tick_t timeOffset = cursorBegin.getTickBegin();
        if (dragMouseTicks) {
            tick_t tickendExact = cursorBegin.getTickBegin() + dragMouseTicks;
            timeOffset          = tickendExact;
            if (grid.grid_dens.getSnap() != SNAP_OFF && !isAlt(kbmods)) {
                std::vector<tick_t> snapPoints;
                snapPoints.reserve(5 * 2 + 1);
                tick_t len           = grid.getTickLength();
                tick_t posSelStart   = floor(tickendExact / (double) len);
                tick_t posOffsetSnap = floor(dragMouseTicks / (double) len);
                for (int i = -2; i <= 2; i++) {
                    snapPoints.push_back(len * (posSelStart + i));
                    snapPoints.push_back(cursorBegin.getTickBegin() + len * (posOffsetSnap + i));
                }
                snapPoints.push_back(cursorBegin.getTickBegin());
                std::sort(snapPoints.begin(), snapPoints.end(), [tickendExact](tick_t const& t1, tick_t const& t2) {
                    return math::abs(tickendExact - t1) < math::abs(tickendExact - t2);
                });
                timeOffset = snapPoints[0];
            }
        }
        cursor.cursorPos = timeOffset;
        if (trNxtSelected) {
            cursor.setTrack(cursorBegin.cursorTrack + (trNxtSelected->idx - dragStartTrackIdx));
        }
    }
}
void guitrack_editor::dragSelectionRelease(gui_clip* gui, MouseEvent& evt) {
    if (action.dragtype) {
        bool showclip = true;
        if (action.dragtype == DRAG_CLIPS_MOVE || action.dragtype == DRAG_CLIPS_COPY) {
            const DAW::Cursor& cursorBegin = action.cursorBegin;
            selectionMoved |= cursorBegin.cursorPos != cursor.cursorPos;
            selectionMoved |= cursorBegin.cursorTrack != cursor.cursorTrack;
            ivec2 local                      = evt.relMousepos;
            track_gui_entry_t* trNxtSelected = getTrackFromMouse(iGuiMgr, local, true);
            if (trNxtSelected) {
                DawInstance::get()->setSelectedTrackEntry(trNxtSelected);
            }
            if (selectionMoved && trNxtSelected) {
                ThreadLock lock = MainCtrl::getPlayThread()->lockThread();

                //TODO: make this more efficient: right now tracks get copied that are not modified
                DAW::Cursor allAffected = cursor.expandTo(cursorBegin);

                track_selection_t selection;
                iGuiMgr.getTrackSelection(allAffected, selection);

                int32_t trackOffset = dragStartTrackIdx - cursorBegin.cursorTrack;
                tick_t dstPos       = cursor.cursorPos;
                int32_t dstTrack    = trNxtSelected->idx;

                trackstate_t resizePreModifyState;
                project.trackList.copyTracks(selection.trackIdxMin, selection.trackIdxMax, resizePreModifyState);

                resizePreModifyState.cursor               = cursorBegin;
                std::shared_ptr<clip_clipboard> clipboard = DAW::copySelection(iGuiMgr, cursorBegin);
                if (!isCtrl(evt.kbmods)) {
                    DAW::cutSelection(iGuiMgr, cursorBegin);
                }
                int32_t trackGuiIdx = dstTrack - trackOffset;
                DAW::pasteClipboard(iGuiMgr, clipboard.get(), trackGuiIdx, dstPos);
                DawInstance::get()->updateVisibleTrackContents();
                showclip                          = false;
                auto* track_action = new action_modify_track("Move clips", std::move(resizePreModifyState));
                DawInstance::get()->pushHist(track_action);
            }
        } else if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT || action.dragtype == DRAG_CLIPS_RESIZE_RIGHT) {
            clip_t* clipPtr        = gui->m_clip;
            ThreadLock lock        = MainCtrl::getPlayThread()->lockThread();
            track_t* trackPtr      = gui->m_track;
            trackdata_midi_t& midi = trackPtr->getMidi();
            midi.deleteEmptyClips(DawInstance::get());
            if (!midi.hasClip(clipPtr)) {
                gui      = nullptr;
                showclip = false;
            }

            if (dragStartLayout.diff(trackPtr)) {
                auto* track_action = new action_modify_track("Resize clips", m_resizePreModifyState.copy());
                DawInstance::get()->pushHist(track_action);
            }
        }
        action.dragtype = DRAG_NONE;
        if (gui && showclip)
            DawInstance::get()->setEditClip(gui);
    }
}

bool guitrack_editor::clipDropBegin(dragdrop_midifile& clip, ivec2 mousepos, int kbmods) {
    tick_t tick                     = grid.screenToTickSnap(mousepos.x, SNAP_ON);
    tick_t tickExact                = grid.screenToTickSnap(mousepos.x, SNAP_OFF);
    track_gui_entry_t* trackClicked = getTrackFromMouse(iGuiMgr, mousepos, false);
    if (trackClicked != nullptr) {
        DAW::Cursor dragCursor;
        dragCursor.selRange      = 0;
        dragCursor.selTrackRange = 0;
        dragCursor.cursorPos     = tick;
        dragCursor.setTrack(trackClicked->idx);
        cursor = dragCursor;

        dragStartTick     = tickExact;
        dragStartTrackIdx = trackClicked->idx;

        clip_clipboard* clipboard = clip.clipboard.get();
        clipboard->srcTrack       = trackClicked->idx;

        action.dragtype    = clip_dragtype_t::DROP_FILE_EXTERNAL;
        action.clipboard   = clip.clipboard;
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
//        dragClipboardMove(mousepos);//TODO: maybe call move again to set final pos?

        ThreadLock lock                  = MainCtrl::getPlayThread()->lockThread();
        track_gui_entry_t* trNxtSelected = getTrackFromMouse(iGuiMgr, mousepos, true);
        int32_t tick                     = grid.screenToTickSnap(mousepos.x, SNAP_ON);
        tick_t dstPos                    = tick;
        int32_t dstTrack                 = trNxtSelected->idx;
        DAW::pasteClipboard(iGuiMgr, action.clipboard.get(), dstTrack, dstPos);
        DawInstance::get()->updateVisibleTrackContents();
        action.clipboard   = nullptr;
        action.dragtype    = DRAG_NONE;
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

        clip_clipboard* _clipboard = action.clipboard.get();
        for (int i = 0; _clipboard && i <= _clipboard->selTrackRange; i++) {
            track_clipboard_t* trClipboard = _clipboard->tracks[i].get();
            int32_t trackIdx               = _clipboard->srcTrack + i + (cursor.cursorTrack - action.cursorBegin.cursorTrack);
            if (!project.trackList.validTrackIdx(trackIdx)) {
                continue;
            }
            trackIdx    = project.trackList.clampTrackIdx(trackIdx);
            track_t* tr = project.trackList[trackIdx];
            for (auto it = trClipboard->clips.begin(); it != trClipboard->clips.end(); it++) {
                clip_t* cl = (*it).get();
                if (cl->clipType == CLIP_AUDIO) {
#ifdef TODO_AUDIO_CLIP_DRAGGED_RENDER_WAVEFORM
                    ivec2 clipPos      = ivec2();
                    ivec2 clipSize     = tr->content->size;//TODO: get rid of *tr here, figure out size before and add default fallback
                    audiofile_t* audio = audiocache::getInstance()->get(cl->audio.id);
                    if (!audio || !getClipPosition(grid, tr->content->size, cl, clipPos, clipSize, 0)) {
                        //my_printf("release %012x from prerender() (clipped) \n", &cl->audio.waveformRef);
                        waveformrender::getInstance()->release(&cl->audio.waveformRef);
                        //cl->audio.waveformRef.fbId = -1;
                        //cl->audio.waveformRef.rendered = false;
                        continue;
                    }

                    clipSize.y -= (HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT * 2);
                    ivec2 posClipped  = clipPos;
                    ivec2 sizeClipped = clipSize;
                    tr->content->scissorClip(posClipped, sizeClipped);
                    auto waveform                         = makeWaveformFromClip(project, grid, tr->content->size, cl, clipPos, clipSize, posClipped, sizeClipped);
                    gui_waveform_texture_ref& waveformRef = cl->audio.waveformRef;
                    if (!waveformRef.queued) {
                        if (!waveformRef.rendered || waveform != waveformRef.waveform) {
                            dbgassert(!waveformRef.queued);
                            //my_printf("release %012x from prerender() (refresh) \n", &waveformRef);
                            waveformrender::getInstance()->release(&waveformRef);
                            if (waveform.size.x > 0 && waveform.size.y > 0) {
                                waveformRef.waveform = waveform;
                                /*int ret = */ waveformrender::getInstance()->queueUpdate(audio, &waveformRef);
                            }

                            //waveformRef.fbId = ret;
                            //waveformRef.rendered = true;
                        }
                    }
#endif
                }
            }
        }
    }
}

void guitrack_editor::renderClip(NVGcontext* vg, const track_gui_entry_t* const entry, clip_t* cl, tick_t offset) {
    ivec2 clipPos     = ivec2();
    ivec2 scissorSize = entry->content->size;
    ivec2 clipSize    = entry->content->size;

    if (getClipPosition(grid, scissorSize, cl, clipPos, clipSize, offset)) {
        clipPos.y += entry->content->pos.y;
        if (cl->clipType == CLIP_MIDI && entry->track->type == TRACK_TYPE_MIDI) {
            renderMidiClip(vg, theme, entry, cl, clipPos, clipSize);
        } else if (cl->clipType == CLIP_AUDIO && entry->track->type == TRACK_TYPE_AUDIO) {
            static int logOnce = 0;
            if (!logOnce) {
                logOnce = 1;
                log_printf("dragged waveform rendering not implemented\n", 0);
            }
#ifdef TODO_IMPLEMENT_DRAGGED_WAVE_FORM_RENDERING
//track_gui_entry_t entry;
//dbgassert(iGuiMgr.getTrackEntry(tr, entry));
//dbgassert(entry.clipsGuis.count(cl));
//const gui_waveform_texture_ref * ptr = dynamic_cast<gui_audio_clip*>(entry.clipsGuis[cl])->waveformRef;
//renderAudioClip(vg, theme, tr, cl, ptr, clipPos, clipSize, clipPos, clipSize);
#endif
        }
    }
}

void guitrack_editor::renderAction(NVGcontext* vg, clip_dragaction& renderAction) {
    clip_clipboard* _clipboard = renderAction.clipboard.get();
    for (int i = 0; _clipboard && i <= _clipboard->selTrackRange; i++) {
        track_clipboard_t* trClipboard = _clipboard->tracks[i].get();
        int32_t trackIdx               = _clipboard->srcTrack + i + (cursor.cursorTrack - renderAction.cursorBegin.cursorTrack);
        if (!project.trackList.validTrackIdx(trackIdx)) {
            continue;
        }
        trackIdx    = project.trackList.clampTrackIdx(trackIdx);
        track_t* tr = project.trackList[trackIdx];
        track_gui_entry_t* entry;
        dbgassert(iGuiMgr.getPointerEntry(tr, &entry));
        dbgassert(entry->content != nullptr);
        for (auto it = trClipboard->clips.begin(); it != trClipboard->clips.end(); it++) {
            clip_t* cl = (*it).get();
            renderClip(vg, entry, cl, (cursor.cursorPos - _clipboard->srcPos));
        }
    }
}
void guitrack_editor::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    float w         = (float) size.x;
    double bgRepeat = grid.incr_bg * 2.0;
    float bgOffset  = (float) fmod((double) grid.offset, bgRepeat);
    int steps_bg    = math::ceildS32((w + bgRepeat) / grid.incr_bg);
    float x         = -bgOffset;
    NVGpaint paint{};
    paint.image = -1;

    /**
     * render grid background
     * steps:
     * 1. draw full width bright
     * 2. then draw dark rects
     * this way is more efficient
     * also drawing them zig-zag would give shimmering edges due to rounding errors
     */

    /* draw full width bright */
    nvgBeginPath(vg);
    nvgRect(vg, -2, 0, w + 2, size.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
    nvgFill(vg);

    /* draw dark grid areas */
    int nRendered = 0;
    for (int i = 0; i < steps_bg; i += 2) {
        nvgBatchedRect(vg, x, 0, grid.incr_bg, size.y);
        nRendered++;
        x += grid.incr_bg * 2.0f;
        if (x > w)
            break;
    }

    if (nRendered) {
        paint.innerColor = theme->getColor(GuiColor::COL_GRID_DRK);
        paint.customPar  = 1;
        nvgFillPaint(vg, paint);
        nvgBatchedRender(vg);
    }


    /* draw grid lines: bar, beat, xth */
    const float renderOffsetGrid = 0.0f;// for debugging AA
    bool hasMorePasses           = true;
    for (int pass = 0; hasMorePasses && pass < 3; ++pass) {
        hasMorePasses = false;
        nRendered = 0;
        for (grid_div& g : grid.gridList) {
            if (g.color == pass) {
                float lineThickness = 4.0f;
                nvgBatchedRect(vg, g.screenpos - lineThickness * 0.5f + renderOffsetGrid, 0, lineThickness, size.y);
                paint.feather = g.thickness;
                nRendered++;
            } else {
                hasMorePasses |= g.color > pass;
            }
        }
        if (nRendered) {
            switch (pass) {
                case 0:
                    paint.innerColor = theme->getColor(GuiColor::COL_LINE_BAR);
                    break;
                case 1:
                    paint.innerColor = theme->getColor(GuiColor::COL_LINE_QRT);
                    break;
                case 2:
                default:
                    paint.innerColor = theme->getColor(GuiColor::COL_LINE_XTH);
                    break;
            }
            paint.customPar = 2;
            nvgFillPaint(vg, paint);
            nvgBatchedRender(vg);
        }
    }

    /* draw master track and return track contents */
    const ivec2 cs       = getSizeContent();
    const int ySplit     = getPosYFirstReturnTrack(iGuiMgr.getTracksVisibleFlat());
    int32_t bottomHeight = cs.y - ySplit;
    if (bottomHeight > 0) {
        nvgSave(vg);
        nvgIntersectScissor(vg, 0, ySplit, cs.x, bottomHeight);
        for (track_t* g : project.tracksBottom.tracksFlat) {
            track_gui_entry_t* entry;
            if (!iGuiMgr.getPointerEntry(g, &entry)) {
                dbgassert(0);
                continue;
            }
            bool trackVisible = iGuiMgr.isVisible(entry);
            dbgassert(entry->content->isVisible() == trackVisible);
            if (entry->content->isVisible()) {
                nvgSave(vg);
                entry->content->render(vg);
                nvgRestore(vg);
                for (gui_track_subtrack* g2 : entry->subtracks) {
                    nvgSave(vg);
                    g2->render(vg);
                    nvgRestore(vg);
                    drawSeperator(vg, theme, g2->top() - TRACK_HEIGHT_SPACING_HALF, cs);
                }
            }
        }
        nvgRestore(vg);
    }

    /* draw audio/midi track track contents */
    bool restore = ySplit > 0;
    if (ySplit > 0) {
        nvgSave(vg);
        nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
        for (track_t* t : project.trackMidiAudioCtr.tracksFlat) {
            track_gui_entry_t* entry;
            dbgassert(iGuiMgr.getPointerEntry(t, &entry));
            dbgassert(entry->content != nullptr);
            auto totalHeight = entry->content->size.y;
            if (!entry->subtracks.empty()) {
                totalHeight = entry->subtracks.back()->bottom() - entry->content->top();
            }
            auto contentPos = entry->content->pos.y;
            if (entry->content->isVisible() && contentPos < ySplit && contentPos + totalHeight > 0) {
                nvgSave(vg);
                entry->content->render(vg);
                nvgRestore(vg);
                for (gui_track_subtrack* g2 : entry->subtracks) {
                    nvgSave(vg);
                    g2->render(vg);
                    nvgRestore(vg);
                    drawSeperator(vg, theme, g2->top() - TRACK_HEIGHT_SPACING_HALF, cs);
                }
            }
        }
        if (action.dragtype) {
            nvgSave(vg);
            renderAction(vg, action);
            nvgRestore(vg);
        }
    }

//    if (dragdrop.isDragging && dragdrop.isValidTarget) {
//        nvgSave(vg);
//        renderDragDropClip(vg, dragdrop);
//        nvgRestore(vg);
//    }

    /* draw selection overlay and track content cursor */
    DAW::Cursor& c = dawCtrl->getCursor();
    if (iGuiMgr.validTrackIdx(c.cursorTrack)) {
        const track_gui_entry_t* trEntry = iGuiMgr.at(c.cursorTrack);
        if (c.selRange) {
            int32_t trackBegin                = c.getTrackBegin();
            int32_t trackEnd                  = c.getTrackEnd();
            trackBegin                        = iGuiMgr.clampTrackIdx(trackBegin);
            trackEnd                          = iGuiMgr.clampTrackIdx(trackEnd);
            const track_gui_entry_t* trBEntry = iGuiMgr.at(trackBegin);
            const track_gui_entry_t* trEEntry = iGuiMgr.at(trackEnd);
            int32_t tickBegin                 = c.getTickBegin();
            int32_t tickEnd                   = c.getTickEnd();
            //double tickBeginX = max(-2, (int) grid.tickToScreenD(tickBegin));
            //double tickEndX = min(size.x + 2, (int) grid.tickToScreenD(tickEnd));
            double tickBeginX = grid.tickToScreenD(tickBegin);
            double tickEndX   = grid.tickToScreenD(tickEnd);
            float trackYMin   = math::min(trBEntry->content->top(), trEEntry->content->top());
            float trackYMax   = math::max(trBEntry->content->bottom(), trEEntry->content->bottom());
            if (c.isSubtrackSelection()) {
                int32_t ssTrIdx = c.getSubTrackBegin();
                int32_t esTrIdx = c.getSubTrackEnd();
                if (trBEntry->validSubtrack(ssTrIdx) && trBEntry->validSubtrack(esTrIdx)) {
                    trackYMin = trBEntry->subtracks[ssTrIdx]->top();
                    trackYMax = trBEntry->subtracks[esTrIdx]->bottom();
                } else {
                    dbgassert(0);
                }
            }

            if (tickEndX > -4.0 && tickBeginX < cs.x + 4.0) {
                if (TRACK_CTR_MIDIAUDIO != TRACKTYPE_TO_CTR(trEEntry->track->type)) {
                    restore = false;
                    nvgRestore(vg);
                }
                tickBeginX   = CLAMP_I(tickBeginX, -4.0, cs.x + 3.0);
                tickEndX     = CLAMP_I(tickEndX, -3.0, cs.x + 4.0);
                float width  = (float) (tickEndX - tickBeginX);
                float height = trackYMax - trackYMin;

                nvgBeginPath(vg);
                nvgRect(vg, (float) tickBeginX, trackYMin, width, height);
                nvgFillColor(vg, G_SELECTION);
                nvgFill(vg);
            }


        } else {
            float cursorScreenX = (float) grid.tickToScreenD(c.cursorPos);
            if (cursorScreenX >= -2 && cursorScreenX < size.x + 2) {
                float trackYMin = trEntry->content->top();
                float trackYMax = trEntry->content->bottom();
                if (c.isSubtrackSelection()) {
                    int32_t ssTrIdx = c.getSubTrackBegin();
                    int32_t esTrIdx = c.getSubTrackEnd();
                    if (trEntry->validSubtrack(ssTrIdx) && trEntry->validSubtrack(esTrIdx)) {
                        trackYMin = trEntry->subtracks[ssTrIdx]->top();
                        trackYMax = trEntry->subtracks[esTrIdx]->bottom();
                    } else {
                        dbgassert(0);
                    }
                }
                cursorScreenX += 0.5;
                NVGcolor cursorColor = getCursorColor();
                nvgBeginPath(vg);
                nvgMoveTo(vg, cursorScreenX, trackYMin + 1);
                nvgLineTo(vg, cursorScreenX, trackYMax - 1);
                nvgStrokeColor(vg, cursorColor);
                nvgStrokeWidth(vg, 1.5f);
                nvgStroke(vg);
            }
        }
    }
    if (restore) {
        nvgRestore(vg);
    }
}
int32_t getPosYFirstReturnTrack(const track_gui_vector_td& tracksVisibleFlat) {
    track_gui_entry_t* trLastVisible = nullptr;
    track_gui_entry_t* trFirstReturn = nullptr;
    for (track_gui_entry_t* trEntry : tracksVisibleFlat) {
        auto trackTypeContainer = TRACKTYPE_TO_CTR(trEntry->track->type);
        switch (trackTypeContainer) {
            case TRACK_CTR_MIDIAUDIO:
                trLastVisible = trEntry;
                break;
            case TRACK_CTR_RETURN:
            case TRACK_CTR_MASTER:
                if (!trFirstReturn) {
                    trFirstReturn = trEntry;
                }
                break;
            default:
                break;
        }
    }
    if (trFirstReturn && trFirstReturn->content) {
        return trFirstReturn->content->top() - TRACK_HEIGHT_SPACING_HALF;
    }
    if (trLastVisible && trLastVisible->content) {
        return trLastVisible->content->bottom() + TRACK_HEIGHT_SPACING_HALF;
    }
    return 0;
}
//track_gui_entry_t
gui_track_subtrack* getSubTrackFromMouse(track_gui_manager_i& iGuiMgr, ivec2 mouse, bool isDragSnap) {
    for (auto* tr : iGuiMgr.getTracksVisibleFlat()) {
        if (tr->subtracks.empty()) {
            continue;
        }
        for (gui_track_subtrack* atr : tr->subtracks) {
            int top    = atr->top();
            int bottom = atr->bottom();
            if (mouse.y >= top && mouse.y < bottom) {
                return atr;
            }
        }
    }
    return nullptr;
}
track_gui_entry_t* getTrackFromMouse(track_gui_manager_i& iGuiMgr, ivec2 mouse, bool isDragSnap) {
    track_gui_entry_t* trackInside    = nullptr;
    track_gui_entry_t* trackClosest   = nullptr;
    double minDist                    = 0;
    const track_gui_vector_td& tracks = iGuiMgr.getTracksVisibleFlat();
    for (track_gui_entry_t* tr : tracks) {
        if (!tr->content->isVisible()) {
            log_printf("track %s is not visible but is in in tracksVisibleFlat\n", StringAsCStr(tr->track->name));
            continue;
        }
        dbgassert(tr->content->isVisible());

        int top    = tr->content->top();
        int bottom = tr->content->bottom();
        if (trackClosest == nullptr) {
            minDist      = math::min(math::abs(top - mouse.y), math::abs(bottom - mouse.y));
            trackClosest = tr;
        } else if (math::abs(top - mouse.y) < minDist) {
            minDist      = math::abs(top - mouse.y);
            trackClosest = tr;
        } else if (math::abs(bottom - mouse.y) < minDist) {
            minDist      = math::abs(bottom - mouse.y);
            trackClosest = tr;
        }
        if (mouse.y >= top && mouse.y < bottom) {
            trackInside = tr;
            break;
        }
    }
    if (isDragSnap && !trackInside) {
        return trackClosest;
    }
    return trackInside;
}
