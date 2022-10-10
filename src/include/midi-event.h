#pragma once
#include "types.h"
#include "note.h"
#include <vector>

struct MidiIOEvent {
    uint32_t message;
    int32_t timestamp;
};

struct midievent_note_t {
    int32_t pitch    = 0;
    int32_t velocity = 127;
    tick_t tickOffsetInBlock;
    tick_t globalTick  = 0;
    bool isNoteOn      = false;
    bool isLoopNoteOff = false;
    midievent_note_t(int32_t p, int32_t v, tick_t t, tick_t gt, bool b, bool b2)
        : pitch(p), velocity(v), tickOffsetInBlock(math::max(0, t)), globalTick(gt), isNoteOn(b), isLoopNoteOff(b2) {
    }
};
void sortNoteEvents(std::vector<midievent_note_t>& noteEvents);

namespace DAW::Host {

    struct midievent_ctrl_t {
        tick_t tick;
        uint32_t message;
        int32_t midiTime;
    };
    struct midi_input_events_t {
        std::vector<midievent_ctrl_t> m_list;
        void addMidiEvent(tick_t tick, uint32_t message, int32_t midiTime);
    };
}