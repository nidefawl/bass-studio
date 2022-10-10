#pragma once
#include <cstdlib>
#include "midi-event.h"
#include "types.h"
#include "note.h"
#include "math/seq_math.h"
#include <vstsdk-host-2.4/aeffect.h>
#include <vstsdk-host-2.4/aeffectx.h>

struct VstEvent_t {
    static inline void ReallocVstEvents(VstEvent_t** handle, size_t size) {
        size = math::max((size_t) 128, size);
        if (*handle == nullptr || (*handle)->maxEvents < (int32_t) size) {
            if (*handle) delete *handle;
            *handle = new VstEvent_t(size);
        }
        (*handle)->reset();
    }
    int32_t maxEvents;
    VstEvents* vstEvents;
    VstMidiEvent* evtArr;
    int32_t numOns  = 0;
    int32_t numOffs = 0;

    explicit VstEvent_t(size_t s) : maxEvents(s) {
        /**
         * Allocates following struct equivalent to:
            struct VstEvents
            {
                VstInt32 numEvents;        ///< number of Events in array
                VstIntPtr reserved;        ///< zero (Reserved for future use)
                VstEvent* events[maxEvents];    ///< event pointer array, variable size
                VstMidiEvent midiEvents[maxEvents];
            };
         */

        size_t hdr = sizeof(VstEvents) + sizeof(VstEvent*) * (s - 2);
        size_t len = sizeof(VstMidiEvent) * (s);
        vstEvents  = static_cast<VstEvents*>(std::malloc(hdr));
        evtArr     = static_cast<VstMidiEvent*>(std::malloc(len));
        memset(vstEvents, 0, hdr);
        memset(evtArr, 0, len);
    }

    void reset() {
        numOns = numOffs = 0;
        //        vstEvents->numEvents = 0;
        //        memset(vstEvents->events, 0, sizeof(VstEvent)*maxEvents);
        memset(vstEvents, 0, sizeof(VstEvents) + sizeof(VstEvent*) * (maxEvents - 2));
        memset(evtArr, 0, sizeof(VstMidiEvent) * (maxEvents));
    }

    ~VstEvent_t() {
        std::free(vstEvents);
        std::free(evtArr);
    }

    void writeNoteOn(unsigned char* buf, int32_t pitch, int32_t velocity) {
        buf[0] = 0x90;
        buf[1] = CLAMP_I(pitch, 0, 0x7F);
        buf[2] = CLAMP_I(velocity, 0, 0x7F);
        buf[3] = 0;
    }

    void writeNoteOff(unsigned char* buf, int32_t pitch) {
        buf[0] = 0x80;
        buf[1] = CLAMP_I(pitch, 0, 0x7F);
        buf[2] = 0x40;
        buf[3] = 0;
    }
    VstMidiEvent& nextEvent() {
        dbgassert(vstEvents->numEvents < maxEvents);
        VstMidiEvent& evt = evtArr[vstEvents->numEvents];
        evt = {};
        evt.type        = kVstMidiType;
        evt.byteSize    = 24;//sizeof(VstMidiEvent);
        evt.flags       = 0; //kVstMidiEventIsRealtime;
        vstEvents->events[vstEvents->numEvents] = reinterpret_cast<VstEvent*>(&evt);
        vstEvents->numEvents++;
        return evt;
    }

    void sort() {
        std::sort(vstEvents->events, vstEvents->events + vstEvents->numEvents, [](VstEvent* a, VstEvent* b) {
            return a->deltaFrames < b->deltaFrames;
        });
    }

    void writeVstNoteEvent(const midievent_note_t& nevt, double tickToSamples, int32_t blockSize) {
        auto& evt = nextEvent();
        evt.deltaFrames = math::floordS32(nevt.tickOffsetInBlock * tickToSamples);
        dbgassert(evt.deltaFrames >= 0 && evt.deltaFrames < blockSize);
        if (nevt.isNoteOn) {
            numOns++;
            writeNoteOn((unsigned char*) evt.midiData, nevt.pitch, nevt.velocity);
        } else {
            numOffs++;
            writeNoteOff((unsigned char*) evt.midiData, nevt.pitch);
        }
    }

    void writeVstCtrlEvent(const DAW::Host::midievent_ctrl_t& nevt, int32_t sampleOffsetInBlock) {
        auto& evt = nextEvent();
        evt.deltaFrames = sampleOffsetInBlock;
        evt.midiData[0] = (nevt.message >> 0) & 0xFF;
        evt.midiData[1] = (nevt.message >> 8) & 0xFF;
        evt.midiData[2] = (nevt.message >> 16) & 0xFF;
        evt.midiData[3] = (nevt.message >> 24) & 0xFF;
    }

    void writeMessage(unsigned char c0, unsigned char c1, unsigned char c2, unsigned char c3, int32_t delta) {
        auto& evt = nextEvent();
        evt.deltaFrames    = 0;
        auto* buf = reinterpret_cast<unsigned char*>(evt.midiData);
        buf[0] = c0;
        buf[1] = c1;
        buf[2] = c2;
        buf[3] = c3;
    }

    void writeInstantOff() {
        /* Send all notes off midi event */
        writeMessage(0xB0, 123, 0, 0, 0);
    }
};