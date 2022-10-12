#pragma once
/*
 * Based on https://github.com/craigsapp/midifile
 * Modifications (c) Michael Hept
 */
#include "MidiMessage.h"
#include <vector>


class MidiEvent : public MidiMessage {
   public:
                 MidiEvent     (void);
                 MidiEvent     (int command);
                 MidiEvent     (int command, int param1);
                 MidiEvent     (int command, int param1, int param2);
                 MidiEvent     (int aTime, int aTrack, std::vector<unsigned char>& message);
                 MidiEvent     (const MidiMessage& message);
                 MidiEvent     (const MidiEvent& mfevent);

                ~MidiEvent     ();

      MidiEvent& operator=     (const MidiEvent& mfevent);
      MidiEvent& operator=     (const MidiMessage& message);
      MidiEvent& operator=     (const std::vector<unsigned char>& bytes);
      MidiEvent& operator=     (const std::vector<char>& bytes);
      MidiEvent& operator=     (const std::vector<int>& bytes);
      void       clearVariables(void);

      // functions related to event linking (note-ons to note-offs).
      void       unlinkEvent   (void);
      void       unlinkEvents  (void);
      void       linkEvent     (MidiEvent* mev);
      void       linkEvents    (MidiEvent* mev);
      void       linkEvent     (MidiEvent& mev);
      void       linkEvents    (MidiEvent& mev);
      int        isLinked      (void);
      MidiEvent* getLinkedEvent(void);
      int        getTickDuration(void);
      double     getDurationInSeconds(void);

      int       tick;
      int       track;
      double    seconds;
      int       seq;

   private:
      MidiEvent* eventlink;      // used to match note-ons and note-offs

};
