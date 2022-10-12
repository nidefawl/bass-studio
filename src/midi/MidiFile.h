/*
 * Modifications (c) Michael Hept 2017-2022
 * Removed BinAsc support
 * Use std::string instead of const char*
 * Remove rwstatus field from MidiFile
 */

//
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Fri Nov 26 14:12:01 PST 1999
// Last Modified: Fri Dec  2 13:26:44 PST 1999
// Last Modified: Fri Nov 10 12:13:15 PST 2000 Added some more editing cap.
// Last Modified: Thu Jan 10 10:03:39 PST 2002 Added allocateEvents()
// Last Modified: Mon Jun 10 22:43:10 PDT 2002 Added clear()
// Last Modified: Sat Dec 17 23:11:57 PST 2005 Added millisecond ticks
// Last Modified: Tue Feb  5 11:51:43 PST 2008 Read() set to const char*
// Last Modified: Tue Apr  7 09:23:48 PDT 2009 Added addMetaEvent
// Last Modified: Fri Jun 12 22:58:34 PDT 2009 Renamed SigCollection class
// Last Modified: Thu Jul 22 23:28:54 PDT 2010 Added tick to time mapping
// Last Modified: Thu Jul 22 23:28:54 PDT 2010 Changed _MidiEvent to MidiEvent
// Last Modified: Tue Feb 22 13:26:40 PST 2011 Added write(std::ostream)
// Last Modified: Mon Nov 18 13:10:37 PST 2013 Added .printHex function.
// Last Modified: Mon Feb  9 14:01:31 PST 2015 Removed FileIO dependency.
// Last Modified: Sat Feb 14 22:35:25 PST 2015 Split out subclasses.
// Filename:      midifile/include/MidiFile.h
// Website:       http://midifile.sapp.org
// Syntax:        C++11
// vim:           ts=3 expandtab
//
// Description:   A class which can read/write Standard MIDI files.
//                MIDI data is stored by track in an array.
//

#ifndef _MIDIFILE_H_INCLUDED
#define _MIDIFILE_H_INCLUDED

#include "MidiEventList.h"

#include <vector>
#include <istream>
#include <fstream>

#define TIME_STATE_DELTA       0
#define TIME_STATE_ABSOLUTE    1

#define TRACK_STATE_SPLIT      0
#define TRACK_STATE_JOINED     1

class _TickTime {
   public:
      int    tick;
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
      bool      read                      (const std::string& aFile);
      bool      readFromInputStream       (std::istream& istream);
      bool      write                     (const std::string& aFile);
      bool      writeToOutputStream       (std::ostream& out);

      // track-related functions:
      MidiEventList& operator[]           (int aTrack);
      const MidiEventList& operator[]     (int aTrack) const;
      int       getTrackCount             (void) const;
      int       getNumTracks              (void) const;
      int       size                      (void) const;

      // join/split track functionality:
      void      markSequence              (void);
      void      clearSequence             (void);
      void      joinTracks                (void);
      void      splitTracks               (void);
      void      splitTracksByChannel      (void);
      int       getTrackState             (void);
      int       hasJoinedTracks           (void);
      int       hasSplitTracks            (void);
      int       getSplitTrack             (int track, int index);
      int       getSplitTrack             (int index);

      void      sortTrack                 (MidiEventList& trackData);
      void      sortTracks                (void);

      int       addTrack                  (void);
      int       addTrack                  (int count);
      int       addTracks(int count) { return addTrack(count); }
      void      deleteTrack               (int aTrack);
      void      mergeTracks               (int aTrack1, int aTrack2);
      int       getTrackCountAsType1      (void);

      int       getEventCount             (int aTrack);
      void      allocateEvents            (int track, int aSize);
      int       getNumEvents              (int aTrack);

      // tick-related functions:
      void      makeDeltaTicks                (void);
      void      makeAbsoluteTicks         (void);
      int       getTickState              (void);
      int       isDeltaTicks              (void);
      int       isAbsoluteTicks           (void);

      // ticks-per-quarter related functions:
      void      setMillisecondTicks       (void);
      int       getTicksPerQuarterNote    (void);
      int       getTPQ                    (void);
      void      setTicksPerQuarterNote    (int ticks);
      void      setTPQ                    (int ticks);

      // physical-time analysis functions:
      void      doTimeAnalysis            (void);
      double    getTimeInSeconds          (int aTrack, int anIndex);
      double    getTimeInSeconds          (int tickvalue);
      int       getAbsoluteTickTime       (double starttime);

      double    getTotalTimeInSeconds     (void);
      int       getTotalTimeInTicks       (void);
      double    getTotalTimeInQuarters    (void);

      // note-analysis functions:
      int       linkNotePairs             (void);
      int       linkEventPairs            (void);
      void      clearLinks                (void);

      // filename functions:
      void      setFilename               (const std::string& aname);
      const std::string& getFilename      (void);

