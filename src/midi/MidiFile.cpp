/*
 * Based on https://github.com/craigsapp/midifile
 * Modifications (c) Michael Hept
 */
#include "MidiFile.h"
#include "midi/MidiEventList.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>

MidiFile::MidiFile(const std::string& _filename) {
    read(_filename);
}

MidiFile::~MidiFile() {
    for (auto* evt : tracks) {
        delete evt;
    }
}

bool MidiFile::read(const std::string& filename) {
    timemapvalid = 0;
    setFilename(filename);
    std::fstream input(filename, std::ios::binary | std::ios::in);
    if (!input.is_open()) {
        return false;
    }
    return readFromInputStream(input);
}

bool MidiFile::readFromInputStream(std::istream& input) {
    auto& filename = getFilename();
    // Read the MIDI header (4 bytes of ID, 4 byte data size,
    // anticipated 6 bytes of data.

    auto character = input.get();
    if (character == EOF) {
        std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
        std::cerr << "Expecting 'M' at first byte, but found nothing." << std::endl;
        return false;
    } else if (character != 'M') {
        std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
        std::cerr << "Expecting 'M' at first byte but got '"
                  << (char) character << "'" << std::endl;
        return false;
    }

    character = input.get();
    if (character == EOF) {
        std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
        std::cerr << "Expecting 'T' at second byte, but found nothing." << std::endl;
        return false;
    } else if (character != 'T') {
        std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
        std::cerr << "Expecting 'T' at second byte but got '"
                  << (char) character << "'" << std::endl;
        return false;
    }

    character = input.get();
    if (character == EOF) {
        std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
        std::cerr << "Expecting 'h' at third byte, but found nothing." << std::endl;
        return false;
    } else if (character != 'h') {
        std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
        std::cerr << "Expecting 'h' at third byte but got '"
                  << (char) character << "'" << std::endl;
        return false;
    }

    character = input.get();
    if (character == EOF) {
        std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
        std::cerr << "Expecting 'd' at fourth byte, but found nothing." << std::endl;
        return false;
    } else if (character != 'd') {
        std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
        std::cerr << "Expecting 'd' at fourth byte but got '"
                  << (char) character << "'" << std::endl;
        return false;
    }

    // read header size (allow larger header size?)
    auto hdrSizeU32 = readLittleEndian4Bytes(input);
    if (hdrSizeU32 != 6) {
        std::cerr << "File " << filename
                  << " is not a MIDI 1.0 Standard MIDI file." << std::endl;
        std::cerr << "The header size is " << hdrSizeU32 << " bytes." << std::endl;
        return false;
    }

    // Header parameter #1: format type
    auto fmtTypeU16 = readLittleEndian2Bytes(input);
    switch (fmtTypeU16) {
        case 0:
        case 1:
            break;
        case 2:
            // Type-2 MIDI files should probably be allowed as well,
            // but I have never seen one in the wild to test with.
        default:
            std::cerr << "Error: cannot handle a type-" << fmtTypeU16
                      << " MIDI file" << std::endl;
            return false;
    }

    // Header parameter #2: track count
    auto trackCountU16 = readLittleEndian2Bytes(input);
    if (fmtTypeU16 == 0 && trackCountU16 != 1) {
        std::cerr << "Error: Type 0 MIDI file can only contain one track" << std::endl;
        std::cerr << "Instead track count is: " << trackCountU16 << std::endl;
        return false;
    }
    clear();
    tracks.resize(trackCountU16);
    for (int i = 0; i < trackCountU16; i++) {
        tracks[i] = new MidiEventList;
    }

    // Header parameter #3: Ticks per quarter note
    auto tickPerQuarterU16 = readLittleEndian2Bytes(input);
    if (tickPerQuarterU16 >= 0x8000) {
        int framespersecond = 255 - ((tickPerQuarterU16 >> 8) & 0x00ff) + 1;
        int subframes       = tickPerQuarterU16 & 0x00ff;
        switch (framespersecond) {
            case 25:
                framespersecond = 25;
                break;
            case 24:
                framespersecond = 24;
                break;
            case 29:
                framespersecond = 29;
                break;// really 29.97 for color television
            case 30:
                framespersecond = 30;
                break;
            default:
                std::cerr << "Warning: unknown FPS: " << framespersecond << std::endl;
                std::cerr << "Using non-standard FPS: " << framespersecond << std::endl;
        }
        ticksPerQuarterNote = framespersecond * subframes;

        // std::cerr << "SMPTE ticks: " << ticksPerQuarterNote << " ticks/sec" << std::endl;
        // std::cerr << "SMPTE frames per second: " << framespersecond << std::endl;
        // std::cerr << "SMPTE subframes per frame: " << subframes << std::endl;
    } else {
        ticksPerQuarterNote = tickPerQuarterU16;
    }

    for (int i = 0; i < trackCountU16; i++) {
        character = input.get();
        if (character == EOF) {
            std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
            std::cerr << "Expecting 'M' at first byte in track, but found nothing."
                      << std::endl;
            return false;
        }
        if (character != 'M') {
            std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
            std::cerr << "Expecting 'M' at first byte in track but got '"
                      << (char) character << "'" << std::endl;
            return false;
        }

        character = input.get();
        if (character == EOF) {
            std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
            std::cerr << "Expecting 'T' at second byte in track, but found nothing."
                      << std::endl;
            return false;
        }
        if (character != 'T') {
            std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
            std::cerr << "Expecting 'T' at second byte in track but got '"
                      << (char) character << "'" << std::endl;
            return false;
        }

        character = input.get();
        if (character == EOF) {
            std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
            std::cerr << "Expecting 'r' at third byte in track, but found nothing."
                      << std::endl;
            return false;
        }
        if (character != 'r') {
            std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
            std::cerr << "Expecting 'r' at third byte in track but got '"
                      << (char) character << "'" << std::endl;
            return false;
        }

        character = input.get();
        if (character == EOF) {
            std::cerr << "In file " << filename << ": unexpected end of file." << std::endl;
            std::cerr << "Expecting 'k' at fourth byte in track, but found nothing."
                      << std::endl;
            return false;
        } else if (character != 'k') {
            std::cerr << "File " << filename << " is not a MIDI file" << std::endl;
            std::cerr << "Expecting 'k' at fourth byte in track but got '"
                      << (char) character << "'" << std::endl;
            return false;
        }

        // Now read track chunk size and throw it away because it is
        // not really necessary since the track MUST end with an
        // end of track meta event, and many MIDI files found in the wild
        // do not correctly give the track size.
        auto trackChunkSizeU32 = readLittleEndian4Bytes(input);

        // Set the size of the track allocation so that it might
        // approximately fit the data.
        size_t minReserve = trackChunkSizeU32 > 1000 ? 500 : trackChunkSizeU32 / 2;
        tracks[i]->clear();
        tracks[i]->reserve(minReserve);

        // Read MIDI events in the track, which are pairs of VLV values
        // and then the bytes for the MIDI message.  Running status messags
        // will be filled in with their implicit command byte.
        // The timestamps are converted from delta ticks to absolute ticks,
        // with the absticks variable accumulating the VLV tick values.
        int32_t absticks       = 0;
        uint8_t runningCommand = 0;
        std::vector<uint8_t> bytes;
        while (!input.eof()) {
            auto tickOffset = readVLValue(input);
            absticks += int32_t(tickOffset);
            auto xstatus = extractMidiData(input, bytes, runningCommand);
            if (xstatus == 0) {
                return false;
            }
            MidiEvent event;
            event.setMessage(bytes);
            event.tick  = absticks;
            event.track = i;

            if (bytes[0] == 0xff && bytes[1] == 0x2f) {
                // end-of-track message
                // comment out the following line if you don't want to see the
                // end of track message (which is always required, and will added
                // automatically when a MIDI is written, so it is not necessary.
                tracks[i]->push_back(event);
                break;
            }
            tracks[i]->push_back(event);
        }
    }

    midiTimingType = TIME_STATE_ABSOLUTE;

    // The original order of the MIDI events is marked with an enumeration which
    // allows for reconstruction of the order when merging/splitting tracks to/from
    // a type-0 configuration.
    markSequence();

    return true;
}

