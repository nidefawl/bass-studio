#include "appconfig.h"
#include "assert_dbg.h"
#include "host/audiosample.h"
#include "commands.h"
#include "compiler.h"
#include "event.h"
#include "gui/gui.h"
#include "host/daw/daw_async_task.h"
#include "seq_time.h"
#include "tls.h"
#include "host/track/track_types.h"
#include "trackctr.h"
#include <cstdint>
#include <utility>
#include <vector>
#include "exceptions.h"
#include "seq_util.h"
#include "str_util.h"
#include "color_util.h"
#include "math/seq_math.h"
#include "host/track/track.h"
#include "host/clip/clip.h"
#include "host/daw/clipboard.h"
#include "cursor.h"
#include "host/daw/edithistory.h"
#include "keyboard.h"
#include "basectrl.h"
#include "host/daw/mainctrl.h"
#include "grid.h"
#include "gui/container/container.h"
#include "trackctr.h"
#include "host/track/trackctr_types.h"
#include "trackcontent.h"
#include "tracktimeline.h"
#include "theme.h"
#include "gui/contextmenu/contextmenu.h"
#include "mouse.h"
#include "mousecursor.h"
#include "logging.h"
#include "host/audiocache/audiocache.h"
#include "gui/cliprenderer/cliprenderer.h"
#include "host/track/track_impl.h"
#include "host/midiarp/midiarp.h"
#include "logging.h"
#include "host/host_pluginmanager.h"
#include "types.h"
#include "wave/waveform_render.h"
#include "wave/waveform_render_impl.h"
#include "host/daw/daw_async_consolidate_clips.h"


/*static*/ void action_modify_track::loadTrackSnapshot(DawInstance* daw, track_t* track, const track_snapshot_t* trackStored) {
    if (trackStored->storeOpts.storeClips) {
        track->getMidi().deleteClips(daw);
        track->releaseTrackContent();
    }
    *track = *trackStored;
    if (trackStored->storeOpts.storeAutomation) {
        auto pluginMgr = daw->getPluginManager();
        track_impl_t* trImpl = track->getStage();
        if (trImpl->arp) {
            loadAutomation(trackStored->data.trackArp.automatedParams, trImpl->arp);
        }
        loadAutomation(trackStored->data.trackParams.automatedParams, &trImpl->mixer);
        std::deque<const std::vector<plugin_snapshot_t>*> pluginSnapshots;
        pluginSnapshots.push_front(&trackStored->data.pluginSnapshots);
        while (!pluginSnapshots.empty()) {
            auto pVecSnaps = pluginSnapshots.front();
            pluginSnapshots.pop_front();
            for (auto& snap : *pVecSnaps) {
                auto stage2 = pluginMgr->getPluginById(snap.projectGlobalId);
                dbgassert(stage2);
                loadAutomation(snap.automatedParams, stage2);
                pluginSnapshots.push_front(&snap.pluginSnapshots);
            }
        }
    }
}
void action_modify_track::undo(DawInstance* daw) {
    log_lf(Log::L_DEBUG, "action_modify_track undo, num tracks: %zd\n", before.tracks.size());

    daw->resetMouseContext();
    daw->resetEditClip();
    bool initAfter = after.tracks.empty();
    if (initAfter) {
        after.cursor = MainCtrl::get()->getCursor();
    }
    trackallcontainer_t& trCtr = daw->getTracks();
    for (track_snapshot_t* trackStored : before.tracks) {
        log_printf("Undo track %s %d\n", TrackTypeToName(trackStored->trackSettings.type), trackStored->localIdx);
        if (trCtr.validTrackTypeIdx(trackStored->trackSettings.type, trackStored->localIdx)) {
            track_t* track = trCtr.getTrackTypeIdx(trackStored->trackSettings.type, trackStored->localIdx);
            if (initAfter) {
                after.tracks.push_back(new track_snapshot_t(track, trackStored->storeOpts));
            }
            loadTrackSnapshot(daw, track, trackStored);
        } else {
            log_printf("idx is now invalid\n");
        }
    }
    MainCtrl::get()->getCursor() = before.cursor;
}
void action_modify_track::redo(DawInstance* daw) {
    daw->resetMouseContext();
    daw->resetEditClip();
    trackallcontainer_t& trCtr = daw->getTracks();
    for (track_snapshot_t* trackStored : after.tracks) {
        if (trCtr.validTrackTypeIdx(trackStored->trackSettings.type, trackStored->localIdx)) {
            track_t* track = trCtr.getTrackTypeIdx(trackStored->trackSettings.type, trackStored->localIdx);
            loadTrackSnapshot(daw, track, trackStored);
        }
    }
    MainCtrl::get()->getCursor() = after.cursor;
}

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
            dbgassert(0);
        }
    }
}
namespace DAW {