      int       addEvent                  (int aTrack, int aTick,
                                           std::vector<unsigned char>& midiData);
      int       addEvent                  (MidiEvent& mfevent);

      // MIDI message adding convenience functions:
      int       addNoteOn                 (int aTrack, int aTick,
                                           int aChannel, int key, int vel);
      int       addNoteOff                (int aTrack, int aTick,
                                           int aChannel, int key, int vel);
      int       addNoteOff                (int aTrack, int aTick,
                                           int aChannel, int key);
      int       addController             (int aTrack, int aTick,
                                           int aChannel, int num, int value);
      int       addPatchChange            (int aTrack, int aTick,
                                           int aChannel, int patchnum);
      int       addTimbre                 (int aTrack, int aTick,
                                           int aChannel, int patchnum);
      int       addPitchBend              (int aTrack, int aTick,
                                           int aChannel, double amount);

      // Meta-event adding convenience functions:
      int       addMetaEvent              (int aTrack, int aTick, int aType,
                                             std::vector<unsigned char>& metaData);
      int       addMetaEvent              (int aTrack, int aTick, int aType,
                                           const std::string& metaData);
      int       addCopyright              (int aTrack, int aTick,
                                           const std::string& text);
      int       addTrackName              (int aTrack, int aTick,
                                           const std::string& name);
      int       addInstrumentName         (int aTrack, int aTick,
                                           const std::string& name);
      int       addLyric                  (int aTrack, int aTick,
                                           const std::string& text);
      int       addMarker                 (int aTrack, int aTick,
                                           const std::string& text);
      int       addCue                    (int aTrack, int aTick,
                                           const std::string& text);
      int       addTempo                  (int aTrack, int aTick,
                                           double aTempo);
      int       addTimeSignature          (int aTrack, int aTick,
                                           int top, int bottom,
                                           int clocksPerClick = 24,
                                           int num32dsPerQuarter = 8);
      int       addCompoundTimeSignature  (int aTrack, int aTick,
                                           int top, int bottom,
                                           int clocksPerClick = 36,
                                           int num32dsPerQuarter = 8);

      void      erase                     (void);
      void      clear                     (void);
      void      clear_no_deallocate       (void);
      MidiEvent&  getEvent                (int aTrack, int anIndex);

      MidiFile& operator=(MidiFile other);

      // static functions:
      static unsigned char readByte                (std::istream& input);
      static uint16_t      readLittleEndian2Bytes  (std::istream& input);
      static uint32_t      readLittleEndian4Bytes  (std::istream& input);
      static std::ostream& writeLittleEndianUShort (std::ostream& out, uint16_t value);
      static std::ostream& writeBigEndianUShort    (std::ostream& out, uint16_t value);
      static std::ostream& writeLittleEndianShort  (std::ostream& out, short  value);
      static std::ostream& writeBigEndianShort     (std::ostream& out, short  value);
      static std::ostream& writeLittleEndianULong  (std::ostream& out, uint32_t  value);
      static std::ostream& writeBigEndianULong     (std::ostream& out, uint32_t  value);
      static std::ostream& writeLittleEndianLong   (std::ostream& out, long   value);
      static std::ostream& writeBigEndianLong      (std::ostream& out, long   value);
      static std::ostream& writeLittleEndianFloat  (std::ostream& out, float  value);
      static std::ostream& writeBigEndianFloat     (std::ostream& out, float  value);
      static std::ostream& writeLittleEndianDouble (std::ostream& out, double value);
      static std::ostream& writeBigEndianDouble    (std::ostream& out, double value);

   protected:
       std::vector<MidiEventList*> tracks;           // MIDI file events
       int ticksPerQuarterNote = 0;                  // time base of file
       int midiTrackState      = TRACK_STATE_SPLIT;  // joined or split
       int midiTimingType      = TIME_STATE_ABSOLUTE;// absolute or delta
       std::string readFileName;                     // read file name
       int timemapvalid = 0;
       std::vector<_TickTime> timemap;

   private:
      int        extractMidiData  (std::istream& inputfile, std::vector<unsigned char>& array,
                                       unsigned char& runningCommand);
      uint32_t   readVLValue      (std::istream& input);
      uint32_t   unpackVLV        (unsigned char a = 0, unsigned char b = 0, unsigned char c = 0,
                                   unsigned char d = 0);
      void       writeVLValue     (long aValue, std::vector<unsigned char>& data);
      int        makeVLV          (unsigned char *buffer, int number);
      static int ticksearch       (const void* A, const void* B);
      static int secondsearch     (const void* A, const void* B);
      void       buildTimeMap     (void);
      int        linearTickInterpolationAtSecond  (double seconds);
      double     linearSecondInterpolationAtTick  (int ticktime);
};


int eventcompare(const void* a, const void* b);
std::ostream& operator<<(std::ostream& out, MidiFile& aMidiFile);

#endif /* _MIDIFILE_H_INCLUDED */



