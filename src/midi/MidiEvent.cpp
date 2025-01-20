/*
 * Based on https://github.com/craigsapp/midifile
 * Modifications (c) Michael Hept
 */
#include "MidiEvent.hpp"
#include <cstdlib>


MidiEvent::MidiEvent() : MidiMessage() {
    clearVariables();
}


MidiEvent::MidiEvent(int command) : MidiMessage(command) {
    clearVariables();
}


MidiEvent::MidiEvent(int command, int p1) : MidiMessage(command, p1) {
    clearVariables();
}


MidiEvent::MidiEvent(int command, int p1, int p2)
    : MidiMessage(command, p1, p2) {
    clearVariables();
}


MidiEvent::MidiEvent(int aTime, int aTrack, std::vector<uint8_t>& message)
    : MidiMessage(message) {
    tick      = aTime;
    track     = aTrack;
    eventlink = nullptr;
    seconds   = 0.0;
    seq       = 0;
}


MidiEvent::MidiEvent(const MidiEvent& mfevent)
    : MidiMessage(mfevent) {
    tick      = mfevent.tick;
    track     = mfevent.track;
    seconds   = mfevent.seconds;
    seq       = mfevent.seq;
    eventlink = nullptr;
    this->resize(mfevent.size());
    for (int i = 0; i < (int) this->size(); i++) {
        (*this)[i] = mfevent[i];
    }
}

MidiEvent::~MidiEvent() {
    tick  = -1;
    track = -1;
    this->resize(0);
    eventlink = nullptr;
}

/* Clear everything except MidiMessage data */
void MidiEvent::clearVariables() {
    tick      = 0;
    track     = 0;
    seconds   = 0.0;
    seq       = 0;
    eventlink = nullptr;
}

MidiEvent& MidiEvent::operator=(const MidiEvent& mfevent) {
    if (this == &mfevent) {
        return *this;
    }
    tick      = mfevent.tick;
    track     = mfevent.track;
    seconds   = mfevent.seconds;
    seq       = mfevent.seq;
    eventlink = nullptr;
    this->resize(mfevent.size());
    for (int i = 0; i < (int) this->size(); i++) {
        (*this)[i] = mfevent[i];
    }
    return *this;
}

MidiEvent& MidiEvent::operator=(const MidiMessage& message) {
    if (this == &message) {
        return *this;
    }
    clearVariables();
    this->resize(message.size());
    for (int i = 0; i < (int) this->size(); i++) {
        (*this)[i] = message[i];
    }
    return *this;
}

MidiEvent& MidiEvent::operator=(const std::vector<uint8_t>& bytes) {
    if (this == &bytes) {
        return *this;
    }
    clearVariables();
    this->resize(bytes.size());
    for (int i = 0; i < (int) this->size(); i++) {
        (*this)[i] = bytes[i];
    }
    return *this;
}

/* Disassociate this event with another. Also tell the other event to disassociate from this event */
void MidiEvent::unlinkEvent() {
    if (eventlink == nullptr) {
        return;
    }
    MidiEvent* mev = eventlink;
    eventlink      = nullptr;
    mev->unlinkEvent();
}

/* Make a link between two messages */
void MidiEvent::linkEvent(MidiEvent* mev) {
    if (mev->eventlink != nullptr) {
        // unlink other event if it is linked to something else;
        mev->unlinkEvent();
    }
    // if this is already linked to something else, then unlink:
    if (eventlink != nullptr) {
        eventlink->unlinkEvent();
    }
    unlinkEvent();

    mev->eventlink = this;
    eventlink      = mev;
}

void MidiEvent::linkEvent(MidiEvent& mev) {
    linkEvent(&mev);
}

/* Returns a linked event.  Usually
   this is the note-off message for a note-on message and vice-versa.
   Returns nullptr if there are no links. */
MidiEvent* MidiEvent::getLinkedEvent() {
    return eventlink;
}

/* Returns true if there is an event which is not nullptr.  This function is similar to getLinkedEvent(). */
int MidiEvent::isLinked() {
    return eventlink == nullptr ? 0 : 1;
}

/* For linked events (note-ons and note-offs),
   return the absolute tick time difference between the two events.
   The tick values are presumed to be in absolute tick mode rather than
   delta tick mode.  Returns 0 if not linked. */
int MidiEvent::getTickDuration() {
    MidiEvent* mev = getLinkedEvent();
    if (mev == nullptr) {
        return 0;
    }
    int tick2 = mev->tick;
    if (tick2 > tick) {
        return tick2 - tick;
    } else {
        return tick - tick2;
    }
}


/* For linked events (note-ons and note-offs) return the duration 
   of the note in seconds. The seconds analysis must be done first.
   Otherwise the duration will be reported as zero. */
double MidiEvent::getDurationInSeconds() {
    MidiEvent* mev = getLinkedEvent();
    if (mev == nullptr) {
        return 0;
    }
    double seconds2 = mev->seconds;
    if (seconds2 > seconds) {
        return seconds2 - seconds;
    } else {
        return seconds - seconds2;
    }
}