bool MidiFile::write(const std::string& filename) {
    std::fstream output(filename, std::ios::binary | std::ios::out);
    if (!output.is_open()) {
        std::cerr << "Error: could not write: " << filename << std::endl;
        return false;
    }
    return writeToOutputStream(output);
}

bool MidiFile::writeToOutputStream(std::ostream& out) {
    int oldTimeState = getTickState();
    if (oldTimeState == TIME_STATE_ABSOLUTE) {
        makeDeltaTicks();
    }

    // write the header of the Standard MIDI File
    const std::string header      = "MThd";
    const std::string trackHeader = "MTrk";
    out.write(header.c_str(), 4);

    // 2. write the size of the header (always a "6" stored in unsigned long (4 bytes))
    writeBigEndianULong(out, 6);

    // 3. MIDI file format, type 0, 1, or 2
    writeBigEndianUShort(out, static_cast<uint16_t>(getTrackCount() == 1 ? 0 : 1));

    // 4. write out the number of tracks.
    writeBigEndianUShort(out, static_cast<uint16_t>(getTrackCount()));

    // 5. write out the number of ticks per quarternote. (avoiding SMTPE for now)
    writeBigEndianUShort(out, static_cast<uint16_t>(getTicksPerQuarterNote()));

    // now write each track.
    std::vector<uint8_t> trackdata;
    uint8_t endoftrack[4] = { 0, 0xff, 0x2f, 0x00 };
    for (int i = 0; i < getTrackCount(); i++) {
        auto& track = *tracks[i];
        trackdata.clear();
        auto numEvents = static_cast<int32_t>(tracks[i]->size());
        for (int j = 0; j < numEvents; j++) {
            auto& evt = track[j];
            if (evt.empty()) {
                // Don't write empty events (probably a delete message).
                continue;
            }
            if (evt.isEndOfTrack()) {
                // Suppress end-of-track meta messages (one will be added
                // automatically after all track data has been written).
                continue;
            }
            writeVLValue(evt.tick, trackdata);
            auto eventSize = static_cast<int32_t>(evt.size());
            if ((evt.getCommandByte() == 0xf0) ||
                (evt.getCommandByte() == 0xf7)) {
                // 0xf0 == Complete sysex message (0xf0 is part of the raw MIDI).
                // 0xf7 == Raw byte message (0xf7 not part of the raw MIDI).
                // Print the first byte of the message (0xf0 or 0xf7), then
                // print a VLV length for the rest of the bytes in the message.
                // In other words, when creating a 0xf0 or 0xf7 MIDI message,
                // do not insert the VLV byte length yourself, as this code will
                // do it for you automatically.
                trackdata.push_back(evt[0]);// 0xf0 or 0xf7;
                writeVLValue(eventSize - 1, trackdata);
                for (int k = 1; k < eventSize; k++) {
                    trackdata.push_back(evt[k]);
                }
            } else {
                // non-sysex type of message, so just output the
                // bytes of the message:
                for (int k = 0; k < eventSize; k++) {
                    trackdata.push_back(evt[k]);
                }
            }
        }
        auto dataSize = trackdata.size();
        if ((dataSize < 3) || !((trackdata[dataSize - 3] == 0xff) && (trackdata[dataSize - 2] == 0x2f))) {
            trackdata.push_back(endoftrack[0]);
            trackdata.push_back(endoftrack[1]);
            trackdata.push_back(endoftrack[2]);
            trackdata.push_back(endoftrack[3]);
        }

        // now ready to write to MIDI file.

        // first write the track ID marker "MTrk":
        out.write(trackHeader.c_str(), 4);
        // A. write the size of the MIDI data to follow:
        writeBigEndianULong(out, static_cast<uint32_t>(trackdata.size()));

        // B. write the actual data
        out.write(reinterpret_cast<char*>(trackdata.data()), static_cast<std::streamsize>(trackdata.size()));
    }

    if (oldTimeState == TIME_STATE_ABSOLUTE) {
        makeAbsoluteTicks();
    }

    return true;
}

MidiEventList& MidiFile::operator[](int aTrack) {
    return *tracks[aTrack];
}

const MidiEventList& MidiFile::operator[](int aTrack) const {
    return *tracks[aTrack];
}

int MidiFile::getTrackCount() const {
    return (int) tracks.size();
}

/* MidiFile::markSequence -- Assign a sequence serial number to
  every MidiEvent in every track in the MIDI file.  This is
  useful if you want to preseve the order of MIDI messages in
  a track when they occur at the same tick time.  Particularly
  for use with joinTracks() or sortTracks().  markSequence will
  be done automatically when a MIDI file is read, in case the
  ordering of events occuring at the same time is important.
  Use clearSequence() to use the default sorting behavior of
  sortTracks(). */
void MidiFile::markSequence() {
    int sequence = 1;
    for (int i = 0; i < getTrackCount(); i++) {
        for (int j = 0; j < tracks[i]->size(); j++) {
            (*tracks[i])[j].seq = sequence++;
        }
    }
}

/* MidiFile::clearSequence -- Remove any seqence serial numbers from
  MidiEvents in the MidiFile.  This will cause the default ordering by
  sortTracks() to be used, in which case the ordering of MidiEvents
  occurding at the same tick may switch their ordering. */
void MidiFile::clearSequence() {
    for (int i = 0; i < getTrackCount(); i++) {
        for (int j = 0; j < tracks[i]->size(); j++) {
            (*tracks[i])[j].seq = 0;
        }
    }
}

/* MidiFile::joinTracks -- Interleave the data from all tracks,
  but keeping the identity of the tracks unique so that
  the function splitTracks can be called to split the
  tracks into separate units again.  The style of the
  MidiFile when read from a file is with tracks split.
  The original track index is stored in the MidiEvent::track
  variable. */
void MidiFile::joinTracks() {
    if (getTrackState() == TRACK_STATE_JOINED) {
        return;
    }
    if (getTrackCount() == 1) {
        return;
    }

    MidiEventList* joinedTrack;
    joinedTrack = new MidiEventList;

    int messagesum = 0;
    int length     = getTrackCount();
    int i, j;
    for (i = 0; i < length; i++) {
        messagesum += (*tracks[i]).size();
    }
    joinedTrack->reserve((int) (messagesum + 32 + messagesum * 0.1));

    int oldTimeState = getTickState();
    if (oldTimeState == TIME_STATE_DELTA) {
        makeAbsoluteTicks();
    }
    for (i = 0; i < length; i++) {
        for (j = 0; j < (int) tracks[i]->size(); j++) {
            joinedTrack->push_back_no_copy(&(*tracks[i])[j]);
        }
    }

    clear_no_deallocate();

    delete tracks[0];
    tracks.resize(0);
    tracks.push_back(joinedTrack);
    sortTracks();
    if (oldTimeState == TIME_STATE_DELTA) {
        makeDeltaTicks();
    }

    midiTrackState = TRACK_STATE_JOINED;
}

/* MidiFile::splitTracks -- Take the joined tracks and split them
  back into their separate track identities. */
