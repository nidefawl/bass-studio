#pragma once
/*
 * Based on https://github.com/craigsapp/midifile
 * Modifications (c) Michael Hept
 */
#include "MidiMessage.h"
#include <vector>

class MidiEvent final : public MidiMessage {
public:
    MidiEvent();
    explicit MidiEvent(int command);
    MidiEvent(int command, int param1);
    MidiEvent(int command, int param1, int param2);
    MidiEvent(int aTime, int aTrack, std::vector<unsigned char>& message);
    explicit MidiEvent(const MidiMessage& message);
    MidiEvent(const MidiEvent& mfevent);

    ~MidiEvent();

    MidiEvent& operator=(const MidiEvent& mfevent);
    MidiEvent& operator=(const MidiMessage& message);
    MidiEvent& operator=(const std::vector<unsigned char>& bytes);
    MidiEvent& operator=(const std::vector<char>& bytes);
    MidiEvent& operator=(const std::vector<int>& bytes);
    void clearVariables();

    /* functions related to event linking (note-ons to note-offs). */
    void unlinkEvent();
    void unlinkEvents();
    void linkEvent(MidiEvent* mev);
    void linkEvents(MidiEvent* mev);
    void linkEvent(MidiEvent& mev);
    void linkEvents(MidiEvent& mev);
    int isLinked();
    MidiEvent* getLinkedEvent();
    int getTickDuration();
    double getDurationInSeconds();

    int tick       = 0;
    int track      = 0;
    double seconds = 0.0;
    int seq        = 0;

private:
   /* used to match note-ons and note-offs */
    MidiEvent* eventlink = nullptr;
};
