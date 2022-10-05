#include <algorithm>
#include <functional>
#include <vector>
#include <memory>

#include "assert_dbg.h"
#include "automation.h"
#include "mainctrl.h"
#include "math/seq_math.h"
#include "project.h"
#include "grid.h"
#include "note.h"
#include "cursor.h"
#include "str_util.h"
#include "seq_util.h"
#include "track.h"
#include "clip.h"

#include "gui/gui.h"
#include "gui/track/trackctr.h"
#include "gui/track/trackcontent.h"
#include "track_impl.h"



void copyClipsInRange(const trackdata_midi_t& in, track_clipboard_t& out, int32_t srcPos, int32_t dstPos, int32_t len) {
    for (const auto* const c : in.clips) {
        if (c->end() > srcPos && c->time < srcPos + len) {
            clip_t clone(*c);
            if (c->time < srcPos && c->end() > srcPos) {
                cutClipLeft(&clone, srcPos - c->time);
            }
            if (c->time < srcPos + len && c->end() > srcPos + len) {
                cutClipRight(&clone, (c->end()) - (srcPos + len));
            }
            out.clips.push_back(std::make_shared<clip_t>(std::move(clone)));
        }
    }
    stable_sort(out.clips.begin(), out.clips.end(), [](auto const& a, auto const& b) {
        return a->time < b->time;
    });
}

bool hasClipsInRange(const trackdata_midi_t& in, int32_t srcPos, int32_t len) {
    for (const auto* const c : in.getConstClips()) {
        if (c->end() > srcPos && c->time < srcPos + len) {
            return true;
        }
    }
    return false;
}

namespace DAW {

    void pasteFullClipboard(track_gui_manager_i& trackList, clip_clipboard* clipboard, int32_t track, tick_t tick, bool pasteAutomation) {
        tick_t tickOffset  = tick - clipboard->srcPos;
        tick_t trackOffset = track;
        for (int i = 0; i <= clipboard->selTrackRange; i++) {
            track_clipboard_t* trClipboard = clipboard->tracks[i].get();
            if (!trackList.validTrackIdx(i + trackOffset)) {
                continue;
            }
            int32_t trackIdx = trackList.clampTrackIdx(i + trackOffset);
            track_gui_entry_t* tr = trackList.atNC(trackIdx);
            //if (tr->track->type == TRACK_TYPE_MIDI) {
            trackdata_midi_t& midi = tr->track->getMidi();
            for (auto & clip : trClipboard->clips) {
                clip_t* cloned = clip->clone();
                cloned->time += tickOffset;
                tick_t tickBegin = cloned->time;
                tick_t tickEnd   = cloned->end();
                cutIntersectingClips(tr->track->getMidi(), tickBegin, tickEnd, DawInstance::get());
                midi.addClip(cloned);
            }
            midi.sortClips();
            if (pasteAutomation) {
                auto& automations = trClipboard->automations;
                for (automation_clipboard_t& automClipboard : automations) {
                    auto device = tr->track->getStage()->getAutomatableByType(automClipboard.paramRef);
                    if (device) {
                        auto automation = device->getOrCreateAutomation(automClipboard.paramRef.paramIdx);
                        if (automation) {
                            automation->setRange(tick, tick + clipboard->selRange, automClipboard.dataPoints);
                        }
                    }
                }
            }
        }
    }

    void pasteClipboard(track_gui_manager_i& trackList, clip_clipboard* clipboard, DAW::Cursor& cursor, bool pasteAutomation) {
        if (clipboard->type == clip_clipboard::ClipboardFull) {
            if (cursor.isSubtrackSelection())
                return;
            pasteFullClipboard(trackList, clipboard, cursor.getTrackBegin(), cursor.getTickBegin(), pasteAutomation);
        } else if (clipboard->type == clip_clipboard::ClipboardAutomation) {
            if (!cursor.isSubtrackSelection())
                return;
            int32_t tickBegin  = cursor.getTickBegin();
            int32_t tickLen    = clipboard->selRange;
            int32_t trackBegin = cursor.getTrackBegin();
            if (trackList.validTrackIdx(trackBegin)) {
                track_gui_entry_t* tr  = trackList.atNC(trackBegin);
                int32_t subTrackOffset = cursor.getSubTrackBegin();
                for (int i = 0; i <= clipboard->selTrackRange && i < CtrSize(clipboard->automationLanes); i++) {
                    int32_t subTrackIdx = subTrackOffset + i;
                    if (tr->validSubtrack(subTrackIdx)) {
                        gui_track_subtrack* subtrack          = tr->subtracks[subTrackIdx];
                        auto& automClipboard = clipboard->automationLanes[i];
                        if (!automClipboard.dataPoints.empty() && subtrack->at) {
                            auto automation = subtrack->at->getOrCreateAutomation(subtrack->param);
                            if (automation) {
                                automation->setRange(tickBegin, tickBegin + tickLen, automClipboard.dataPoints);
                            }
                        }
                    }
                }
            }
        }
    }