    track_gui_entry_t* getTrackFromMouseImpl(track_gui_manager_i& iGuiMgr, ivec2 mouse) {
        const track_gui_vector_td& _tracks = iGuiMgr.getTracksVisibleFlat();
        for (auto it = _tracks.rbegin(); it != _tracks.rend(); ++it) {
            auto tr = *it;
            if (tr->content->isVisible()) {
                ivec2 topBottom{};
                getTrackGuiYBounds(tr, topBottom);
                if (mouse.y >= topBottom.x && mouse.y < topBottom.y) {
                    return tr;
                }
            }
        }
        return nullptr;
    }
    gui_track_subtrack* getSubTrackFromMouse(track_gui_manager_i& iGuiMgr, ivec2 mouse, bool isDragSnap) {
        track_gui_entry_t* trackExact = getTrackFromMouseImpl(iGuiMgr, mouse);
        if (trackExact && !trackExact->subtracks.empty()) {
            for (gui_track_subtrack* atr : trackExact->subtracks) {
                if (mouse.y >= atr->top() && mouse.y < atr->bottom()) {
                    return atr;
                }
            }
        }
        return nullptr;
    }
    track_gui_entry_t* getTrackFromMouse(track_gui_manager_i& iGuiMgr, ivec2 mouse) {
        return getTrackFromMouseImpl(iGuiMgr, mouse);
    }
    gui_clip* GetClipFromMouse(track_gui_manager_i& iGuiMgr, ivec2 mouse) {
        const track_gui_vector_td& _tracks = iGuiMgr.getTracksVisibleFlat();
        for (auto it = _tracks.rbegin(); it != _tracks.rend(); ++it) {
            auto tr = *it;
            if (tr->content->isVisible() && !tr->clipsGuis.empty()) {
                ivec2 topBottom{};
                getTrackGuiYBounds(tr, topBottom);
                if (mouse.y >= topBottom.x && mouse.y < topBottom.y) {
                    auto mousePosTrackRelative = mouse - ivec2{ 0, topBottom.x };
                    for (auto& [clip, guiClip] : tr->clipsGuis) {
                        if (guiClip->parent && guiClip->isVisible()
                            && !guiClip->isCulled()
                            && guiClip->contains(mousePosTrackRelative)) {
                            return guiClip;
                        }
                    }
                }
            }
        }
        return nullptr;
    }
    gui_clip* GetClipGuiFromTime(track_gui_entry_t* tr, tick_t time) {
        for (auto& [clip, guiClip] : tr->clipsGuis) {
            if (guiClip->parent && guiClip->isVisible()
                && !guiClip->isCulled()
                && clip->start() <= time && clip->end() > time) {
                return guiClip;
            }
        }
        return nullptr;
    }
    gui_clip* GetClipGuiFromTimeAndTrackIdx(track_gui_manager_i& iGuiMgr, int32_t trackIdx, tick_t time) {
        if (iGuiMgr.validTrackIdx(trackIdx)) {
            track_gui_entry_t* tr = iGuiMgr.atNC(trackIdx);
            return GetClipGuiFromTime(tr, time);
        }
        return nullptr;
    }

