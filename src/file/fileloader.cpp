#include <exception>
#include <memory>

#include "fileloader.h"
#include "logging.h"
#include "track.h"
#include "clipboard.h"

tick_t roundUp(tick_t len) {
    if (len > 0 && (len & (TICKS_BAR - 1)) == 0) return len;
    tick_t rndLen = TICKS_BAR;
    while (len < rndLen && rndLen > TICKS_16TH) {
        rndLen >>= 1;
    }
    return ((len + rndLen) / rndLen) * rndLen;
}
void LoadMidiTask::loadFile() {
    using std::make_shared;
    using std::shared_ptr;
    try {
        if (StrEndsWith(path, ".mid") && FileExists(path)) {
            MidiFile midiFile;
            if (midiFile.read(path)) {
                shared_ptr<clip_clipboard> fileClipboard = make_shared<clip_clipboard>();
                tick_t tpqMidiFile = midiFile.getTicksPerQuarterNote();
                // tick_t scale = TICKS_QUARTER / tpqMidiFile; // they better use multiple of 8 or something
                int tracks = midiFile.getTrackCount();
                if (!tracks) {
                    log_lf(Log::L_WARN, "No tracks in midi file\n");
                }

                tick_t tickClipMin = -1;
                tick_t tickClipMax = -1;
                for (int track = 0; track < tracks; track++) {
                    clip_notes_t notes;
                    MidiEventList& list = midiFile[track];
                    list.linkEventPairs();
                    auto numEventsTrack = list.size();
                    if (!numEventsTrack) {
                        log_lf(Log::L_WARN, "No events in midi track %d\n", track);
                    }
                    int numBrokenEvents = 0;
                    int noteOnEvents = 0;
                    for (int event = 0; event < numEventsTrack; event++) {
                        MidiEvent& evt = list[event];
                        if (!evt.empty() && evt.isNoteOn()) {
                            noteOnEvents++;
                            MidiEvent* evt2 = evt.getLinkedEvent();
                            if (evt2 && evt2->isNoteOff()) {
                                tick_t start = evt.tick;
                                tick_t end   = evt2->tick;
                                int32_t key  = evt2->getKeyNumber();
                                note_t note;
                                note.time  = (((start * 100) / tpqMidiFile) * TICKS_QUARTER) / 100;
                                note.len   = ((((end - start) * 100) / tpqMidiFile) * TICKS_QUARTER) / 100;
                                note.pitch = key;
                                notes.m_list.push_back(note);
                                //log_printf("note %d %d - %d\n", key, start, end);
                                continue;
                            }
                            numBrokenEvents++;
                        }
                    }
                    log_lf(Log::L_DEBUG, "%d noteOnEvents in midi track %d\n", noteOnEvents, track);
                    if (numBrokenEvents) {
                        log_lf(Log::L_WARN, "%d invalid midi events on track %d\n", numBrokenEvents, track);
                    }
                    if (!notes.empty()) {
                        shared_ptr<track_clipboard_t> trClipboard = make_shared<track_clipboard_t>();
                        notes.updateBounds();
                        tick_t clipLength = roundUp(notes.lastNote.end() - notes.firstNote.start());

                        String filepath, name, ext;
                        SplitPath(path, &filepath, &name, &ext);
                        clip_t clip;
                        clip.clipType = CLIP_MIDI;
                        clip.name     = name;
                        // clip.notes = move(notes);
                        clip.notes       = notes;
                        clip.time        = 0;
                        clip.offsetStart = 0;
                        clip.setLen(clipLength);
                        clip.loopEnabled = false;
                        if (tickClipMin < 0) {
                            tickClipMin = clip.start();
                            tickClipMax = clip.end();
                        } else {
                            tickClipMin = math::min(tickClipMin, clip.start());
                            tickClipMax = math::max(tickClipMax, clip.end());
                        }
                        trClipboard->clips.push_back(make_shared<clip_t>(std::move(clip)));
                        fileClipboard->tracks.push_back(trClipboard);
                    }
                }
                if (!fileClipboard->tracks.empty()) {
                    for (auto& track : fileClipboard->tracks) {
                        for (auto& clip : track->clips) {
                            clip->setLen(tickClipMax - tickClipMin);
                            clip->loopLen = clip->getLen();
                        }
                    }
                    fileClipboard->srcTrack      = 0;
                    fileClipboard->srcPos        = 0;
                    fileClipboard->selRange      = tickClipMax - tickClipMin;
                    fileClipboard->selTrackRange = static_cast<int32_t>(fileClipboard->tracks.size()) - 1;
                    this->clipboard              = fileClipboard;
                }
            } else {
                log_lf(Log::L_WARN, "failed reading %s, its hard\n", StringAsCStr(path));
            }
        }
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "Exception loading midi file %s: %s\n", StringAsCStr(path), e.what());
    }
}
