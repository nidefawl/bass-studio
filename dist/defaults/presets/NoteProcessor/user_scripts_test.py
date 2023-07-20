import copy
import random
from note_bindings import *

PARAM_TYPE_INT = 0
PARAM_TYPE_FLOAT = 1
TICK_BITS = 12
TICK_BITS_BAR = (TICK_BITS + 2)
TICKS_QUARTER = (1 << TICK_BITS)
TICKS_16TH = (TICKS_QUARTER >> 2)
TICKS_BAR = (TICKS_QUARTER << 2)

class note_processor:
    '''base class for note processors'''
    def getName(self):
        return 'note_processor'
    def getParameters(self):
        # return tuples of (name, type, min, max, default)
        return []
    def process(self, ctxt):
        pass

def copyNote(other):
    note = note_t()
    note.time = other.time
    note.pitch = other.pitch
    note.velocity = other.velocity
    note.len = other.len
    return note

class generateRandomCoordProgression(note_processor):
    '''generate random coordinate progression'''
    def getName(self):
        return 'Generate Random Coord Progression'
    def getParameters(self):
        return [
            ('bars', PARAM_TYPE_INT, 4, 16, 4),
        ]
    def process(self, ctxt):
        notes = []
        nBars = int(ctxt.params[0])
        rand = random.Random(ctxt.seed)
        progression = [0]
        # generate random starting note
        # 0 = C, 1 = C#, 2 = D, 3 = D#, 4 = E, 5 = F, 6 = F#, 7 = G, 8 = G#, 9 = A, 10 = A#, 11 = B
        startNote = (12*3)+rand.randint(0, 11)
        # generate major or minor coord progression
        # 0 = major, 1 = minor
        majorMinor = rand.randint(0, 1)
        for bar in range(nBars - 1):
            # generate random progression
            progressionDirection = rand.randint(0, 1)
            if progressionDirection == 0:
              # up
              progression.append(rand.randint(1, 2))
            else:
              # down
              progression.append(rand.randint(-2, -1))
        notes = []
        currentNote = startNote
        for i in range(len(progression)):
            currentNote = currentNote + progression[i]
            note = note_t()
            note.time = int(i * TICKS_BAR)
            note.pitch = currentNote
            note.velocity = 100
            note.len = int(TICKS_BAR)
            notes.append(note)
            if majorMinor == 0:
                # minor
                note = copyNote(note)
                note.pitch = currentNote + 4
                notes.append(note)
                note = copyNote(note)
                note.pitch = currentNote + 7
                notes.append(note)
            else:
                # major
                note = copyNote(note)
                note.pitch = currentNote + 3
                notes.append(note)
                note = copyNote(note)
                note.pitch = currentNote + 7
                notes.append(note)
        return notes

export_processors = [
    generateRandomCoordProgression(),
]