#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include "math/seq_math.h"
#include "midi/MidiFile.h"
#include "fileloader.h"
#include "logging.h"
#include "seq_time.h"
#include "host/clip/clip.h"
#include "host/track/track.h"
#include "host/daw/clipboard.h"
#include "fileio.h"

namespace {
    tick_t roundTickLenUp(tick_t len) {
        if (len > 0 && (len & (TICKS_BAR - 1)) == 0) return len;
        tick_t rndLen = TICKS_BAR;
        while (len < rndLen && rndLen > TICKS_16TH) {
            rndLen >>= 1;
        }
        return ((len + rndLen) / rndLen) * rndLen;
    }
}

namespace DAW {
    std::shared_ptr	<clip_clipboard> LoadMidiFile(const String& path) {
        using std::make_shared;
        using std::shared_ptr;
        if (!StrEndsWith(path, ".mid"))
            throw appexception(StringFormat("File %s is not a midi file\n", StringAsCStr(path)));
        if (!FileExists(path))
            throw appexception(StringFormat("File %s does not exist\n", StringAsCStr(path)));
        MidiFile midiFile;
        if (!midiFile.read(path)) {
            throw appexception(StringFormat("Failed reading %s\n", StringAsCStr(path)));
        }
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
            String trackName = "";
            clip_notes_t notes;
            clip_control_data_t controlData;
            MidiEventList& list = midiFile[track];
            list.linkEventPairs();
            auto numEventsTrack = list.getSize();
            if (!numEventsTrack) {
                log_lf(Log::L_WARN, "No events in midi track %d\n", track);
            }
            tick_t endOfTrack = -1;
            for (int event = 0; event < numEventsTrack; event++) {
                MidiEvent& evt = list[event];
                if (evt.isMeta()) {
                    // extract track name 
                    if (evt.getMetaType() == 0x03 && evt.size() > 3) {
                        auto textBegin = evt.data() + 3;
                        auto dataEnd = evt.data() + evt.size();
                        // find terminating null byte
                        auto end = std::find(textBegin, dataEnd, '\0');
                        trackName = String(textBegin, end);
                    }
                    if (evt.isEndOfTrack()) {
                        endOfTrack = (((evt.tick * 100) / tpqMidiFile) * TICKS_QUARTER) / 100;
                    }
                } else if (!evt.empty() && evt.isPitchbend()) {
                    auto pb = (evt.getP2() << 7) +  evt.getP1();
                    double pbValue = double(pb);
                    pbValue /= 8192.0 * 2.0;
                    auto tick = (((evt.tick * 100) / tpqMidiFile) * TICKS_QUARTER) / 100;
                    auto& shape = controlData.pitchBend.shape;
                    shape.pts.push_back({vec2(tick, pbValue), 0.5f});
                } else if (!evt.empty() && evt.isController()) {
                    auto controller = evt.getP1();
                    auto value = evt.getP2() / 127.0f;
                    auto& ccShape = controlData.getOrCreateChannel(controller).shape;
                    auto tick = (((evt.tick * 100) / tpqMidiFile) * TICKS_QUARTER) / 100;
                    ccShape.pts.push_back({vec2(tick, value), 0.5f});
                } else if (!evt.empty() && evt.isNoteOn()) {
                    MidiEvent* evt2 = evt.getLinkedEvent();
                    if (evt2 && evt2->isNoteOff()) {
                        tick_t start = evt.tick;
                        tick_t end   = evt2->tick;
                        int32_t key  = evt2->getKeyNumber();
                        note_t note;
                        note.time  = (((start * 100) / tpqMidiFile) * TICKS_QUARTER) / 100;
                        note.len   = ((((end - start) * 100) / tpqMidiFile) * TICKS_QUARTER) / 100;
                        note.pitch = key;
                        note.velocity = evt.getVelocity();
                        notes.m_list.push_back(note);
                        continue;
                    }
                }
            }
            if (!notes.isEmpty()) {
                shared_ptr<track_clipboard_t> trClipboard = make_shared<track_clipboard_t>();
                notes.updateBounds();
                auto posA = roundTickLenUp(notes.lastNote.start() - notes.firstNote.start());
                auto posB = roundTickLenUp(endOfTrack);
                // pick the position that is closer to the end of lastNote
                tick_t clipLength = posA;
                if (math::abs(notes.lastNote.end() - posA) > math::abs(notes.lastNote.end() - posB)) {
                    clipLength = posB;
                }

                if (trackName.empty()) {
                    String name;
                    SplitPath(path, nullptr, &name, nullptr);
                    trackName = name;
                }
                controlData.sort();
                controlData.eraseDuplicates();
                controlData.updateBounds();
                clip_t clip;
                clip.clipType = CLIP_MIDI;
                clip.name     = trackName;
                clip.notes       = std::move(notes);
                clip.controlData = std::move(controlData);
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
                    // clip->setLen(tickClipMax - tickClipMin);
                    clip->loopLen = clip->getLen();
                }
            }
            fileClipboard->srcTrack      = 0;
            fileClipboard->srcPos        = 0;
            fileClipboard->selRange      = tickClipMax - tickClipMin;
            fileClipboard->selTrackRange = static_cast<int32_t>(fileClipboard->tracks.size()) - 1;
            return fileClipboard;
        }
        return nullptr;
    }

    bool SaveMidiFile(const String& path, const clip_clipboard& clipboard) {
        auto defaultChannel = 0;
        auto defaultTempo = 128.0;
        MidiFile midiFile;
        midiFile.setTicksPerQuarterNote(TICKS_QUARTER);
        for (const auto& track : clipboard.tracks) {
            for (const auto& clip : track->clips) {
                auto trackIdx = midiFile.getTrackCount();
                auto& notes = clip->notes;
                auto& controlData = clip->controlData;
                auto trackName = clip->name;
                if (trackName.empty()) {
                    trackName = "Track " + std::to_string(trackIdx + 1);
                }
                midiFile.addTrack();
                midiFile.addTrackName(trackIdx, 0, trackName);
                midiFile.addTempo(trackIdx, 0, defaultTempo);
                midiFile.addTimeSignature(trackIdx, 0, 4, 4);
                auto notesCount = notes.m_list.size();
                for (size_t i = 0; i < notesCount; i++) {
                    auto& note = notes.m_list[i];
                    int key = note.pitch;
                    int velocity = note.velocity;
                    int start = note.time;
                    int end = start + note.len;
                    int channel = int(note.channel);
                    midiFile.addNoteOn(trackIdx, start, channel, key, velocity);
                    midiFile.addNoteOff(trackIdx, end, channel, key, velocity);
                }
                for (const auto& cc : controlData.ccChannels) {
                    auto& shape = cc.second.shape;
                    auto controller = cc.first;
                    // TODO: use sampleAtTick to support quadratic/exponential shapes
                    for (const auto& pt : shape.pts) {
                        auto tick = math::roundfS32(pt.pos.x);
                        auto ccScaled = math::clamp(pt.pos.y * 127.0f, 0.0f, 127.0f);
                        auto value = math::clamp<int32_t>(math::roundfS32(ccScaled), 0, 127);
                        midiFile.addController(trackIdx, tick, defaultChannel, controller, value);
                    }
                }
                auto& pbShape = controlData.pitchBend.shape;
                for (const auto& pt : pbShape.pts) {
                    auto tick = math::roundfS32(pt.pos.x);
                    auto pbScaled = math::clamp(pt.pos.y * 2.0f - 1.0f, -1.0f, 1.0f);
                    midiFile.addPitchBend(trackIdx, tick, defaultChannel, pbScaled);
                }
                auto clipDurationMidiTicks = clip->getLen();
                midiFile.addEndOfTrack(trackIdx, clipDurationMidiTicks);
            }
        }
        midiFile.sortTracks();
        return midiFile.write(path);
    }
} // namespace DAW

void LoadMidiTask::run() { 
    clipboard = DAW::LoadMidiFile(path);
    // Note: Exceptions are caught by the worker thread
}