    void muteIntersecting(track_gui_manager_i& trackList, const DAW::Cursor& _cursor) {
        int32_t tickBegin  = _cursor.getTickBegin();
        int32_t tickEnd    = _cursor.getTickEnd();
        int32_t trackBegin = _cursor.getTrackBegin();
        int32_t trackEnd   = _cursor.getTrackEnd();
        if (!_cursor.isSubtrackSelection()) {
            for (int i = trackBegin; i <= trackEnd; i++) {
                if (trackList.validTrackIdx(i)) {
                    track_gui_entry_t* tr = trackList.atNC(i);
                    muteIntersectingClips(tr->track->getMidi(), tickBegin, tickEnd);
                }
            }
        }
    }

    std::shared_ptr<clip_clipboard> consolidateClipboard(std::shared_ptr<clip_clipboard>& clipboardIn, const DAW::Cursor& _cursor) {
        clip_clipboard* const pClipboardIn   = clipboardIn.get();
        auto clipboard = std::make_shared<clip_clipboard>();

        int32_t tickBegin     = _cursor.getTickBegin();
        int32_t tickEnd       = _cursor.getTickEnd();
        int32_t trackBegin    = _cursor.getTrackBegin();
        int32_t trackEnd      = _cursor.getTrackEnd();
        //int32_t trackSubBegin = _cursor.getSubTrackBegin();
        //int32_t trackSubEnd   = _cursor.getSubTrackEnd();
        clipboard->srcPos     = tickBegin;
        clipboard->srcTrack   = trackBegin;
        clipboard->selRange   = tickEnd - tickBegin;
        if (_cursor.isSubtrackSelection()) {
        } else {
            clipboard->selTrackRange = trackEnd - trackBegin;
            clipboard->selRange      = tickEnd - tickBegin;
            clipboard->type          = clip_clipboard::ClipboardFull;
            for (const auto& shPtrClipboard : pClipboardIn->tracks) {
                track_clipboard_t trackClipboardOut;
                clip_t clip;
                clip.clipType    = CLIP_MIDI;
                clip.time        = tickBegin;
                clip.offsetStart = 0;
                clip.setLen(tickEnd - tickBegin);
                clip.loopEnabled = false;
                //consolidated.time = tickBegin;
                //consolidated.setLen(tickEnd - tickBegin);
                const auto& clips = shPtrClipboard->clips;
                std::vector<note_t> notes;
                for (const auto& shPtrClip : clips) {
                    if (shPtrClip->end() <= tickBegin || shPtrClip->start() > tickEnd) {
                        continue;
                    }
                    notes.clear();
                    shPtrClip->getInTimeRange(tickBegin, tickEnd, tickBegin, tickEnd, notes);
                    clip.notes.addAll(notes);
                }
                clip.notes.removeDuplicates();
                clip.notes.visitNotes([tickBegin](note_t& note) {
                    note.time -= tickBegin;
                });
                trackClipboardOut.clips.push_back(std::make_shared<clip_t>(std::move(clip)));
                clipboard->tracks.push_back(std::make_shared<track_clipboard_t>(std::move(trackClipboardOut)));
            }
        }
        return clipboard;
    }

