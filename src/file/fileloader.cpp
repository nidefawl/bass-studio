#include <exception>
#include <memory>

#include "fileloader.h"
#include "logging.h"
#include "host/track/track.h"
#include "host/daw/clipboard.h"
#include "plugins/synth/IPlugMidi.h"

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
            auto posA = roundUp(notes.lastNote.start() - notes.firstNote.start());
            auto posB = roundUp(endOfTrack);
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
        this->clipboard              = fileClipboard;
    }
}

void LoadMidiTask::run() { 
    loadFile();
}