void MidiFile::splitTracks() {
    if (getTrackState() == TRACK_STATE_SPLIT) {
        return;
    }
    int oldTimeState = getTickState();
    if (oldTimeState == TIME_STATE_DELTA) {
        makeAbsoluteTicks();
    }

    int maxTrack = 0;
    int i;
    int length = tracks[0]->size();
    for (i = 0; i < length; i++) {
        if ((*tracks[0])[i].track > maxTrack) {
            maxTrack = (*tracks[0])[i].track;
        }
    }
    int newtrackCount = maxTrack + 1;

    if (newtrackCount <= 1) {
        return;
    }

    MidiEventList* olddata = tracks[0];
    tracks[0]              = nullptr;
    tracks.resize(newtrackCount);
    for (i = 0; i < newtrackCount; i++) {
        tracks[i] = new MidiEventList;
    }

    int trackValue = 0;
    for (i = 0; i < length; i++) {
        trackValue = (*olddata)[i].track;
        tracks[trackValue]->push_back_no_copy(&(*olddata)[i]);
    }

    olddata->detach();
    delete olddata;

    if (oldTimeState == TIME_STATE_DELTA) {
        makeDeltaTicks();
    }

    midiTrackState = TRACK_STATE_SPLIT;
}

/* MidiFile::splitTracksByChannel -- Take the joined tracks and split them
  back into their separate track identities. */
void MidiFile::splitTracksByChannel() {
    joinTracks();
    if (getTrackState() == TRACK_STATE_SPLIT) {
        return;
    }

    int oldTimeState = getTickState();
    if (oldTimeState == TIME_STATE_DELTA) {
        makeAbsoluteTicks();
    }

    int maxTrack = 0;
    int i;
    MidiEventList& eventlist = *tracks[0];
    MidiEventList* olddata   = &eventlist;
    int length               = eventlist.size();
    for (i = 0; i < length; i++) {
        if (eventlist[i].size() == 0) {
            continue;
        }
        if ((eventlist[i][0] & 0xf0) == 0xf0) {
            // ignore system and meta messages.
            continue;
        }
        if (maxTrack < (eventlist[i][0] & 0x0f)) {
            maxTrack = eventlist[i][0] & 0x0f;
        }
    }
    int newtrackCount = maxTrack + 2;// + 1 for expression track

    if (newtrackCount <= 1) {
        // only one channel, so don't do anything (leave as Type-0 file).
        return;
    }

    tracks[0] = nullptr;
    tracks.resize(newtrackCount);
    for (i = 0; i < newtrackCount; i++) {
        tracks[i] = new MidiEventList;
    }

    int trackValue = 0;
    for (i = 0; i < length; i++) {
        trackValue = 0;
        if ((eventlist[i][0] & 0xf0) == 0xf0) {
            trackValue = 0;
        } else if (eventlist[i].size() > 0) {
            trackValue = (eventlist[i][0] & 0x0f) + 1;
        }
        tracks[trackValue]->push_back_no_copy(&eventlist[i]);
    }

    olddata->detach();
    delete olddata;

    if (oldTimeState == TIME_STATE_DELTA) {
        makeDeltaTicks();
    }

    midiTrackState = TRACK_STATE_SPLIT;
}

int MidiFile::getTrackState() {
    return midiTrackState;
}

int MidiFile::hasJoinedTracks() {
    return midiTrackState == TRACK_STATE_JOINED;
}

int MidiFile::hasSplitTracks() {
    return midiTrackState == TRACK_STATE_SPLIT;
}

/* MidiFile::getSplitTrack --  Return the track index when the MidiFile
  is in the split state.  This function returns the original track
  when the MidiFile is in the joined state.  The MidiEvent::track
  variable is used to store the original track index when the
  MidiFile is converted to the joined-track state. */
int MidiFile::getSplitTrack(int track, int index) {
    if (hasSplitTracks()) {
        return track;
    } else {
        return getEvent(track, index).track;
    }
}

int MidiFile::getSplitTrack(int index) {
    if (hasSplitTracks()) {
        return 0;
    } else {
        return getEvent(0, index).track;
    }
}

/* MidiFile::deltaTicks -- convert the time data to
    delta time, which means that the time field
    in the MidiEvent struct represents the time
    since the last event was played. When a MIDI file
    is read from a file, this is the default setting. */
void MidiFile::makeDeltaTicks() {
    if (getTickState() == TIME_STATE_DELTA) {
        return;
    }
    int i, j;
    int temp;
    int length    = getTrackCount();
    int* timedata = new int[length];
    for (i = 0; i < length; i++) {
        timedata[i] = 0;
        if (tracks[i]->size() > 0) {
            timedata[i] = (*tracks[i])[0].tick;
        } else {
            continue;
        }
        for (j = 1; j < (int) tracks[i]->size(); j++) {
            temp          = (*tracks[i])[j].tick;
            int deltatick = temp - timedata[i];
            if (deltatick < 0) {
                std::cerr << "Error: negative delta tick value: " << deltatick << std::endl
                          << "Timestamps must be sorted first"
                          << " (use MidiFile::sortTracks() before writing)." << std::endl;
            }
            (*tracks[i])[j].tick = deltatick;
            timedata[i]          = temp;
        }
    }
    midiTimingType = TIME_STATE_DELTA;
    delete[] timedata;
}

/* MidiFile::absoluteTicks -- convert the time data to
   absolute time, which means that the time field
   in the MidiEvent struct represents the exact tick
   time to play the event rather than the time since
   the last event to wait untill playing the current
   event. */
void MidiFile::makeAbsoluteTicks() {
    if (getTickState() == TIME_STATE_ABSOLUTE) {
        return;
    }
    int i, j;
    int length    = getTrackCount();
    int* timedata = new int[length];
    for (i = 0; i < length; i++) {
        timedata[i] = 0;
        if (tracks[i]->size() > 0) {
            timedata[i] = (*tracks[i])[0].tick;
        } else {
            continue;
        }
        for (j = 1; j < (int) tracks[i]->size(); j++) {
            timedata[i] += (*tracks[i])[j].tick;
            (*tracks[i])[j].tick = timedata[i];
        }
    }
    midiTimingType = TIME_STATE_ABSOLUTE;
    delete[] timedata;
}

int MidiFile::getTickState() {
    return midiTimingType;
}

int MidiFile::isDeltaTicks() {
    return midiTimingType == TIME_STATE_DELTA ? 1 : 0;
}

int MidiFile::isAbsoluteTicks() {
    return midiTimingType == TIME_STATE_ABSOLUTE ? 1 : 0;
}

void MidiFile::setFilename(const std::string& aname) {
    readFileName = aname;
}

const std::string& MidiFile::getFilename() {
    return readFileName;
}

int MidiFile::addEvent(int aTrack, int aTick, std::vector<uint8_t>& midiData) {
    timemapvalid = 0;
    MidiEvent anEvent;
    anEvent.tick  = aTick;
    anEvent.track = aTrack;
    anEvent.setMessage(midiData);

    tracks[aTrack]->push_back(anEvent);
    return tracks[aTrack]->size() - 1;
}

int MidiFile::addEvent(MidiEvent& mfevent) {
    if (getTrackState() == TRACK_STATE_JOINED) {
        tracks[0]->push_back(mfevent);
        return (int) tracks[0]->size() - 1;
    } else {
        tracks[mfevent.track]->push_back(mfevent);
        return (int) tracks[mfevent.track]->size() - 1;
    }
}

int MidiFile::addMetaEvent(int aTrack, int aTick, int aType, std::vector<uint8_t>& metaData) {
    timemapvalid = 0;
    int i;
    int length = (int) metaData.size();
    std::vector<uint8_t> fulldata;
    uint8_t size[23] = { 0 };
    int lengthsize   = makeVLV(size, length);

    fulldata.resize(2 + lengthsize + length);
    fulldata[0] = 0xff;
    fulldata[1] = aType & 0x7F;
    for (i = 0; i < lengthsize; i++) {
        fulldata[2 + i] = size[i];
    }
    for (i = 0; i < length; i++) {
        fulldata[2 + lengthsize + i] = metaData[i];
    }

    return addEvent(aTrack, aTick, fulldata);
}


int MidiFile::addMetaEvent(int aTrack, int aTick, int aType,
                           const std::string& metaData) {
    auto len = metaData.length();
    std::vector<uint8_t> buffer;
    buffer.resize(len);
    for (size_t i = 0; i < len; i++) {
        buffer[i] = metaData[i];
    }
    return addMetaEvent(aTrack, aTick, aType, buffer);
}