    std::shared_ptr<clip_clipboard> copySelection(const track_gui_manager_i& trackList, const DAW::Cursor& _cursor, bool copyAutomation) {
        auto clipboard = std::make_shared<clip_clipboard>();

        int32_t tickBegin     = _cursor.getTickBegin();
        int32_t tickEnd       = _cursor.getTickEnd();
        int32_t trackBegin    = _cursor.getTrackBegin();
        int32_t trackEnd      = _cursor.getTrackEnd();
        int32_t trackSubBegin = _cursor.getSubTrackBegin();
        int32_t trackSubEnd   = _cursor.getSubTrackEnd();
        clipboard->srcPos     = tickBegin;
        clipboard->srcTrack   = trackBegin;
        clipboard->selRange   = tickEnd - tickBegin;
        if (_cursor.isSubtrackSelection()) {
            clipboard->selTrackRange = trackSubEnd - trackSubBegin;
            clipboard->type          = clip_clipboard::ClipboardAutomation;
            if (trackList.validTrackIdx(trackBegin)) {
                const track_gui_entry_t* const tr = trackList.at(trackBegin);
                for (int i = trackSubBegin; i <= trackSubEnd; i++) {
                    if (tr->validSubtrack(i)) {
                        const gui_track_subtrack* subtrack = tr->subtracks[i];
                        const automatable_t* automatable   = subtrack->at;
                        const automated_param_t* automation     = nullptr;
                        if (automatable) {
                            automation = automatable->getRegisteredConstAutomation(subtrack->param);
                        }

                        if (automation) {
                            automation_clipboard_t automationClipboard;
                            automationClipboard.start     = tickBegin;
                            automationClipboard.len       = tickEnd - tickBegin;
                            automationClipboard.paramRef = automatable->toRef();
                            automationClipboard.paramRef.paramIdx = subtrack->param;
                            std::vector<automation_point_t> data;
                            automation->copyRange(tickBegin, tickEnd, data);
                            automationClipboard.dataPoints = std::move(data);
                            dbgassert(automationClipboard.paramRef.paramIdx > -1);
                            clipboard->automationLanes.push_back(std::move(automationClipboard));
                        }
                    }
                }
            }
        }
        if (!_cursor.isSubtrackSelection()) {
            clipboard->selTrackRange = trackEnd - trackBegin;
            clipboard->selRange      = tickEnd - tickBegin;
            clipboard->type          = clip_clipboard::ClipboardFull;
            for (int i = 0; i <= clipboard->selTrackRange; i++) {
                track_clipboard_t trackClipboard;
                std::vector<automation_clipboard_t> automationLanes;
                if (trackList.validTrackIdx(trackBegin + i)) {
                    const track_gui_entry_t* tr = trackList.at(trackBegin + i);
                    copyClipsInRange(tr->track->getConstMidi(), trackClipboard, clipboard->srcPos, 0, clipboard->selRange);
                    auto trackImpl = tr->track->getStage();
                    std::vector<automatable_t*> targets;
                    trackImpl->getAutomatableTrackTargets(targets);
                    std::vector<automation_lane_t> allParams;
                    for (auto& automatable : targets) {
                        allParams.clear();
                        automatable->getAllAutomatedParams(allParams);
                        for (const auto& automation : allParams) {
                            std::vector<automation_point_t> data;
                            automation.copyRange(tickBegin, tickEnd, data);
                            automation_clipboard_t automationClipboard;
                            automationClipboard.dataPoints = std::move(data);
                            automationClipboard.start     = tickBegin;
                            automationClipboard.len       = tickEnd - tickBegin;
                            automationClipboard.paramRef = automatable->toRef();
                            automationClipboard.paramRef.paramIdx = automation.paramIdx;
                            dbgassert(automationClipboard.paramRef.paramIdx > -1);
                            automationLanes.push_back(std::move(automationClipboard));
                        }
                    }
                }
                trackClipboard.automations = std::move(automationLanes);
                clipboard->tracks.push_back(std::make_shared<track_clipboard_t>(std::move(trackClipboard)));
            }
        }
        return clipboard;
    }