    track_gui_entry_t* getTrackFromMouseClosest(track_gui_manager_i& iGuiMgr, ivec2 mouse) {
        double minDist = 0;
        const track_gui_vector_td& _tracks = iGuiMgr.getTracksVisibleFlat();
        track_gui_entry_t* trackClosest = getTrackFromMouseImpl(iGuiMgr, mouse);
        if (!trackClosest) {
            for (auto it = _tracks.rbegin(); it != _tracks.rend(); ++it) {
                auto tr = *it;
                if (!tr->content->isVisible()) {
                    continue;
                }
                ivec2 topBottom{};
                getTrackGuiYBounds(tr, topBottom);
                const int top = topBottom.x;
                const int bottom = topBottom.y;
                const int minDistToTr = math::min(math::abs(top - mouse.y), math::abs(bottom - mouse.y));
                if (!trackClosest || minDistToTr < minDist) {
                    minDist = minDistToTr;
                    trackClosest = tr;
                }
            }
        }
        return trackClosest;
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
    template<typename Functor>
    void VisitIntersectingClips(track_gui_entry_t* trEntry, tick_t tickBegin, tick_t tickEnd, Functor f) {
        auto& midi = trEntry->track->getMidi();
        for (clip_t* c : midi.getClips()) {
            if (c->start() < tickEnd && c->end() > tickBegin) {
                f(trEntry, c);
            }
        }
    }

    template<typename Functor>
    void VisitIntersecting(track_gui_manager_i& trackList, const DAW::Cursor& _cursor, Functor f) {
        int32_t tickBegin  = _cursor.getTickBegin();
        int32_t tickEnd    = _cursor.getTickEnd();
        int32_t trackBegin = _cursor.getTrackBegin();
        int32_t trackEnd   = _cursor.getTrackEnd();
        if (!_cursor.isSubtrackSelection()) {
            for (int i = trackBegin; i <= trackEnd; i++) {
                if (trackList.validTrackIdx(i)) {
                    track_gui_entry_t* tr = trackList.atNC(i);
                    VisitIntersectingClips(tr, tickBegin, tickEnd, f);
                }
            }
        }
    }

    bool HandleEditorCommand(DawInstance* daw, DawCtrl* dawCtrl, track_gui_manager_i& iGuiMgr, DAW::Cursor& cursor, scaled_grid& grid, project_t& project, const UI::CommandContext& ctxt) {
        auto& kevt = ctxt.kevt;
        if (kevt.type != K_RELEASE) {
            trackstate_t preModifyState;
            ThreadLock lock      = daw->lockPlayThread();
            bool modified        = false;
            bool handledKeyinput = false;
            String desc          = "???";
            bool bCopyAutomation = daw_tls::getTls().runtime->copyAutomation;
            auto command = ctxt.type;
            if (kevt.type == K_PRESS) {
                if (command == CMD_SELECT_ALL) {
                    tick_t evtMin            = INVALID_TICK;
                    tick_t evtMax            = INVALID_TICK;
                    track_gui_entry_t* trMin = nullptr;
                    track_gui_entry_t* trMax = nullptr;
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
                if (command == CMD_CREATE_EMPTY_CLIP && cursor.getRange()) {
                    int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
                    int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
                    project.trackList.copyTracks(idxBegin, idxEnd, preModifyState);
                    preModifyState.cursor = cursor;
                    /* maybe do    this->clipboard = copy */
                    std::shared_ptr<clip_clipboard> clipboardCopy         = DAW::copySelection(iGuiMgr, cursor, bCopyAutomation);
                    std::shared_ptr<clip_clipboard> clipboardConsolidated = DAW::consolidateClipboard(clipboardCopy, cursor);
                    cursor.setLeftAligned();
                    //cursor.cursorPos += cursor.getRange();
                    DAW::cutSelection(daw, iGuiMgr, cursor, bCopyAutomation);
                    DAW::pasteClipboard(daw, iGuiMgr, clipboardConsolidated.get(), cursor, bCopyAutomation);
                    DAW::VisitIntersecting(iGuiMgr, cursor, [](track_gui_entry_t* trEntry, clip_t* c) {
                        c->rgb = trEntry->track->rgb;
                        c->setDirty();
                    });
                    grid.makeTickVisible(cursor.cursorPos + clipboardConsolidated->selRange);
                    handledKeyinput = true;
                    modified        = true;
                    desc            = idxBegin == idxEnd ? "Create clip" : "Create clips";
                }
                if (command == CMD_DELETE && cursor.getRange()) {
                    int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
                    int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
                    project.trackList.copyTracks(idxBegin, idxEnd, preModifyState);
                    preModifyState.cursor = cursor;
                    DAW::cutSelection(daw, iGuiMgr, cursor, bCopyAutomation);
                    handledKeyinput = true;
                    modified        = true;
                    desc            = "Delete clips";
                } else if (command == CMD_CUT && cursor.getRange()) {
                    auto m_clipboard      = DAW::copySelection(iGuiMgr, cursor, bCopyAutomation);
                    daw->setClipClipboard(m_clipboard);
                    int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
                    int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
                    project.trackList.copyTracks(idxBegin, idxEnd, preModifyState);
                    preModifyState.cursor = cursor;
                    DAW::cutSelection(daw, iGuiMgr, cursor, bCopyAutomation);
                    handledKeyinput = true;
                    modified        = true;
                    desc            = "Cut clips";
                } else if (cursor.getRange() && (command == CMD_MUTE || command == CMD_SET_COLOR || command == CMD_SET_NAME)) {
                    auto clipboard = DAW::copySelection(iGuiMgr, cursor, bCopyAutomation);
                    daw->setClipClipboard(clipboard);
                    int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
                    int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
                    project.trackList.copyTracks(idxBegin, idxEnd, preModifyState);
                    preModifyState.cursor = cursor;
                    switch (command) {
                        case CMD_MUTE:
                            DAW::VisitIntersecting(iGuiMgr, cursor, [](track_gui_entry_t* trEntry, clip_t* c) {
                                c->enabled = !c->enabled;
                                c->setDirty();
                            });
                            desc = "Mute clips";
                            break;
                        case CMD_SET_COLOR:
                            DAW::VisitIntersecting(iGuiMgr, cursor, [rgb = ctxt.argInt0](track_gui_entry_t* trEntry, clip_t* c) {
                                c->rgb = rgb;
                                c->setDirty();
                            });
                            desc = "Set color";
                            break;
                        case CMD_SET_NAME:
                            DAW::VisitIntersecting(iGuiMgr, cursor, [strName = ctxt.argStr0](track_gui_entry_t* trEntry, clip_t* c) {
                                c->name = strName;
                                c->setDirty();
                            });
                            desc = "Set name";
                            break;
                        default:
                            unreachable();
                            break;
                    }
                    grid.makeTickVisible(cursor.cursorPos + cursor.selRange / 2);
                    handledKeyinput = true;
                    modified        = true;
                } else if (command == CMD_COPY && cursor.getRange()) {
                    auto clipboard = DAW::copySelection(iGuiMgr, cursor, bCopyAutomation);
                    daw->setClipClipboard(clipboard);
                    handledKeyinput = true;
                } else if (command == CMD_CONSOLIDATE && cursor.getRange() && !cursor.isSubtrackSelection()) {
                    auto pTask = new consolidate_task_t{};
                    auto& task = *pTask;
                    task.dawCtrl = dawCtrl;
                    task.daw = daw;
                    task.iGuiMgr = &iGuiMgr;
                    task.cursor = cursor;
                    task.cursor.setLeftAligned();
                    task.clipboardCopy = DAW::copySelection(iGuiMgr, cursor, bCopyAutomation);
                    task.bCopyAutomation = bCopyAutomation;
                    daw->setAsyncTask(pTask);
                    handledKeyinput = true;
                    desc            = "Consolidate selection";
                } else if (command == CMD_DUPLICATE && cursor.getRange()) {
                    int32_t idxBegin = iGuiMgr.getTrackProjectIndex(cursor.getTrackBegin());
                    int32_t idxEnd   = iGuiMgr.getTrackProjectIndex(cursor.getTrackEnd());
                    project.trackList.copyTracks(idxBegin, idxEnd, preModifyState);
                    preModifyState.cursor = cursor;
                    std::shared_ptr<clip_clipboard> newClipboard = DAW::copySelection(iGuiMgr, cursor, bCopyAutomation);
                    cursor.setLeftAligned();
                    cursor.cursorPos += cursor.getRange();
                    DAW::pasteClipboard(daw, iGuiMgr, newClipboard.get(), cursor, bCopyAutomation);
                    grid.makeTickVisible(cursor.cursorPos + newClipboard->selRange);
                    handledKeyinput = true;
                    modified        = true;
                    desc            = "Duplicate clips";
                } else if ((command == CMD_PASTE_NO_AUTOMATION || command == CMD_PASTE) && daw->getClipboardType() == ClipBoardType::CLIPBOARD_CLIPS) {
                    if (command == CMD_PASTE_NO_AUTOMATION) {
                        bCopyAutomation = false;
                    }
                    auto& clipboard = daw->getClipsClipboard();
                    DAW::Cursor pasteRange = cursor;
                    track_selection_t pasteSelection;
                    pasteRange.selTrackRange = clipboard->selTrackRange;
                    iGuiMgr.getTrackSelection(pasteRange, pasteSelection);
                    project.trackList.copyTracks(pasteSelection.trackIdxMin, pasteSelection.trackIdxMax, preModifyState);
                    preModifyState.cursor = cursor;
                    cursor.setLeftAligned();
                    if (clipboard->type == clip_clipboard::ClipboardFull)
                        DAW::cutSelection(daw, iGuiMgr, cursor, bCopyAutomation);
                    DAW::pasteClipboard(daw, iGuiMgr, clipboard.get(), cursor, bCopyAutomation);
                    cursor.selTrackRange = clipboard->selTrackRange;
                    cursor.selRange      = clipboard->selRange;
                    grid.makeTickVisible(cursor.getTickEnd());
                    handledKeyinput = true;
                    modified        = true;
                    desc            = "Paste clips";
                }
            }
            
            if (command == GlobalCommandType::CMD_MOVE_CURSOR) {
                auto dir = ivec2(ctxt.argInt0, ctxt.argInt1);
                if (dir.y) {
                    if (isShift(kevt.mods)) {
                        if (cursor.isSubtrackSelection()) {
                            if (iGuiMgr.validTrackIdx(cursor.cursorTrack)) {
                                cursor.selSubTrackRange += -dir.y;
                                const track_gui_entry_t* tr = iGuiMgr.at(cursor.cursorTrack);
                                cursor.fixCursorSubRange(CtrSize(tr->subtracks));
                            }
                        } else {
                            cursor.selTrackRange += -dir.y;
                            cursor.fixCursorTrackRange(CtrSize(iGuiMgr.getTracksVisibleFlat()));
                        }
                    } else {

                        cursor.setLeftAligned();
                        auto moveMainCursor = [&]() {
                            cursor.setTrack(project.trackList.clampTrackIdx(cursor.cursorTrack - dir.y));
                        };
                        auto moveCursor = [&]() {
                            if (cursor.isSubtrackSelection()) {
                                if (!iGuiMgr.validTrackIdx(cursor.cursorTrack)) {
                                    cursor.setTrack(0);
                                    cursor.cursorSubTrack   = -1;
                                    cursor.selSubTrackRange = 0;
                                    return;
                                }
                                const track_gui_entry_t* tr = iGuiMgr.at(cursor.cursorTrack);
                                cursor.cursorSubTrack -= dir.y;
                                cursor.fixCursorSubRange(CtrSize(tr->subtracks));
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
                daw->pushHist(track_action);
            }
            if (handledKeyinput) {
                daw->updateVisibleTrackContents();
            }
            return handledKeyinput;
        }
        return false;
    }
} // namespace DAW

bool guitrack_editor::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    if (ctxt.type == GlobalCommandType::CMD_BEGIN_RENAME) {
        if (ctxt.kevt.type == KeyboardState::K_PRESS) {
            DAW::OpenFloatingTextInput(dawCtrl, dawCtrl->m_mousePos, ivec2(200, 20), "", [trEditor = this](const String& str) {
                DAW::UI::CommandContext ctxtSetName = {GlobalCommandType::CMD_SET_NAME, {}};
                ctxtSetName.argStr0 = str;
                trEditor->handleEditorCommand(ctxtSetName);
                return false;
            });
        }
        return true;
    }
    if (DAW::HandleEditorCommand(dawCtrl->getDaw(), dawCtrl, iGuiMgr, cursor, grid, project, ctxt)) {
        return true;
    }
    return false;
}
bool guitrack_editor::handleKeyInput(KeyEvent& kevt) {
    if (kevt.type != K_REPEAT && isCtrlKey(kevt.keyCode)) {
        if ((action.dragtype == DRAG_CLIPS_MOVE || action.dragtype == DRAG_CLIPS_COPY)) {
            if ((action.dragtype == DRAG_CLIPS_COPY) != (kevt.type == K_PRESS)) {
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
    if (kevt.type != K_REPEAT && isAltKey(kevt.keyCode)) {
        dawCtrl->window->fireMouseMoved();
        return false;
    }
    if (action.dragtype) {
        return false;
    }
    if (kevt.cmd) {
        auto temp = kevt.cmd->getKeybindContextData(kevt);
        if (handleEditorCommand(temp)) {
            return true;
        }
    }
    if (isArrowKey(kevt.keyCode)) {
        ivec2 dir;
        arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
        DAW::UI::CommandContext ctxt = {GlobalCommandType::CMD_MOVE_CURSOR, kevt, dir.x, dir.y};
        if (handleEditorCommand(ctxt)) {
            return true;
        }
    }
    return false;
}


void guitrack_editor::trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
    ivec2 local  = evt.relMousepos;
    int32_t tick = grid.screenToTickSnap(local.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
    gui_track_subtrack* subTr = DAW::getSubTrackFromMouse(iGuiMgr, local, false);
    if (subTr) {
        subTrSelected = subTr;
        trSelected = subTr->m_trackentry;
    } else {
        subTrSelected = nullptr;
        trSelected = DAW::getTrackFromMouse(iGuiMgr, local);
    }
    if (trSelected != nullptr) {
        dawCtrl->getDaw()->setEditClip(nullptr, {});
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
        ivec2 local         = evt.relMousepos;
        DAW::Cursor& cursor = dawCtrl->getCursor();

        track_gui_entry_t* trNxtSelected = nullptr;
        gui_track_subtrack* subTr        = DAW::getSubTrackFromMouse(iGuiMgr, local, true);
        if (subTrSelected) {
            if (subTr && subTr->m_track != subTrSelected->m_track) {
                subTr = nullptr;
            }
            trNxtSelected = DAW::getTrackFromMouse(iGuiMgr, local);
            if (trNxtSelected && trNxtSelected->idx < subTrSelected->m_trackentry->idx) {
                subTr = subTrSelected->m_trackentry->subtracks.front();
            }
            if (trNxtSelected && trNxtSelected->idx > subTrSelected->m_trackentry->idx) {
                subTr = subTrSelected->m_trackentry->subtracks.back();
            }
        } else {
            trNxtSelected = DAW::getTrackFromMouseClosest(iGuiMgr, local);

            if (!trNxtSelected)
                return;
            //if track is folded get last child in linear layout
        }
        int32_t tick = grid.screenToTickSnap(local.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
        if (evt.guiDragged == this) {// cursor move / range select

            cursor.selRange = tick - cursor.cursorPos;
            if (cursor.isSubtrackSelection()) {
                //c.isSubtrackSelection() guarantees subTrSelected to be non-nullptr
                dbgassert(subTrSelected);
                if (subTr) {
                    cursor.selSubTrackRange = (subTr->idx - subTrSelected->idx);
                    dbgassert(cursor.getSubTrackEnd() > -1);
                    dbgassert(cursor.getSubTrackBegin() <= cursor.getSubTrackEnd());
                    dbgassert(cursor.getSubTrackBegin() < (int) subTr->m_trackentry->subtracks.size());
                    dbgassert(cursor.getSubTrackEnd() < (int) subTr->m_trackentry->subtracks.size());
                }

            } else {
                cursor.selTrackRange = (trNxtSelected->idx - trSelected->idx);
                log_printf("sel track pos %d range %d\n", cursor.getTrackBegin(), cursor.getTrackRange());

            }
//            beatbar16th_t songPos = MainCtrl::get()->toBeatBar16th(tick);
//            log_printf("Select at Track %d - %d %d %d %d = %u.%u.%u\n", trSelected->idx, c.cursorPos, tick, c.selRange, local.x, songPos.bar, songPos.beat, songPos.th);
        }
    }
}
void guitrack_editor::trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
    auto daw = dawCtrl->getDaw();
    ivec2 local = evt.relMousepos;
    track_gui_entry_t* trNxtSelected = DAW::getTrackFromMouseClosest(iGuiMgr, local);
    daw->setSelectedTrackEntry(trNxtSelected);
    trSelected    = nullptr;
    subTrSelected = nullptr;
    if (trNxtSelected) {
        int32_t tick = grid.screenToTickSnap(local.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
        auto gClip = DAW::GetClipGuiFromTime(trNxtSelected, tick);
        if (gClip) {
            clipboard_view_t view;
            DAW::GetClipboardView(iGuiMgr, cursor, view);
            daw->setEditClip(gClip, view);
        }
    }
}
void guitrack_editor::dragSelectionBegin(gui_clip* gClip, MouseEvent& evt) {
    selectionMoved      = false;
    ivec2 local         = evt.relMousepos;
    tick_t tickExact    = grid.screenToTickSnap(local.x, SNAP_OFF);
    track_t* track      = gClip->m_track;
    clip_t* clicked     = gClip->m_clip;
    track_gui_entry_t* trackClicked = DAW::getTrackFromMouse(iGuiMgr, local);

    if (trackClicked) {
        dawCtrl->getDaw()->setSelectedTrackEntry(trackClicked);
    }

    action.dragtype  = DRAG_NONE;
    action.clipboard = nullptr;
    const auto heightTitle = gClip->theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    auto clipTrackPos = toControlsObjectSpace(evt.mousepos, gClip->parent);
    if (gClip->isLeftDragZone(clipTrackPos, heightTitle)) {
        action.dragtype = DRAG_CLIPS_RESIZE_LEFT;
    } else if (gClip->isRightDragZone(clipTrackPos, heightTitle)) {
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
        bool bCopyAutomation = daw_tls::getTls().runtime->copyAutomation;
        action.clipboard   = DAW::copySelection(iGuiMgr, action.cursorBegin, bCopyAutomation);
    }
}
void guitrack_editor::dragSelectionMove(gui_clip* gui, MouseEvent& evt) {
    if (action.dragtype) {
        if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT || action.dragtype == DRAG_CLIPS_RESIZE_RIGHT) {
            auto daw = dawCtrl->getDaw();
            ThreadLock lock = daw->lockPlayThread();
            clip_t* clip                  = gui->m_clip;
            track_gui_entry_t* trackentry = gui->m_trackentry;
            track_t* track                = trackentry->track;
            dragStartLayout.apply(track);
            int32_t tick = grid.screenToTickSnap(evt.relMousepos.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
            if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT) {
                if (clip->start() != tick) {
                    tick_t offset = tick - clip->time;
                    if (clip->getLen() - offset < MIN_CLIPSIZE) {
                        offset = clip->getLen() - MIN_CLIPSIZE;
                    }
                    if (!(grid.grid_dens.getSnap() == SNAP_OFF || isAlt(evt.kbmods)) && clip->getLen() - offset < grid.getTickLength()) {
                        offset = clip->getLen() - grid.getTickLength();
                    }
                    clip->time += offset;
                    clip->adjustLen(-offset);
                    clip->adjustStartOffset(offset);
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
                        clip->adjustLen(-offset);
                    }
                }
            }
            clip->setDirty();
            resizeOtherClips(track->getMidi(), clip);
            setSelectionRange(clip, trackentry);
            daw->updateVisibleTrackContents();
            return;
        }
    }
    dragClipboardMove(evt.relMousepos, evt.kbmods);
}

void guitrack_editor::dragClipboardMove(ivec2 local, KeyboardMods kbmods) {
    if (action.dragtype) {
        track_gui_entry_t* trNxtSelected = DAW::getTrackFromMouseClosest(iGuiMgr, local);

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
    auto daw = dawCtrl->getDaw();
    if (action.dragtype) {
        bool showclip = true;
        if (action.dragtype == DRAG_CLIPS_MOVE || action.dragtype == DRAG_CLIPS_COPY) {
            const DAW::Cursor& cursorBegin = action.cursorBegin;
            selectionMoved |= cursorBegin.cursorPos != cursor.cursorPos;
            selectionMoved |= cursorBegin.cursorTrack != cursor.cursorTrack;
            ivec2 local                      = evt.relMousepos;
            track_gui_entry_t* trNxtSelected = DAW::getTrackFromMouseClosest(iGuiMgr, local);
            if (trNxtSelected) {
                daw->setSelectedTrackEntry(trNxtSelected);
            }
            bool bCopyAutomation = daw_tls::getTls().runtime->copyAutomation;
            if (selectionMoved && trNxtSelected) {
                ThreadLock lock = daw->lockPlayThread();

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
                std::shared_ptr<clip_clipboard> clipboard = DAW::copySelection(iGuiMgr, cursorBegin, bCopyAutomation);
                if (!isCtrl(evt.kbmods)) {
                    DAW::cutSelection(daw, iGuiMgr, cursorBegin, bCopyAutomation);
                }
                int32_t trackGuiIdx = dstTrack - trackOffset;
                DAW::pasteFullClipboard(daw, iGuiMgr, clipboard.get(), trackGuiIdx, dstPos, bCopyAutomation);
                daw->updateVisibleTrackContents();
                showclip = false;
                auto* track_action = new action_modify_track("Move clips", std::move(resizePreModifyState));
                daw->pushHist(track_action);
            }
        } else if (action.dragtype == DRAG_CLIPS_RESIZE_LEFT || action.dragtype == DRAG_CLIPS_RESIZE_RIGHT) {
            clip_t* clipPtr        = gui->m_clip;
            ThreadLock lock        = daw->lockPlayThread();
            track_t* trackPtr      = gui->m_track;
            trackdata_midi_t& midi = trackPtr->getMidi();
            auto& clips = midi.getClips();
            auto it = clips.begin();
            int32_t nRemoved = 0;
            while (it != clips.end()) {
                clip_t* c = *it;
                if (c->getLen() <= 0) {
                    it = clips.erase(it);
                    releaseClipResources(c, daw);
                    auto& clipLayouts = dragStartLayout.clips;
                    clipLayouts.erase(std::remove_if(clipLayouts.begin(), clipLayouts.end(), [c](auto const& l) {
                        return l.clip == c;
                    }), clipLayouts.end());
                    delete c;
                    nRemoved++;
                } else {
                    it++;
                }
            }
            if (nRemoved) {
                midi.sortClips();
            }
            if (!midi.hasClip(clipPtr)) {
                gui      = nullptr;
                showclip = false;
            }
            if (dragStartLayout.diff(trackPtr)) {
                auto* track_action = new action_modify_track("Resize clips", m_resizePreModifyState.copy());
                daw->pushHist(track_action);
            }
        }
        action.dragtype = DRAG_NONE;
        if (showclip) {
            clipboard_view_t view;
            DAW::GetClipboardView(iGuiMgr, cursor, view);
            daw->setEditClip(gui, view);
            if (gui)
                dawCtrl->showClipEditor();
        }
    }
}

bool guitrack_editor::clipDropBegin(dragdrop_midifile& clip, ivec2 mousepos, KeyboardMods kbmods) {
    tick_t tick                     = grid.screenToTickSnap(mousepos.x, SNAP_ON);
    tick_t tickExact                = grid.screenToTickSnap(mousepos.x, SNAP_OFF);
    track_gui_entry_t* trackClicked = DAW::getTrackFromMouse(iGuiMgr, mousepos);
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
        clip.target = makeSafeRef();
        return true;
    }
    return false;
}
void guitrack_editor::clipDropCancel() {
    if (action.dragtype == clip_dragtype_t::DROP_FILE_EXTERNAL) {
        action.clipboard   = nullptr;
        action.dragtype    = DRAG_NONE;
    }
}
bool guitrack_editor::clipDropMove(dragdrop_midifile& clip, ivec2 mousepos, KeyboardMods kbmods) {
    if (!action.dragtype) {
        if (!clipDropBegin(clip, mousepos, kbmods))
            return false;
    }
    if (action.dragtype == clip_dragtype_t::DROP_FILE_EXTERNAL) {
        dragClipboardMove(mousepos, kbmods);
        clip.isValidTarget = true;//inform higher level that we accept and process this drop attempt
        clip.target = makeSafeRef();
        return true;
    }
    return false;
}
bool guitrack_editor::clipDropFinal(dragdrop_midifile& clip, ivec2 mousepos, KeyboardMods kbmods) {
    if (action.dragtype == clip_dragtype_t::DROP_FILE_EXTERNAL) {
//        dragClipboardMove(mousepos);//TODO: maybe call move again to set final pos?
        auto daw = dawCtrl->getDaw();
        auto lock = daw->lockPlayThread();
        track_gui_entry_t* trNxtSelected = DAW::getTrackFromMouseClosest(iGuiMgr, mousepos);
        int32_t tick                     = grid.screenToTickSnap(mousepos.x, SNAP_ON);
        tick_t dstPos                    = tick;
        int32_t dstTrack                 = trNxtSelected->idx;
        bool bCopyAutomation = daw_tls::getTls().runtime->copyAutomation;
        DAW::pasteFullClipboard(daw, iGuiMgr, action.clipboard.get(), dstTrack, dstPos, bCopyAutomation);
        daw->updateVisibleTrackContents();
        action.clipboard   = nullptr;
        action.dragtype    = DRAG_NONE;
        clip.isValidTarget = true;//inform higher level that we accept and process this drop attempt
        clip.target = makeSafeRef();
        return true;
    }
    return false;
}

void guitrack_editor::renderClip(NVGcontext* vg, const track_gui_entry_t* const entry, clip_t* cl, tick_t offset) {
    ivec2 clipPos     = ivec2();
    ivec2 scissorSize = entry->content->size;
    ivec2 clipSize    = entry->content->size;

    if (getClipPositionInt(grid, scissorSize, cl, clipPos, clipSize, offset)) {
        clipPos.y += entry->content->pos.y;
        if (cl->clipType == CLIP_MIDI) {
            renderMidiClip(vg, theme, entry, cl, clipPos, clipSize);
        } else if (cl->clipType == CLIP_AUDIO) {
            //TODO: move this out of here
            auto prjGlobals = dawCtrl->getDaw()->getHost()->prjGlobals;
            auto& clipAudio = cl->audio;
            if (!clipAudio.renderedAudio) {
                clipAudio.renderedAudio = new rendered_audio_clip_t(dawCtrl->getWaveformRenderer());
            }
            audiofile_t* audio = dawCtrl->getDaw()->getAudioCache()->get(clipAudio.id);

            if (!audio) {
                clipAudio.renderedAudio->releaseWaveformTexture();
                return;
            }

            dbgassert(clipSize.x > 0);

            const auto HEIGHT_CLIP_TITLE = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
            // const auto HEIGHT_CLIP_TITLE = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP) / 2;
            ivec2 shrink = ivec2(0, (HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT * 2));
            ivec2 sizeClipped = clipSize - shrink;
            ivec2 posClipped = clipPos + shrink;

            // getClippedPosSize(parent->size, posClipped, sizeClipped);

            if (posClipped.x + sizeClipped.x <= 0 || sizeClipped.x <= 0) {
                clipAudio.renderedAudio->releaseWaveformTexture();
                return;
            }

            const auto tempo100 = prjGlobals.tempo100;
            const auto samplerate = audio->sample->sampleRate;
            auto waveform = makeWaveformFromClip(tempo100, samplerate, grid, entry->content->size, cl, clipPos, clipSize - shrink, posClipped, sizeClipped);
            if (waveform.size.x < 1 || waveform.size.y < 1) {
                clipAudio.renderedAudio->releaseWaveformTexture();
                clipAudio.renderedAudio->updateWaveformTexture(waveform);
                return;
            }
            auto& currentWaveformShape = clipAudio.renderedAudio->getCurrentWaveformShape();
            bool equal = ((waveform.size.y > 0) == (currentWaveformShape.size.y > 0)) && isEqualWaveform3(waveform, currentWaveformShape);

            bool canQueue  = clipAudio.renderedAudio->getWaveformRenderer()->canQueueUpdate();
            ivec2 sizeDiff = math::absvec2(waveform.size - currentWaveformShape.size);
            ivec2 limit    = math::maxvec2(ivec2(1), ivec2(waveform.size.x / 4, 16));
            if (!canQueue) {
                limit.x = waveform.size.x / 4;
            }
            if (waveform.clipped || (dawCtrl && !dawCtrl->isZooming())) {
                limit = { 0, 0 };
            }
            if (!equal || (sizeDiff.x > limit.x || sizeDiff.y > limit.y)) {
                clipAudio.renderedAudio->updateWaveformTexture(waveform);
                if (sizeDiff.x > limit.x || sizeDiff.y > limit.y) {
                    //releaseRendered();
                }
            }

            clipAudio.renderedAudio->updateClipPrerender(vg, cl, audio, false);
            gui_waveform_texture_ref* ref = clipAudio.renderedAudio->getWaveformTextureRef();
            renderAudioClip(vg, clipAudio.renderedAudio->getWaveformRenderer(), theme, entry->track, cl, audio, ref, clipPos, clipSize - shrink, posClipped, sizeClipped);
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
        track_gui_entry_t* entry{};
        always_assert(iGuiMgr.getPointerEntry(tr, &entry));
        dbgassert(entry->content != nullptr);
        for (auto& clip : trClipboard->clips) {
            renderClip(vg, entry, clip.get(), (cursor.cursorPos - _clipboard->srcPos));
        }
    }
}

void guitrack_editor::renderDebugPass(NVGcontext* vg) {
    ivec2 posInset  = getPosContent();
    ivec2 sizeInset = getSizeContent();

    if (sizeInset.y <= 0 || sizeInset.x <= 0) {
        return ;
    }
    nvgSave(vg);
    nvgResetScissor(vg);
    nvgTranslate(vg, posInset.x, posInset.y);

    for (track_t* g : project.trackList) {
        track_gui_entry_t* entry{};
        if (iGuiMgr.getPointerEntry(g, &entry)) {
            if (entry->content->isVisible()) {
                entry->content->renderDebugPass(vg);
                for (auto* gSubtrack : entry->subtracks) {
                    gSubtrack->renderDebugPass(vg);
                }
            }
        }
    }
    nvgRestore(vg);
}

void guitrack_editor::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }

    //TODO: move grid background rendering into standalone function
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
    const int ySplit     = DAW::getPosYFirstReturnTrack(iGuiMgr.getTracksVisibleFlat());
    int32_t bottomHeight = cs.y - ySplit;
    if (bottomHeight > 0) {
        nvgSave(vg);
        nvgIntersectScissor(vg, 0, ySplit, cs.x, bottomHeight);
        for (track_t* g : project.tracksBottom.tracksFlat) {
            track_gui_entry_t* entry{};
            if (!iGuiMgr.getPointerEntry(g, &entry)) {
                dbgassert(0);
                continue;
            }

            dbgassert(entry->content->isVisible() == iGuiMgr.isVisible(entry));

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
            track_gui_entry_t* entry{};
            always_assert(iGuiMgr.getPointerEntry(t, &entry));
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
                nvgFillColor(vg, theme->getColor(GuiColor::COL_SELECTION_BACKGROUND));
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