int MidiFile::addCopyright(int aTrack, int aTick, const std::string& text) {
    MidiEvent* me = new MidiEvent;
    me->makeCopyright(text);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

int MidiFile::addTrackName(int aTrack, int aTick, const std::string& name) {
    MidiEvent* me = new MidiEvent;
    me->makeTrackName(name);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

int MidiFile::addInstrumentName(int aTrack, int aTick, const std::string& name) {
    MidiEvent* me = new MidiEvent;
    me->makeInstrumentName(name);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

int MidiFile::addLyric(int aTrack, int aTick, const std::string& text) {
    MidiEvent* me = new MidiEvent;
    me->makeLyric(text);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

int MidiFile::addMarker(int aTrack, int aTick, const std::string& text) {
    MidiEvent* me = new MidiEvent;
    me->makeMarker(text);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

int MidiFile::addCue(int aTrack, int aTick, const std::string& text) {
    MidiEvent* me = new MidiEvent;
    me->makeCue(text);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

int MidiFile::addTempo(int aTrack, int aTick, double aTempo) {
    MidiEvent* me = new MidiEvent;
    me->makeTempo(aTempo);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

/* MidiFile::addTimeSignature -- Add a time signature meta message
     (meta #0x58).  The "bottom" parameter must be a power of two;
     otherwise, it will be set to the next highest power of two.

Default values:
    clocksPerClick     == 24 (quarter note)
    num32ndsPerQuarter ==  8 (8 32nds per quarter note)

Time signature of 4/4 would be:
   top    = 4
   bottom = 4 (converted to 2 in the MIDI file for 2nd power of 2).
   clocksPerClick = 24 (2 eighth notes based on num32ndsPerQuarter)
   num32ndsPerQuarter = 8

Time signature of 6/8 would be:
   top    = 6
   bottom = 8 (converted to 3 in the MIDI file for 3rd power of 2).
   clocksPerClick = 36 (3 eighth notes based on num32ndsPerQuarter)
   num32ndsPerQuarter = 8 */
int MidiFile::addTimeSignature(int aTrack, int aTick, int top, int bottom,
                               int clocksPerClick, int num32ndsPerQuarter) {
    MidiEvent* me = new MidiEvent;
    me->makeTimeSignature(top, bottom, clocksPerClick, num32ndsPerQuarter);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

/* MidiFile::addCompoundTimeSignature -- Add a time signature meta message
     (meta #0x58), where the clocksPerClick parameter is set to three
     eighth notes for compount meters such as 6/8 which represents
     two beats per measure.

Default values:
    clocksPerClick     == 36 (quarter note)
    num32ndsPerQuarter ==  8 (8 32nds per quarter note) */
int MidiFile::addCompoundTimeSignature(int aTrack, int aTick, int top,
                                       int bottom, int clocksPerClick, int num32ndsPerQuarter) {
    return addTimeSignature(aTrack, aTick, top, bottom, clocksPerClick,
                            num32ndsPerQuarter);
}

/* MidiFile::makeVLV --  This function is used to create
  size byte(s) for meta-messages.  If the size of the data
  in the meta-message is greater than 127, then the size
  should (?) be specified as a VLV. */
int MidiFile::makeVLV(uint8_t* buffer, int number) {

    uint32_t value = (uint32_t) number;

    if (value >= (1 << 28)) {
        std::cerr << "Error: Meta-message size too large to handle" << std::endl;
        buffer[0] = 0;
        buffer[1] = 0;
        buffer[2] = 0;
        buffer[3] = 0;
        return 1;
    }

    buffer[0] = (value >> 21) & 0x7f;
    buffer[1] = (value >> 14) & 0x7f;
    buffer[2] = (value >> 7) & 0x7f;
    buffer[3] = (value >> 0) & 0x7f;

    int i;
    int flag   = 0;
    int length = -1;
    for (i = 0; i < 3; i++) {
        if (buffer[i] != 0) {
            flag = 1;
        }
        if (flag) {
            buffer[i] |= 0x80;
        }
        if (length == -1 && buffer[i] >= 0x80) {
            length = 4 - i;
        }
    }

    if (length == -1) {
        length = 1;
    }

    if (length < 4) {
        for (i = 0; i < length; i++) {
            buffer[i] = buffer[4 - length + i];
        }
    }

    return length;
}

/* MidiFile::addNoteOn -- Add a note-on message to the given track at the
   given time in the given channel. */
int MidiFile::addNoteOn(int aTrack, int aTick, int aChannel, int key, int vel) {
    MidiEvent* me = new MidiEvent;
    me->makeNoteOn(aChannel, key, vel);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

/* MidiFile::addNoteOff -- Add a note-off message (using 0x80 messages). */
int MidiFile::addNoteOff(int aTrack, int aTick, int aChannel, int key,
                         int vel) {
    MidiEvent* me = new MidiEvent;
    me->makeNoteOff(aChannel, key, vel);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

/* MidiFile::addNoteOff -- Add a note-off message (using 0x90 messages with
  zero attack velocity). */
int MidiFile::addNoteOff(int aTrack, int aTick, int aChannel, int key) {
    MidiEvent* me = new MidiEvent;
    me->makeNoteOff(aChannel, key);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

/* MidiFile::addController -- Add a controller message in the given
   track at the given tick time in the given channel. */
int MidiFile::addController(int aTrack, int aTick, int aChannel,
                            int num, int value) {
    MidiEvent* me = new MidiEvent;
    me->makeController(aChannel, num, value);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

/* MidiFile::addPatchChange -- Add a patch-change message in the given
   track at the given tick time in the given channel. */
int MidiFile::addPatchChange(int aTrack, int aTick, int aChannel,
                             int patchnum) {
    MidiEvent* me = new MidiEvent;
    me->makePatchChange(aChannel, patchnum);
    me->tick = aTick;
    tracks[aTrack]->push_back_no_copy(me);
    return tracks[aTrack]->size() - 1;
}

/* MidiFile::addTimbre -- Add a patch-change message in the given
   track at the given tick time in the given channel.  Alias for
   MidiFile::addPatchChange(). */
int MidiFile::addTimbre(int aTrack, int aTick, int aChannel, int patchnum) {
    return addPatchChange(aTrack, aTick, aChannel, patchnum);
}

/* MidiFile::addPitchBend -- convert  number in the range from -1 to +1
    into two 7-bit numbers (smallest piece first)

  -1.0 maps to 0 (0x0000)
   0.0 maps to 8192 (0x2000 --> 0x40 0x00)
  +1.0 maps to 16383 (0x3FFF --> 0x7F 0x7F) */
int MidiFile::addPitchBend(int aTrack, int aTick, int aChannel, double amount) {
    timemapvalid = 0;
    amount += 1.0;
    int value = int(amount * 8192 + 0.5);

    // prevent any wrap-around in case of round-off errors
    if (value > 0x3fff) {
        value = 0x3fff;
    }
    if (value < 0) {
        value = 0;
    }

    int lsbint = 0x7f & value;
    int msbint = 0x7f & (value >> 7);

    std::vector<uint8_t> mididata;
    mididata.resize(3);
    if (aChannel < 0) {
        aChannel = 0;
    } else if (aChannel > 15) {
        aChannel = 15;
    }
    mididata[0] = uint8_t(0xe0 | aChannel);
    mididata[1] = uint8_t(lsbint);
    mididata[2] = uint8_t(msbint);

    return addEvent(aTrack, aTick, mididata);
}

int MidiFile::addTrack() {
    int length = getTrackCount();
    tracks.resize(length + 1);
    tracks[length] = new MidiEventList;
    tracks[length]->reserve(10000);
    tracks[length]->clear();
    return length;
}

int MidiFile::addTrack(int count) {
    int length = getTrackCount();
    tracks.resize(length + count);
    int i;
    for (i = 0; i < count; i++) {
        tracks[length + i] = new MidiEventList;
        tracks[length + i]->reserve(10000);
        tracks[length + i]->clear();
    }
    return length + count - 1;
}

void MidiFile::allocateEvents(int track, int aSize) {
    int oldsize = tracks[track]->size();
    if (oldsize < aSize) {
        tracks[track]->reserve(aSize);
    }
}

/* MidiFile::deleteTrack -- remove a track from the MidiFile.
  Tracks are numbered starting at track 0. */
void MidiFile::deleteTrack(int aTrack) {
    int length = getTrackCount();
    if (aTrack < 0 || aTrack >= length) {
        return;
    }
    if (length == 1) {
        return;
    }
    tracks.erase(tracks.begin() + aTrack);
}

/* MidiFile::clear -- make the MIDI file empty with one
    track with no data in it. */
void MidiFile::clear() {
    for (int i = 0; i < getTrackCount(); i++) {
        delete tracks[i];
    }
    timemapvalid = 0;
    timemap.clear();
    midiTrackState = TRACK_STATE_SPLIT;
    midiTimingType = TIME_STATE_ABSOLUTE;
}


void MidiFile::erase() {
    clear();
}

/* MidiFile::getEvent -- return the event at the given index in the
   specified track. */
MidiEvent& MidiFile::getEvent(int aTrack, int anIndex) {
    return (*tracks[aTrack])[anIndex];
}

/* MidiFile::getTicksPerQuarterNote -- returns the number of
  time units that are supposed to occur during a quarternote. */
int MidiFile::getTicksPerQuarterNote() {
    if (ticksPerQuarterNote == 0xE728) {
        // this is a special case which is the SMPTE time code
        // setting for 25 frames a second with 40 subframes
        // which means one tick per millisecond.  When SMPTE is
        // being used, there is no real concept of the quarter note,
        // so presume 60 bpm as a simiplification here.
        // return 1000;
    }
    return ticksPerQuarterNote;
}

/* MidiFile::getEventCount -- returns the number of events
  in a given track. */
int MidiFile::getEventCount(int aTrack) {
    return tracks[aTrack]->size();
}


int MidiFile::getNumEvents(int aTrack) {
    return tracks[aTrack]->size();
}

/* MidiFile::mergeTracks -- combine the data from two
  tracks into one.  Placing the data in the first
  track location listed, and Moving the other tracks
  in the file around to fill in the spot where Track2
  used to be.  The results of this function call cannot
  be reversed. */
void MidiFile::mergeTracks(int aTrack1, int aTrack2) {
    MidiEventList* mergedTrack;
    mergedTrack      = new MidiEventList;
    int oldTimeState = getTickState();
    if (oldTimeState == TIME_STATE_DELTA) {
        makeAbsoluteTicks();
    }

    int length = getTrackCount();
    for (int i = 0; i < (int) tracks[aTrack1]->size(); i++) {
        mergedTrack->push_back((*tracks[aTrack1])[i]);
    }
    for (int i = 0; i < (int) tracks[aTrack2]->size(); i++) {
        (*tracks[aTrack2])[i].track = aTrack1;
        mergedTrack->push_back((*tracks[aTrack2])[i]);
    }

    sortTrack(*mergedTrack);

    delete tracks[aTrack1];

    tracks[aTrack1] = mergedTrack;

    for (int i = aTrack2; i < length - 1; i++) {
        tracks[i]   = tracks[i + 1];
        auto& track = *tracks[i];
        for (int j = 0; j < (int) track.size(); j++) {
            track[j].track = i;
        }
    }

    tracks[length - 1] = nullptr;
    tracks.resize(length - 1);

    if (oldTimeState == TIME_STATE_DELTA) {
        makeDeltaTicks();
    }
}

void MidiFile::setTicksPerQuarterNote(int ticks) {
    ticksPerQuarterNote = ticks;
}

/* MidiFile::setMillisecondTicks -- set the ticks per quarter note
  value to milliseconds.  The format for this specification is
  highest 8-bits: SMPTE Frame rate (as a negative 2's compliment value).
  lowest 8-bits: divisions per frame (as a positive number).
  for millisecond resolution, the SMPTE value is -25, and the
  frame rate is 40 frame per division.  In hexadecimal, these
  values are: -25 = 1110,0111 = 0xE7 and 40 = 0010,1000 = 0x28
  So setting the ticks per quarter note value to 0xE728 will cause
  delta times in the MIDI file to represent milliseconds. */
void MidiFile::setMillisecondTicks() {
    ticksPerQuarterNote = 0xE728;
}

/* MidiFile::sortTrack -- */
void MidiFile::sortTrack(MidiEventList& trackData) {
    if (midiTimingType == TIME_STATE_ABSOLUTE) {
        qsort(trackData.data(), trackData.size(), sizeof(MidiEvent*), eventcompare);
    }
}

/* MidiFile::sortTracks -- sort all tracks in the MidiFile. */
void MidiFile::sortTracks() {
    if (midiTimingType == TIME_STATE_ABSOLUTE) {
        for (int i = 0; i < getTrackCount(); i++) {
            sortTrack(*tracks[i]);
        }
    }
}

/* MidiFile::getTrackCountAsType1 --  Return the number of tracks in the
   MIDI file.  Returns the size of the events if not in joined state.
   If in joined state, reads track 0 to find the maximum track
   value from the original unjoined tracks. */
int MidiFile::getTrackCountAsType1() {
    if (getTrackState() == TRACK_STATE_JOINED) {
        int output = 0;
        int i;
        for (i = 0; i < (int) tracks[0]->size(); i++) {
            if (getEvent(0, i).track > output) {
                output = getEvent(0, i).track;
            }
        }
        return output + 1;// I think the track values are 0 offset...
    } else {
        return (int) tracks.size();
    }
}

/* MidiFile::getTimeInSeconds -- return the time in seconds for
    the current message. */
double MidiFile::getTimeInSeconds(int aTrack, int anIndex) {
    return getTimeInSeconds(getEvent(aTrack, anIndex).tick);
}


double MidiFile::getTimeInSeconds(int tickvalue) {
    if (timemapvalid == 0) {
        buildTimeMap();
        if (timemapvalid == 0) {
            return -1.0;// something went wrong
        }
    }

    _TickTime key;
    key.tick    = tickvalue;
    key.seconds = -1;

    void* ptr = bsearch(&key, timemap.data(), timemap.size(),
                        sizeof(_TickTime), ticksearch);

    if (ptr == nullptr) {
        // The specific tick value was not found, so do a linear
        // search for the two tick values which occur before and
        // after the tick value, and do a linear interpolation of
        // the time in seconds values to figure out the final
        // time in seconds.
        // Since the code is not yet written, kill the program at this point:
        return linearSecondInterpolationAtTick(tickvalue);
    } else {
        return ((_TickTime*) ptr)->seconds;
    }
}

/* MidiFile::getAbsoluteTickTime -- return the tick value represented
   by the input time in seconds.  If there is not tick entry at
   the given time in seconds, then interpolate between two values. */
int MidiFile::getAbsoluteTickTime(double starttime) {
    if (timemapvalid == 0) {
        buildTimeMap();
        if (timemapvalid == 0) {
            return -1.0;// something went wrong
        }
    }

    _TickTime key;
    key.tick    = -1;
    key.seconds = starttime;

    void* ptr = bsearch(&key, timemap.data(), timemap.size(),
                        sizeof(_TickTime), secondsearch);

    if (ptr == nullptr) {
        // The specific seconds value was not found, so do a linear
        // search for the two time values which occur before and
        // after the given time value, and do a linear interpolation of
        // the time in tick values to figure out the final time in ticks.
        return linearTickInterpolationAtSecond(starttime);
    } else {
        return ((_TickTime*) ptr)->tick;
    }
}

/* MidiFile::getTotalTimeInSeconds -- Returns the duration of the MidiFile
   event list in seconds.  If doTimeAnalysis() is not called before this
   function is called, it will be called automatically. */
double MidiFile::getTotalTimeInSeconds() {
    if (timemapvalid == 0) {
        buildTimeMap();
        if (timemapvalid == 0) {
            return -1.0;// something went wrong
        }
    }
    double output = 0.0;
    for (int i = 0; i < (int) tracks.size(); i++) {
        if (tracks[i]->last().seconds > output) {
            output = tracks[i]->last().seconds;
        }
    }
    return output;
}

/* MidiFile::getTotalTimeInTicks -- Returns the absolute tick value for the
   latest event in any track.  If the MidiFile is in TIME_STATE_DELTA,
   then temporarily got into TIME_STATE_ABSOLUTE to do the calculations.
   Note that this is expensive, so you should normally call this function
   while in aboslute tick mode. */
int MidiFile::getTotalTimeInTicks() {
    int oldTimeState = getTickState();
    if (oldTimeState == TIME_STATE_DELTA) {
        makeAbsoluteTicks();
    }
    if (oldTimeState == TIME_STATE_DELTA) {
        makeDeltaTicks();
    }
    int output = 0.0;
    for (int i = 0; i < (int) tracks.size(); i++) {
        if (tracks[i]->last().tick > output) {
            output = tracks[i]->last().tick;
        }
    }
    return output;
}

/* MidiFile::getTotalTimeInQuarters -- Returns the Duration of the MidiFile
   in units of quarter notes.  If the MidiFile is in TIME_STATE_DELTA,
   then temporarily got into TIME_STATE_ABSOLUTE to do the calculations.
   Note that this is expensive, so you should normally call this function
   while in aboslute tick mode. */
double MidiFile::getTotalTimeInQuarters() {
    double totalTicks = getTotalTimeInTicks();
    return totalTicks / getTicksPerQuarterNote();
}

/* MidiFile::doTimeAnalysis -- Identify the real-time position of
   all events by monitoring the tempo in relations to the tick
   times in the file. */
void MidiFile::doTimeAnalysis() {
    buildTimeMap();
}

/* MidiFile::linkNotePairs --  Link note-ons to note-offs separately
    for each track.  Returns the total number of note message pairs
    that were linked. */
int MidiFile::linkNotePairs() {
    int i;
    int sum = 0;
    for (i = 0; i < getTrackCount(); i++) {
        if (tracks[i] == nullptr) {
            continue;
        }
        sum += tracks[i]->linkNotePairs();
    }
    return sum;
}

int MidiFile::linkEventPairs() {
    return linkNotePairs();
}

void MidiFile::clearLinks() {
    for (int i = 0; i < getTrackCount(); i++) {
        if (tracks[i] == nullptr) {
            continue;
        }
        tracks[i]->clearLinks();
    }
}

/* MidiFile::linearTickInterpolationAtSecond -- return the tick value at the
   given input time. */
int MidiFile::linearTickInterpolationAtSecond(double seconds) {
    if (timemapvalid == 0) {
        buildTimeMap();
        if (timemapvalid == 0) {
            return -1.0;// something went wrong
        }
    }

    int i;
    double lasttime = timemap[timemap.size() - 1].seconds;
    // give an error value of -1 if time is out of range of data.
    if (seconds < 0.0) {
        return -1;
    }
    if (seconds > timemap[timemap.size() - 1].seconds) {
        return -1;
    }

    // Guess which side of the list is closest to target:
    // Could do a more efficient algorithm since time values are sorted,
    // but good enough for now...
    int startindex = -1;
    if (seconds < lasttime / 2) {
        for (i = 0; i < (int) timemap.size(); i++) {
            if (timemap[i].seconds > seconds) {
                startindex = i - 1;
                break;
            } else if (timemap[i].seconds == seconds) {
                startindex = i;
                break;
            }
        }
    } else {
        for (i = (int) timemap.size() - 1; i > 0; i--) {
            if (timemap[i].seconds < seconds) {
                startindex = i + 1;
                break;
            } else if (timemap[i].seconds == seconds) {
                startindex = i;
                break;
            }
        }
    }

    if (startindex < 0) {
        return -1;
    }
    if (startindex >= (int) timemap.size() - 1) {
        return -1;
    }

    double x1 = timemap[startindex].seconds;
    double x2 = timemap[startindex + 1].seconds;
    double y1 = timemap[startindex].tick;
    double y2 = timemap[startindex + 1].tick;
    double xi = seconds;

    return (xi - x1) * ((y2 - y1) / (x2 - x1)) + y1;
}

/* MidiFile::linearSecondInterpolationAtTick -- return the time in seconds
   value at the given input tick time. (Ticks input could be made double). */
double MidiFile::linearSecondInterpolationAtTick(int ticktime) {
    if (timemapvalid == 0) {
        buildTimeMap();
        if (timemapvalid == 0) {
            return -1.0;// something went wrong
        }
    }

    int i;
    double lasttick = timemap[timemap.size() - 1].tick;
    // give an error value of -1 if time is out of range of data.
    if (ticktime < 0.0) {
        return -1;
    }
    if (ticktime > timemap.back().tick) {
        return -1;// don't try to extrapolate
    }

    // Guess which side of the list is closest to target:
    // Could do a more efficient algorithm since time values are sorted,
    // but good enough for now...
    int startindex = -1;
    if (ticktime < lasttick / 2) {
        for (i = 0; i < (int) timemap.size(); i++) {
            if (timemap[i].tick > ticktime) {
                startindex = i - 1;
                break;
            } else if (timemap[i].tick == ticktime) {
                startindex = i;
                break;
            }
        }
    } else {
        for (i = (int) timemap.size() - 1; i > 0; i--) {
            if (timemap[i].tick < ticktime) {
                startindex = i;
                break;
            } else if (timemap[i].tick == ticktime) {
                startindex = i;
                break;
            }
        }
    }

    if (startindex < 0) {
        return -1;
    }
    if (startindex >= (int) timemap.size() - 1) {
        return -1;
    }

    if (timemap[startindex].tick == ticktime) {
        return timemap[startindex].seconds;
    }

    double x1 = timemap[startindex].tick;
    double x2 = timemap[startindex + 1].tick;
    double y1 = timemap[startindex].seconds;
    double y2 = timemap[startindex + 1].seconds;
    double xi = ticktime;

    return (xi - x1) * ((y2 - y1) / (x2 - x1)) + y1;
}

/* MidiFile::buildTimeMap -- build an index of the absolute tick values
     found in a MIDI file, and their corresponding time values in
     seconds, taking into consideration tempo change messages.  If no
     tempo messages are given (or until they are given, then the
     tempo is set to 120 beats per minute).  If SMPTE time code is
     used, then ticks are actually time values.  So don't build
     a time map for SMPTE ticks, and just calculate the time in
     seconds from the tick value (1000 ticks per second SMPTE
     is the only mode tested (25 frames per second and 40 subframes
     per frame). */
void MidiFile::buildTimeMap() {

    // convert the MIDI file to absolute time representation
    // in single track mode (and undo if the MIDI file was not
    // in that state when this function was called.
    //
    int trackstate = getTrackState();
    int timestate  = getTickState();

    makeAbsoluteTicks();
    joinTracks();

    int allocsize = getNumEvents(0);
    timemap.reserve(allocsize + 10);
    timemap.clear();

    _TickTime value;

    int lasttick = 0;
    int curtick;
    int tickinit = 0;

    int i;
    int tpq               = getTicksPerQuarterNote();
    double defaultTempo   = 120.0;
    double secondsPerTick = 60.0 / (defaultTempo * tpq);

    double lastsec = 0.0;
    double cursec  = 0.0;

    for (i = 0; i < getNumEvents(0); i++) {
        curtick                = getEvent(0, i).tick;
        getEvent(0, i).seconds = cursec;
        if ((curtick > lasttick) || !tickinit) {
            tickinit = 1;

            // calculate the current time in seconds:
            cursec                 = lastsec + (curtick - lasttick) * secondsPerTick;
            getEvent(0, i).seconds = cursec;

            // store the new tick to second mapping
            value.tick    = curtick;
            value.seconds = cursec;
            timemap.push_back(value);
            lasttick = curtick;
            lastsec  = cursec;
        }

        // update the tempo if needed:
        if (getEvent(0, i).isTempo()) {
            secondsPerTick = getEvent(0, i).getTempoSPT(getTicksPerQuarterNote());
        }
    }

    // reset the states of the tracks or time values if necessary here:
    if (timestate == TIME_STATE_DELTA) {
        makeDeltaTicks();
    }
    if (trackstate == TRACK_STATE_SPLIT) {
        splitTracks();
    }

    timemapvalid = 1;
}

/* MidiFile::extractMidiData -- Extract MIDI data from input
   stream.  Return value is 0 if failure; otherwise, returns 1. */
int MidiFile::extractMidiData(std::istream& input, std::vector<uint8_t>& array, uint8_t& runningCommand) {
    using std::cerr;
    using std::endl;
    array.clear();

    auto character = input.get();
    if (character == EOF) {
        cerr << "Error: unexpected end of file." << endl;
        return 0;
    }
    auto byte = uint8_t(character);

    if (byte < 0x80 && runningCommand == 0) {
        cerr << "Error: running command with no previous command" << endl;
        return 0;
    }
    if (byte < 0x80 && runningCommand >= 0xf0) {
        cerr << "Error: running status not permitted with meta and sysex event." << endl;
        return 0;
    }

    bool runningQ = byte < 0x80 ? true : false;
    if (!runningQ) {
        runningCommand = byte;
    }

    array.push_back(runningCommand);
    if (runningQ) {
        array.push_back(byte);
    }

    int i;
    uint8_t metai;
    switch (runningCommand & 0xf0) {
        case 0x80:// note off (2 more bytes)
        case 0x90:// note on (2 more bytes)
        case 0xA0:// aftertouch (2 more bytes)
        case 0xB0:// cont. controller (2 more bytes)
        case 0xE0:// pitch wheel (2 more bytes)
            byte = MidiFile::readByte(input);
            array.push_back(byte);
            if (!runningQ) {
                byte = MidiFile::readByte(input);
                array.push_back(byte);
            }
            break;
        case 0xC0:// patch change (1 more byte)
        case 0xD0:// channel pressure (1 more byte)
            if (!runningQ) {
                byte = MidiFile::readByte(input);
                array.push_back(byte);
            }
            break;
        case 0xF0:
            switch (runningCommand) {
                case 0xff:// meta event
                {
                    if (!runningQ) {
                        byte = MidiFile::readByte(input);// meta type
                        array.push_back(byte);
                    }
                    metai = MidiFile::readByte(input);// meta type
                    array.push_back(metai);
                    for (uint8_t j = 0; j < metai; j++) {
                        byte = MidiFile::readByte(input);// meta type
                        array.push_back(byte);
                    }
                } break;
                // The 0xf0 and 0xf7 meta commands deal with system-exclusive
                // messages. 0xf0 is used to either start a message or to store
                // a complete message.  The 0xf0 is part of the outgoing MIDI
                // bytes.  The 0xf7 message is used to send arbitrary bytes,
                // typically the middle or ends of system exclusive messages.  The
                // 0xf7 byte at the start of the message is not part of the
                // outgoing raw MIDI bytes, but is kept in the MidiFile message
                // to indicate a raw MIDI byte message (typically a partial
                // system exclusive message).
                case 0xf7:// Raw bytes. 0xf7 is not part of the raw
                          // bytes, but are included to indicate
                          // that this is a raw byte message.
                case 0xf0:// System Exclusive message
                {         // (complete, or start of message).
                    int length = (int) readVLValue(input);
                    for (i = 0; i < length; i++) {
                        byte = MidiFile::readByte(input);
                        array.push_back(byte);
                    }
                } break;
                    // other "F" MIDI commands are not expected, but can be
                    // handled here if they exist.
            }
            break;
        default:
            std::cout << "Error reading midifile" << std::endl;
            std::cout << "Command byte was " << (int) runningCommand << std::endl;
            return 0;
    }
    return 1;
}

/* MidiFile::readVLValue -- The VLV value is expected to be unpacked into
  a 4-byte integer no greater than 0x0fffFFFF, so a VLV value up to
  4-bytes in size (FF FF FF 7F) will only be considered.  Longer
  VLV values are not allowed in standard MIDI files, so the extract
  delta time would be truncated and the extra byte(s) will be parsed
  incorrectly as a MIDI command. */
uint32_t MidiFile::readVLValue(std::istream& input) {
    uint8_t b[4] = { 0 };

    for (int i = 0; i < 4; i++) {
        b[i] = MidiFile::readByte(input);
        if (b[i] < 0x80) {
            break;
        }
    }

    return unpackVLV(b[0], b[1], b[2], b[3]);
}

/* MidiFile::unpackVLV -- converts a VLV value to an uint32_t value.
    The bytes a, b, c, d are in big-endian order (the order they would
    be read out of the MIDI file).
default values: a = b = c = d = 0; */
uint32_t MidiFile::unpackVLV(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    if (d > 0x7f) {
        std::cerr << "Error: VLV value was too long" << std::endl;
        return 0;
    }

    uint8_t bytes[4] = { a, b, c, d };
    int count        = 0;
    while (bytes[count] > 0x7f && count < 4) {
        count++;
    }
    count++;

    uint32_t output = 0;
    for (int i = 0; i < count; i++) {
        output = output << 7;
        output = output | (bytes[i] & 0x7f);
    }

    return output;
}

/* MidiFile::writeVLValue -- write a number to the midifile
   as a variable length value which segments a file into 7-bit
   values and adds a contination bit to each.  Maximum size of input
   aValue is 0x0FFFffff. */
void MidiFile::writeVLValue(long aValue, std::vector<uint8_t>& outdata) {
    uint8_t bytes[4] = { 0 };

    if ((uint32_t) aValue >= (1 << 28)) {
        std::cerr << "Error: number too large to convert to VLV" << std::endl;
        aValue = 0x0FFFffff;
    }

    bytes[0] = (uint8_t) (((uint32_t) aValue >> 21) & 0x7f);// most significant 7 bits
    bytes[1] = (uint8_t) (((uint32_t) aValue >> 14) & 0x7f);
    bytes[2] = (uint8_t) (((uint32_t) aValue >> 7) & 0x7f);
    bytes[3] = (uint8_t) (((uint32_t) aValue) & 0x7f);// least significant 7 bits

    int start = 0;
    while ((start < 4) && (bytes[start] == 0)) start++;

    for (int i = start; i < 3; i++) {
        bytes[i] = bytes[i] | 0x80;
        outdata.push_back(bytes[i]);
    }
    outdata.push_back(bytes[3]);
}

/* MidiFile::clear_no_deallocate -- Similar to clear() but does not
  delete the Events in the lists.  This is primarily used internally
  to the MidiFile class, so don't use unless you really know what you
  are doing (otherwise you will end up with memory leaks or
  segmentation faults). */
void MidiFile::clear_no_deallocate() {
    for (int i = 0; i < getTrackCount(); i++) {
        tracks[i]->detach();
        delete tracks[i];
        tracks[i] = nullptr;
    }
    tracks.resize(1);
    tracks[0]    = new MidiEventList;
    timemapvalid = 0;
    timemap.clear();
    // events.resize(0);   // causes a memory leak [20150205 Jorden Thatcher]
}

/* eventcompare -- Event comparison function for sorting tracks.

Sorting rules:
   (1) sort by (absolute) tick value; otherwise, if tick values are the same:
   (2) end-of-track meta message is always last.
   (3) other meta-messages come before regular MIDI messages.
   (4) note-offs come after all other regular MIDI messages except note-ons.
   (5) note-ons come after all other regular MIDI messages. */
int eventcompare(const void* a, const void* b) {
    MidiEvent& aevent = **((MidiEvent**) a);
    MidiEvent& bevent = **((MidiEvent**) b);

    if (aevent.tick > bevent.tick) {
        // aevent occurs after bevent
        return +1;
    } else if (aevent.tick < bevent.tick) {
        // aevent occurs before bevent
        return -1;
    } else if (aevent.seq > bevent.seq) {
        // aevent sequencing state occurs after bevent
        // see MidiFile::markSequence()
        return +1;
    } else if (aevent.seq < bevent.seq) {
        // aevent sequencing state occurs before bevent
        // see MidiFile::markSequence()
        return -1;
    } else if (aevent[0] == 0xff && aevent[1] == 0x2f) {
        // end-of-track meta-message should always be last (but won't really
        // matter since the writing function ignores all end-of-track messages
        // and writes its own.
        return +1;
    } else if (bevent[0] == 0xff && bevent[1] == 0x2f) {
        // end-of-track meta-message should always be last (but won't really
        // matter since the writing function ignores all end-of-track messages
        // and writes its own.
        return -1;
    } else if (aevent[0] == 0xff && bevent[0] != 0xff) {
        // other meta-messages are placed before real MIDI messages
        return -1;
    } else if (aevent[0] != 0xff && bevent[0] == 0xff) {
        // other meta-messages are placed before real MIDI messages
        return +1;
    } else if (((aevent[0] & 0xf0) == 0x90) && (aevent[2] != 0)) {
        // note-ons come after all other types of MIDI messages
        return +1;
    } else if (((bevent[0] & 0xf0) == 0x90) && (bevent[2] != 0)) {
        // note-ons come after all other types of MIDI messages
        return -1;
    } else if (((aevent[0] & 0xf0) == 0x90) || ((aevent[0] & 0xf0) == 0x80)) {
        // note-offs come after all other MIDI messages (except note-ons)
        return +1;
    } else if (((bevent[0] & 0xf0) == 0x90) || ((bevent[0] & 0xf0) == 0x80)) {
        // note-offs come after all other MIDI messages (except note-ons)
        return -1;
    } else {
        return 0;
    }
}

/* for finding a tick entry in the time map. */
int MidiFile::ticksearch(const void* A, const void* B) {
    _TickTime& a = *((_TickTime*) A);
    _TickTime& b = *((_TickTime*) B);

    if (a.tick < b.tick) {
        return -1;
    } else if (a.tick > b.tick) {
        return 1;
    }
    return 0;
}

/* for finding a second entry in the time map. */
int MidiFile::secondsearch(const void* A, const void* B) {
    _TickTime& a = *((_TickTime*) A);
    _TickTime& b = *((_TickTime*) B);

    if (a.seconds < b.seconds) {
        return -1;
    } else if (a.seconds > b.seconds) {
        return 1;
    }
    return 0;
}

/* static read functions */
uint32_t MidiFile::readLittleEndian4Bytes(std::istream& input) {
    uint8_t buffer[4] = { 0 };
    input.read((char*) buffer, 4);
    if (input.eof()) {
        std::cerr << "Error: unexpected end of file." << std::endl;
        return 0;
    }
    return buffer[3] | (buffer[2] << 8) | (buffer[1] << 16) | (buffer[0] << 24);
}

uint16_t MidiFile::readLittleEndian2Bytes(std::istream& input) {
    uint8_t buffer[2] = { 0 };
    input.read((char*) buffer, 2);
    if (input.eof()) {
        std::cerr << "Error: unexpected end of file." << std::endl;
        return 0;
    }
    return buffer[1] | (buffer[0] << 8);
}

uint8_t MidiFile::readByte(std::istream& input) {
    uint8_t buffer[1] = { 0 };
    input.read((char*) buffer, 1);
    if (input.eof()) {
        std::cerr << "Error: unexpected end of file." << std::endl;
        return 0;
    }
    return buffer[0];
}

std::ostream& MidiFile::writeLittleEndianUShort(std::ostream& out, uint16_t value) {
    union {
        char bytes[2];
        uint16_t us;
    } data;
    data.us = value;
    out << data.bytes[0];
    out << data.bytes[1];
    return out;
}

std::ostream& MidiFile::writeBigEndianUShort(std::ostream& out, uint16_t value) {
    union {
        char bytes[2];
        uint16_t us;
    } data;
    data.us = value;
    out << data.bytes[1];
    out << data.bytes[0];
    return out;
}

std::ostream& MidiFile::writeLittleEndianShort(std::ostream& out, short value) {
    union {
        char bytes[2];
        short s;
    } data;
    data.s = value;
    out << data.bytes[0];
    out << data.bytes[1];
    return out;
}

std::ostream& MidiFile::writeBigEndianShort(std::ostream& out, short value) {
    union {
        char bytes[2];
        short s;
    } data;
    data.s = value;
    out << data.bytes[1];
    out << data.bytes[0];
    return out;
}

std::ostream& MidiFile::writeLittleEndianULong(std::ostream& out, uint32_t value) {
    union {
        char bytes[4];
        uint32_t ul;
    } data;
    data.ul = value;
    out << data.bytes[0];
    out << data.bytes[1];
    out << data.bytes[2];
    out << data.bytes[3];
    return out;
}

std::ostream& MidiFile::writeBigEndianULong(std::ostream& out, uint32_t value) {
    union {
        char bytes[4];
        long ul;
    } data;
    data.ul = value;
    out << data.bytes[3];
    out << data.bytes[2];
    out << data.bytes[1];
    out << data.bytes[0];
    return out;
}

std::ostream& MidiFile::writeLittleEndianLong(std::ostream& out, long value) {
    union {
        char bytes[4];
        long l;
    } data;
    data.l = value;
    out << data.bytes[0];
    out << data.bytes[1];
    out << data.bytes[2];
    out << data.bytes[3];
    return out;
}

std::ostream& MidiFile::writeBigEndianLong(std::ostream& out, long value) {
    union {
        char bytes[4];
        long l;
    } data;
    data.l = value;
    out << data.bytes[3];
    out << data.bytes[2];
    out << data.bytes[1];
    out << data.bytes[0];
    return out;
}

std::ostream& MidiFile::writeBigEndianFloat(std::ostream& out, float value) {
    union {
        char bytes[4];
        float f;
    } data;
    data.f = value;
    out << data.bytes[3];
    out << data.bytes[2];
    out << data.bytes[1];
    out << data.bytes[0];
    return out;
}

std::ostream& MidiFile::writeLittleEndianFloat(std::ostream& out, float value) {
    union {
        char bytes[4];
        float f;
    } data;
    data.f = value;
    out << data.bytes[0];
    out << data.bytes[1];
    out << data.bytes[2];
    out << data.bytes[3];
    return out;
}

std::ostream& MidiFile::writeBigEndianDouble(std::ostream& out, double value) {
    union {
        char bytes[8];
        double d;
    } data;
    data.d = value;
    out << data.bytes[7];
    out << data.bytes[6];
    out << data.bytes[5];
    out << data.bytes[4];
    out << data.bytes[3];
    out << data.bytes[2];
    out << data.bytes[1];
    out << data.bytes[0];
    return out;
}

std::ostream& MidiFile::writeLittleEndianDouble(std::ostream& out, double value) {
    union {
        char bytes[8];
        double d;
    } data;
    data.d = value;
    out << data.bytes[0];
    out << data.bytes[1];
    out << data.bytes[2];
    out << data.bytes[3];
    out << data.bytes[4];
    out << data.bytes[5];
    out << data.bytes[6];
    out << data.bytes[7];
    return out;
}

MidiFile& MidiFile::operator=(MidiFile other) {
    tracks.swap(other.tracks);
    return *this;
}