    void cutSelection(track_gui_manager_i& trackList, const DAW::Cursor& _cursor, bool cutAutomation) {
        int32_t tickBegin  = _cursor.getTickBegin();
        int32_t tickEnd    = _cursor.getTickEnd();
        int32_t trackBegin = _cursor.getTrackBegin();
        int32_t trackEnd   = _cursor.getTrackEnd();
        if (!_cursor.isSubtrackSelection()) {
            for (int i = trackBegin; i <= trackEnd; i++) {
                if (trackList.validTrackIdx(i)) {
                    track_gui_entry_t* tr = trackList.atNC(i);
                    //if (tr->track->type == TRACK_TYPE_MIDI) {
                    cutIntersectingClips(tr->track->getMidi(), tickBegin, tickEnd, DawInstance::get());
                    // we don't cut automation for now
                    //}
                }
            }
        } else {
            int32_t trackSBegin = _cursor.getSubTrackBegin();
            int32_t trackSEnd   = _cursor.getSubTrackEnd();
            if (trackList.validTrackIdx(trackBegin)) {
                track_gui_entry_t* tr = trackList.atNC(trackBegin);
                std::vector<automation_point_t> empty(0);
                for (int i = 0; i <= trackSEnd - trackSBegin; i++) {
                    int32_t subTrackIdx = trackSBegin + i;
                    if (tr->validSubtrack(subTrackIdx)) {
                        gui_track_subtrack* subtrack = tr->subtracks[subTrackIdx];
                        automated_param_t* automation     = subtrack->getAutomation();
                        if (automation) {
                            automation->setRange(tickBegin, tickEnd, empty);
                        }
                    }
                }
            }
        }
    }
    bool isSelectionEmpty(const track_gui_manager_i& trackList, const DAW::Cursor& _cursor, bool bIgnoreAutomation) {
        auto isEmpty = true;
        int32_t tickBegin     = _cursor.getTickBegin();
        int32_t tickEnd       = _cursor.getTickEnd();
        int32_t trackBegin    = _cursor.getTrackBegin();
        int32_t trackEnd      = _cursor.getTrackEnd();
        int32_t trackSubBegin = _cursor.getSubTrackBegin();
        int32_t trackSubEnd   = _cursor.getSubTrackEnd();
        if (_cursor.isSubtrackSelection() && !bIgnoreAutomation) {
            if (trackList.validTrackIdx(trackBegin)) {
                const track_gui_entry_t* const tr = trackList.at(trackBegin);
                for (int i = trackSubBegin; i <= trackSubEnd; i++) {
                    if (tr->validSubtrack(i)) {
                        const gui_track_subtrack* subtrack = tr->subtracks[i];
                        const automatable_t* automatable   = subtrack->at;
                        const automated_param_t* automation     = nullptr;
                        if (automatable) {
                            automation = automatable->getRegisteredConstAutomation(subtrack->param);
                        }

                        if (automation) {
                            auto optionalMinMax = automation->getBeginEnd();
                            if (optionalMinMax) {
                                auto minMax = optionalMinMax.value();
                                if (minMax.first <= tickBegin && minMax.second >= tickEnd) {
                                    isEmpty = false;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (!_cursor.isSubtrackSelection()) {
            auto selTrackRange = trackEnd - trackBegin;
            auto selRange      = tickEnd - tickBegin;
            for (int i = 0; i <= selTrackRange; i++) {
                std::vector<automation_clipboard_t> automationLanes;
                if (trackList.validTrackIdx(trackBegin + i)) {
                    const track_gui_entry_t* tr = trackList.at(trackBegin + i);
                    if (hasClipsInRange(tr->track->getConstMidi(), tickBegin, selRange)) {
                        isEmpty = false;
                        break;
                    }
                    if (bIgnoreAutomation)
                        continue;
                    auto trackImpl = tr->track->getStage();
                    std::vector<automatable_t*> targets;
                    trackImpl->getAutomatableTrackTargets(targets);
                    std::vector<automation_lane_t*> allParams;
                    for (auto& automatable : targets) {
                        allParams.clear();
                        automatable->getAllAutomatedParamRef(allParams);
                        for (const auto& automation : allParams) {
                            auto optionalMinMax = automation->getBeginEnd();
                            if (optionalMinMax) {
                                auto minMax = optionalMinMax.value();
                                if (minMax.first <= tickBegin && minMax.second >= tickEnd) {
                                    isEmpty = false;
                                    break;
                                }
                            }
                        }
                        if (!isEmpty) {
                            break;
                        }
                    }
                }
            }
        }
        return isEmpty;
    }

}// namespace DAW


void muteIntersectingClips(trackdata_midi_t& midi, tick_t tickBegin, tick_t tickEnd) {
    for (clip_t* c : midi.clips) {
        if (c->start() < tickEnd && c->end() >= tickBegin) {
            c->enabled = !c->enabled;
            c->setDirty();
        }
    }
}
void cutIntersectingClips(trackdata_midi_t& midi, tick_t tickBegin, tick_t tickEnd, delete_cb* cb) {
    auto it = midi.clips.begin();

    while (it != midi.clips.end()) {
        clip_t* c = *it;
        if (c->len == 0) {
            it = midi.removeClip(c);
            releaseClipResources(c, cb);
            delete c;
            continue;
        }
        if (c->start() >= tickEnd || c->end() < tickBegin) {
            it++;
            continue;
        }
        if (c->start() >= tickBegin && c->end() <= tickEnd) {
            it = midi.removeClip(c);
            releaseClipResources(c, cb);
            delete c;
            continue;
        }
        if (c->time >= tickBegin) {
            //cut left
            cutClipLeft(c, tickEnd - c->time);
            c->setDirty();
        } else if (c->end() <= tickEnd) {
            //cut right
            cutClipRight(c, c->end() - tickBegin);
            c->setDirty();
        } else {
            clip_t* c2 = c->clone();
            cutClipRight(c, c->end() - tickBegin);
            cutClipLeft(c2, tickEnd - c->time);
            it = midi.clips.insert(it, c2);
            c->setDirty();
        }
        it++;
    }
    midi.sortClips();
}

//TODO: rename
void DawInstance::cutIntersecting(track_t* tr, tick_t tickBegin, tick_t tickEnd) {
    cutIntersectingClips(tr->getMidi(), tickBegin, tickEnd, this);
}

//TODO: rename
void DawInstance::cutIntersecting(track_t* tr, clip_t* mask) {
    tick_t tickBegin = mask->time;
    tick_t tickEnd   = mask->end();
    cutIntersecting(tr, tickBegin, tickEnd);
}
