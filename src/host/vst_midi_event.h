#pragma once
#include <cstdlib>
#include "types.h"
#include "note.h"
#include "math/seq_math.h"
#include <vstsdk-host-2.4/aeffect.h>
#include <vstsdk-host-2.4/aeffectx.h>

struct VstEvent_t {
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

    void writeVstMidiEvt(noteevent_t& nevt, double tickToSamples, int32_t blockSize) {
        int32_t idx = vstEvents->numEvents;
        dbgassert(idx < maxEvents);
        VstMidiEvent& evt = evtArr[idx];
        evt.type        = kVstMidiType;
        evt.byteSize    = 24;//sizeof(VstMidiEvent);
        evt.flags       = 0; //kVstMidiEventIsRealtime;
        evt.deltaFrames = math::floordS32(nevt.tickOffsetInBlock * tickToSamples);

        dbgassert(evt.deltaFrames >= 0 && evt.deltaFrames < blockSize);

        if (nevt.isNoteOn) {
            numOns++;
            writeNoteOn((unsigned char*) evt.midiData, nevt.pitch, nevt.velocity);
        } else {
            numOffs++;
            writeNoteOff((unsigned char*) evt.midiData, nevt.pitch);
        }

        vstEvents->events[idx] = reinterpret_cast<VstEvent*>(&evt);
        vstEvents->numEvents++;
    }

    void writeMessage(unsigned char c0, unsigned char c1, unsigned char c2, unsigned char c3, int32_t delta) {
        int32_t idx = vstEvents->numEvents;
        dbgassert(idx < maxEvents);
        VstMidiEvent& evt  = evtArr[idx];
        evt.type           = kVstMidiType;
        evt.byteSize       = 24;//sizeof(VstMidiEvent);
        evt.flags          = 0; //kVstMidiEventIsRealtime
        evt.deltaFrames    = 0;

        unsigned char* buf = (unsigned char*) evt.midiData;

        buf[0] = c0;
        buf[1] = c1;
        buf[2] = c2;
        buf[3] = c3;

        vstEvents->events[idx] = reinterpret_cast<VstEvent*>(&evt);
        vstEvents->numEvents++;
    }
    void writeInstantOff() {
        /* Send all notes off midi event */
        writeMessage(0xB0, 123, 0, 0, 0);
        //for (int32_t i = 0; i < vstEvents->numEvents; i++) {
        //    evtArr[i].deltaFrames = 0;
        //}
    }
};