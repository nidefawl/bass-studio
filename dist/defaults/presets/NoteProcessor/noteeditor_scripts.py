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

class generate4x4Pattern(note_processor):
    '''generate 4x4 pattern'''
    def getName(self):
        return 'Generate 4x4 Pattern'
    def getParameters(self):
        return [
            ('Note Length', PARAM_TYPE_INT, TICKS_16TH/4, TICKS_BAR*4, TICKS_BAR),
            ('Gate %', PARAM_TYPE_FLOAT, 0.0, 1.0, 0.8),
            ('Bars', PARAM_TYPE_INT, 1, 16, 4),
        ]
    def process(self, ctxt):
        notes = []
        noteLength, gate, bars = ctxt.params[:3]
        for bar in range(int(bars)):
            tickPos = 0
            while tickPos < TICKS_BAR:
                note = note_t()
                note.time = int(bar * TICKS_BAR + tickPos)
                note.pitch = 60
                note.velocity = 100
                note.len = int(noteLength * gate)
                notes.append(note)
                tickPos += int(noteLength)
        return notes

class randomizeNoteStartTime(note_processor):
    '''randomize note start time'''
    def getName(self):
        return 'Randomize Note Start Time'
    def getParameters(self):
        return [
            ('Tick Range', PARAM_TYPE_INT, 0.0, 4096<<4, 42.0),
        ]
    def process(self, ctxt):
        tickDuration = ctxt.params[0]
        rand = random.Random(ctxt.seed)
        for note in ctxt.notes:
            randFloat = rand.random()
            note.time += int((-1.0 + 2.0 * randFloat) * tickDuration)
            if note.time < 0:
                note.time = -note.time
            if note.time < 0:
                note.time = 0
        return ctxt.notes

class randomizeNoteVelocity(note_processor):
    '''randomize note velocity'''
    def getName(self):
        return 'Randomize Note Velocity'
    def getParameters(self):
        return [
            ('Velocity Range', PARAM_TYPE_INT, 0.0, 127.0, 5.0),
        ]
    def process(self, ctxt):
        velocityRange = ctxt.params[0]
        rand = random.Random(ctxt.seed)
        for note in ctxt.notes:
            randFloat = rand.random()
            note.velocity += int((-1.0 + 2.0 * randFloat) * velocityRange)
            if note.velocity < 0:
                note.velocity = -note.velocity
            elif note.velocity > 127:
                note.velocity = note.velocity - (note.velocity - 127)
            if note.velocity < 0:
                note.velocity = 0
            elif note.velocity > 127:
                note.velocity = 127
        return ctxt.notes

class makeVelocityRamp(note_processor):
    '''Create a velocity ramp'''
    def getName(self):
        return 'Create a velocity ramp'
    def getParameters(self):
        return [
            ('Velocity Start', PARAM_TYPE_INT, 0.0, 127.0, 5.0),
            ('Velocity End', PARAM_TYPE_INT, 0.0, 127.0, 127.0),
        ]
    def process(self, ctxt):
        start, end = ctxt.params[:2]
        for i, note in enumerate(ctxt.notes):
            note.velocity = int(start + (end - start) * i / len(ctxt.notes))
        return ctxt.notes

export_processors = [
    randomizeNoteVelocity(),
    randomizeNoteStartTime(),
    generate4x4Pattern(),
    makeVelocityRamp(),
]