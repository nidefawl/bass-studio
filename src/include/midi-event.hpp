#pragma once
#include "assert_dbg.h"
#include "types.hpp"
#include "note.hpp"
#include <vector>
#include <array>

struct MidiIOEvent {
    uint32_t message;
    int32_t timestamp;
};

struct midievent_note_t {
    int32_t pitch      = 0;
    int32_t velocity   = 127;
    tick_t tickOffsetInBlock = 0; // TODO: should be float/double for sub-tick precision
    tick_t globalTick  = 0;
    int8_t channel     = 0;
    bool isNoteOn      = false;
    bool isLoopNoteOff = false;
    note_t note;
    midievent_note_t(int32_t p, int32_t v, tick_t t, tick_t gt, bool b, bool b2, int8_t ch = 0)
        : pitch(p), velocity(v), tickOffsetInBlock(math::max(0, t)), globalTick(gt), channel(ch), isNoteOn(b), isLoopNoteOff(b2) {
    }
    midievent_note_t(const note_t& note, tick_t tickOffsetInBlock, tick_t globalTick, bool _isNoteOn, bool _isLoopNoteOff)
        : pitch(note.pitch), velocity(note.velocity), tickOffsetInBlock(math::max(0, tickOffsetInBlock)), globalTick(globalTick), channel(note.channel), isNoteOn(_isNoteOn), isLoopNoteOff(_isLoopNoteOff), note(note) {
    }
    String ToString() const;
};

template<typename Evt>
void InsertMidiEventSorted(std::vector<Evt>& list, Evt&& evt) {
    auto insertPos = std::find_if(list.begin(), list.end(), [tick = evt.globalTick](const auto& ev) { return ev.globalTick > tick;});
    list.insert(insertPos, evt);
}

template<typename Evt>
void InsertMidiEventSortedCopy(std::vector<Evt>& list, const Evt& evt) {
    auto insertPos = std::find_if(list.begin(), list.end(), [tick = evt.globalTick](const auto& ev) { return ev.globalTick > tick;});
    list.insert(insertPos, evt);
}

namespace DAW::Host {
    struct note_event_validator_t {
        std::array<int32_t, 128*16> prevNoteCounts{};
        int32_t numValidationErrors = 0;
        void validate(const std::vector<midievent_note_t>& evts) {
            for (const auto& note : evts) {
                auto& count = prevNoteCounts[note.pitch * 16 + note.channel];
                if (note.isNoteOn) {
                    count++;
                } else {
                    count--;
                }
                if (count > 1 || count < 0) {
                    log_lf(Log::L_ERROR, "Note %d %s has %d events\n", note.pitch, noteName(note.pitch), count);
                    // dbgassert(0);
                    if (numValidationErrors++ < 5) {
                        // Print all events
                        for (const auto& evt : evts) {
                            String asString = evt.ToString();
                            auto idx = &evt - &evts[0];
                            log_lf(Log::L_ERROR, "Evt[%zd] %s\n", idx, asString.c_str());
                        }
                    }
                }
            }
        }
        void reset() {
            prevNoteCounts.fill(0);
        }
    };

    struct midievent_ctrl_t {
        tick_t tick;
        uint32_t message;
        int32_t midiTime;
    };
    struct midi_input_events_t {
        std::vector<midievent_ctrl_t> m_list;
        void addMidiEvent(tick_t tick, uint32_t message, int32_t midiTime);
    };
    void sortControlEvents(std::vector<midievent_ctrl_t>& noteEvents);
    void sortNoteEvents(std::vector<midievent_note_t>& noteEvents);
    struct noteevent_buffer {
        tick_t currentTick = 0;
        std::vector<midievent_note_t> noteEvts;
        std::vector<midievent_ctrl_t> ctrlEvts;
        void update(tick_t blockStart, const std::vector<midievent_note_t>& _noteEvts, const std::vector<midievent_ctrl_t>& _ctrlEvts);
        void reset() {
            noteEvts.clear();
            ctrlEvts.clear();
        }
        void getNotesDelayed(tick_t blockStart, tick_t blockEnd, const double ticksPerBlock, std::vector<midievent_note_t>& noteEvtsOuts, std::vector<midievent_ctrl_t>& ctrlEvtsOut, int32_t midiChannelMatch, int32_t midiChannelRewrite);
    };
}