#pragma once
/*
 * Based on https://github.com/craigsapp/midifile
 * Modifications (c) Michael Hept
 */
#include "MidiEvent.h"
#include <vector>


class MidiEventList {
   public:
                  MidiEventList    (void);

                 ~MidiEventList    ();

                 MidiEventList     (const MidiEventList& other);
                 MidiEventList     (MidiEventList&& other);

      MidiEvent&  operator[]       (int index);
      const MidiEvent&  operator[] (int index) const;
      MidiEvent&  back             (void);
      MidiEvent&  last             (void);
      MidiEvent&  getEvent         (int index);
      void        clear            (void);
      void        reserve          (int rsize);
      int         getSize          (void) const;
      int         size             (void) const;
      int         linkNotePairs    (void);
      int         linkEventPairs   (void);
      void        clearLinks       (void);
      MidiEvent** data             (void);

      int         push             (MidiEvent& event);
      int         push_back        (MidiEvent& event);
      int         append           (MidiEvent& event);

      // careful when using these, intended for internal use in MidiFile class:
      void        detach              (void);
      int         push_back_no_copy   (MidiEvent* event);

      MidiEventList& operator=(MidiEventList other);

   private:
      std::vector<MidiEvent*>     list;

};
