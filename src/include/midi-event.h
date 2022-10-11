#pragma once
#include "assert_dbg.h"
#include "types.h"
#include "note.h"
#include <vector>
#include <array>

struct MidiIOEvent {
    uint32_t message;
    int32_t timestamp;
};

struct midievent_note_t {
    int32_t pitch    = 0;
    int32_t velocity = 127;
    tick_t tickOffsetInBlock = 0; // TODO: should be float/double for sub-tick precision
    tick_t globalTick  = 0;
    bool isNoteOn      = false;
    bool isLoopNoteOff = false;
    midievent_note_t(int32_t p, int32_t v, tick_t t, tick_t gt, bool b, bool b2)
        : pitch(p), velocity(v), tickOffsetInBlock(math::max(0, t)), globalTick(gt), isNoteOn(b), isLoopNoteOff(b2) {
    }
    String ToString();
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
        std::array<int32_t, 128> prevNoteCounts{};
        void validate(const std::vector<midievent_note_t>& notes) {
            for (const auto& note : notes) {
                auto& count = prevNoteCounts[note.pitch];
                if (note.isNoteOn) {
                    count++;
                } else {
                    count--;
                }
                if (count > 1 || count < 0) {
                    log_lf(Log::L_ERROR, "Note %d %s has %d events\n", note.pitch, noteName(note.pitch), count);
                    dbgassert(0);
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
        void getNotesDelayed(tick_t tickLatencyCompensated, const double ticksPerBlock, std::vector<midievent_note_t>& noteEvtsOuts, std::vector<midievent_ctrl_t>& ctrlEvtsOut);
    };
}