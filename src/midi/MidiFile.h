#pragma once
/*
 * Based on https://github.com/craigsapp/midifile
 * Modifications (c) Michael Hept
 */

#include "MidiEventList.h"
#include <vector>
#include <istream>
#include <fstream>

#define TIME_STATE_DELTA 0
#define TIME_STATE_ABSOLUTE 1

#define TRACK_STATE_SPLIT 0
#define TRACK_STATE_JOINED 1

class MidiFileTickTime {
public:
    int tick;
    double seconds;
};


class MidiFile {
public:
    MidiFile() = default;
    explicit MidiFile(const std::string& aFile);
    MidiFile(const MidiFile& other) = delete;
    MidiFile(MidiFile&& other)      = delete;
    ~MidiFile();

    // reading/writing functions:
    bool read(const std::string& aFile);
    bool readFromInputStream(std::istream& istream);
    bool write(const std::string& aFile);
    bool writeToOutputStream(std::ostream& out);

    // track-related functions:
    MidiEventList& operator[](int aTrack);
    const MidiEventList& operator[](int aTrack) const;
    int getTrackCount() const;

    // join/split track functionality:
    void markSequence();
    void clearSequence();
    void joinTracks();
    void splitTracks();
    void splitTracksByChannel();
    int getTrackState();
    int hasJoinedTracks();
    int hasSplitTracks();
    int getSplitTrack(int track, int index);
    int getSplitTrack(int index);

    void sortTrack(MidiEventList& trackData);
    void sortTracks();

    int addTrack();
    int addTrack(int count);
    int addTracks(int count) { return addTrack(count); }
    void deleteTrack(int aTrack);
    void mergeTracks(int aTrack1, int aTrack2);
    int getTrackCountAsType1();

    void allocateEvents(int track, int aSize);
    int getNumEvents(int aTrack);

    // tick-related functions:
    void makeDeltaTicks();
    void makeAbsoluteTicks();
    int getTickState();
    int isDeltaTicks();
    int isAbsoluteTicks();

    // ticks-per-quarter related functions:
    void setMillisecondTicks();
    int getTicksPerQuarterNote();
    int getTPQ();
    void setTicksPerQuarterNote(int ticks);

    // physical-time analysis functions:
    void doTimeAnalysis();
    double getTimeInSeconds(int aTrack, int anIndex);
    double getTimeInSeconds(int tickvalue);
    int getAbsoluteTickTime(double starttime);

    double getTotalTimeInSeconds();
    int getTotalTimeInTicks();
    double getTotalTimeInQuarters();

    // note-analysis functions:
    int linkNotePairs();
    int linkEventPairs();
    void clearLinks();

    // filename functions:
    void setFilename(const std::string& aname);
    const std::string& getFilename();

    int addEvent(int aTrack, int aTick,
                 std::vector<unsigned char>& midiData);
    int addEvent(MidiEvent& mfevent);

    // MIDI message adding convenience functions:
    int addNoteOn(int aTrack, int aTick,
                  int aChannel, int key, int vel);
    int addNoteOff(int aTrack, int aTick,
                   int aChannel, int key, int vel);
    int addNoteOff(int aTrack, int aTick,
                   int aChannel, int key);
    int addController(int aTrack, int aTick,
                      int aChannel, int num, int value);
    int addPatchChange(int aTrack, int aTick,
                       int aChannel, int patchnum);
    int addTimbre(int aTrack, int aTick,
                  int aChannel, int patchnum);
    int addPitchBend(int aTrack, int aTick,
                     int aChannel, double amount);

    // Meta-event adding convenience functions:
    int addMetaEvent(int aTrack, int aTick, int aType,
                     std::vector<unsigned char>& metaData);
    int addMetaEvent(int aTrack, int aTick, int aType,
                     const std::string& metaData);
    int addCopyright(int aTrack, int aTick,
                     const std::string& text);
    int addTrackName(int aTrack, int aTick,
                     const std::string& name);
    int addInstrumentName(int aTrack, int aTick,
                          const std::string& name);
    int addLyric(int aTrack, int aTick,
                 const std::string& text);
    int addMarker(int aTrack, int aTick,
                  const std::string& text);
    int addCue(int aTrack, int aTick,
               const std::string& text);
    int addTempo(int aTrack, int aTick,
                 double aTempo);
    int addTimeSignature(int aTrack, int aTick,
                         int top, int bottom,
                         int clocksPerClick    = 24,
                         int num32dsPerQuarter = 8);
    int addCompoundTimeSignature(int aTrack, int aTick,
                                 int top, int bottom,
                                 int clocksPerClick    = 36,
                                 int num32dsPerQuarter = 8);
    int addEndOfTrack(int aTrack, int aTick);

    void erase();
    void clear();
    void clear_no_deallocate();
    MidiEvent& getEvent(int aTrack, int anIndex);

    MidiFile& operator=(MidiFile other);

    // static functions:
    static unsigned char readByte(std::istream& input);
    static uint16_t readLittleEndian2Bytes(std::istream& input);
    static uint32_t readLittleEndian4Bytes(std::istream& input);
    static std::ostream& writeLittleEndianUShort(std::ostream& out, uint16_t value);
    static std::ostream& writeBigEndianUShort(std::ostream& out, uint16_t value);
    static std::ostream& writeLittleEndianShort(std::ostream& out, short value);
    static std::ostream& writeBigEndianShort(std::ostream& out, short value);
    static std::ostream& writeLittleEndianULong(std::ostream& out, uint32_t value);
    static std::ostream& writeBigEndianULong(std::ostream& out, uint32_t value);
    static std::ostream& writeLittleEndianLong(std::ostream& out, long value);
    static std::ostream& writeBigEndianLong(std::ostream& out, long value);
    static std::ostream& writeLittleEndianFloat(std::ostream& out, float value);
    static std::ostream& writeBigEndianFloat(std::ostream& out, float value);
    static std::ostream& writeLittleEndianDouble(std::ostream& out, double value);
    static std::ostream& writeBigEndianDouble(std::ostream& out, double value);

protected:
    std::vector<MidiEventList*> tracks;           // MIDI file events
    int ticksPerQuarterNote = 0;                  // time base of file
    int midiTrackState      = TRACK_STATE_SPLIT;  // joined or split
    int midiTimingType      = TIME_STATE_ABSOLUTE;// absolute or delta
    std::string readFileName;                     // read file name
    int timemapvalid = 0;
    std::vector<MidiFileTickTime> timemap;

private:
    int extractMidiData(std::istream& inputfile, std::vector<unsigned char>& array,
                        unsigned char& runningCommand);
    uint32_t readVLValue(std::istream& input);
    uint32_t unpackVLV(unsigned char a = 0, unsigned char b = 0, unsigned char c = 0,
                       unsigned char d = 0);
    void writeVLValue(long aValue, std::vector<unsigned char>& data);
    int makeVLV(unsigned char* buffer, int number);
    void buildTimeMap();
    int linearTickInterpolationAtSecond(double seconds);
    double linearSecondInterpolationAtTick(int ticktime);
};


int eventcompare(const void* a, const void* b);
std::ostream& operator<<(std::ostream& out, MidiFile& aMidiFile);
