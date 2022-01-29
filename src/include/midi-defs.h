#pragma once

#define MIDI_CODE_MASK      0xf0
#define MIDI_CHN_MASK       0x0f
/*
#define MIDI_REALTIME       0xf8
#define MIDI_CHAN_MODE      0xfa
*/
#define MIDI_OFF_NOTE       0x80
#define MIDI_ON_NOTE        0x90
#define MIDI_POLY_TOUCH     0xa0
#define MIDI_CTRL           0xb0
#define MIDI_CH_PROGRAM     0xc0
#define MIDI_TOUCH          0xd0
#define MIDI_BEND           0xe0

#define MIDI_SYSEX          0xf0
#define MIDI_Q_FRAME        0xf1
#define MIDI_SONG_POINTER   0xf2
#define MIDI_SONG_SELECT    0xf3
#define MIDI_TUNE_REQ       0xf6
#define MIDI_EOX            0xf7
#define MIDI_TIME_CLOCK     0xf8
#define MIDI_START          0xfa
#define MIDI_CONTINUE       0xfb
#define MIDI_STOP           0xfc
#define MIDI_ACTIVE_SENSING 0xfe
#define MIDI_SYS_RESET      0xff

#define MIDI_ALL_SOUND_OFF     0x78
#define MIDI_RESET_CONTROLLERS 0x79
#define MIDI_LOCAL          0x7a
#define MIDI_ALL_OFF        0x7b
#define MIDI_OMNI_OFF       0x7c
#define MIDI_OMNI_ON        0x7d
#define MIDI_MONO_ON        0x7e
#define MIDI_POLY_ON        0x7f

#define MidiMsgInt32(status, data1, data2) \
         ((((data2) << 16) & 0xFF0000) | \
          (((data1) << 8) & 0xFF00) | \
          ((status) & 0xFF))
#define MidiMsgStatus(msg) ((msg) & 0xFF)
#define MidiMsgData1(msg) (((msg) >> 8) & 0xFF)
#define MidiMsgData2(msg) (((msg) >> 16) & 0xFF)
