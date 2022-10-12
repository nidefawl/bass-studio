#pragma once
/*
 * Based on https://github.com/craigsapp/midifile
 * Modifications (c) Michael Hept
 */
#include "MidiEvent.h"
#include <vector>


class MidiEventList {
public:
    MidiEventList() = default;

    ~MidiEventList();

    MidiEventList(const MidiEventList& other);
    MidiEventList(MidiEventList&& other) noexcept;

    MidiEvent& operator[](int index);
    const MidiEvent& operator[](int index) const;
    MidiEvent& back();
    MidiEvent& last();
    MidiEvent& getEvent(int index);
    void clear();
    void reserve(int rsize);
    int getSize() const;
    int size() const;
    int linkNotePairs();
    int linkEventPairs();
    void clearLinks();
    MidiEvent** data();

    int push(MidiEvent& event);
    int push_back(MidiEvent& event);
    int append(MidiEvent& event);

    // careful when using these, intended for internal use in MidiFile class:
    void detach();
    int push_back_no_copy(MidiEvent* event);

    MidiEventList& operator=(MidiEventList other);

private:
    std::vector<MidiEvent*> list;
};
