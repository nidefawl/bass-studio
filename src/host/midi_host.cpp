#include "midi_host.h"
#include "config.h"
#include "samplerate.h"
#include "str_util.h"
#include "platform.h"
#include "logging.h"
#include <portmidi.h>
#include <pmutil.h>
#include <stdint.h>
#include "midi-defs.h"

#define IN_QUEUE_SIZE 1024
#define OUT_QUEUE_SIZE 1024


bool error(const char* msg, PmError err) {
	log_printf("Error in %s\n", msg);
	log_printf("Error number: %d\n", err);
	log_printf("Error message: %s\n", Pm_GetErrorText(err));
	return false;
}



/****************************************************************************
*               showbytes
* Effect: print hex data, precede with newline if asked
****************************************************************************/

char nib_to_hex[] = "0123456789ABCDEF";

void showbytes(PmMessage data, int len, bool newline)
{
    int count = 0;
    int i;

/*    if (newline) {
        putchar('\n');
        count++;
    } */
    String s = "";
    for (i = 0; i < len; i++) {
        s+=(nib_to_hex[(data >> 4) & 0xF]);
        s+=(nib_to_hex[data & 0xF]);
        count += 2;
        if (count > 72) {
        	s+=('.');
        	s+=('.');
            s+=('.');
            break;
        }
        data >>= 8;
    }
    s+=' ';
    log_out("%s\n", StringAsCStr(s));
}

/****************************************************************************
*               put_pitch
* Inputs:
*    int p: pitch number
* Effect: write out the pitch name for a given number
****************************************************************************/

char* getNoteName(int note)
{
	static const char* const noteNames[12] {
		"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
	};
	static const size_t buf_size = 32;
	static char* const buf = (char*) malloc(buf_size);
	_snprintf_s(buf, buf_size, _TRUNCATE, "%s%d", noteNames[note%12], (note/12)-2);
    return buf;
}
/****************************************************************************
*               output
* Inputs:
*    data: midi message buffer holding one command or 4 bytes of sysex msg
* Effect: format and print  midi data
****************************************************************************/

char val_format[] = "    Val %d\n";
char vel_format[] = "    Vel %d\n";

void midihost::handleMessage(PmMessage data, std::vector<MidiIOEvent>& messages) {
	bool verbose = true;
    int command;    /* the current command */
    int chan;   /* the midi channel of the current event */
    int len;    /* used to get constant field width */

    /* log_printf("output data %8x; ", data); */

    command = Pm_MessageStatus(data) & MIDI_CODE_MASK;
    chan = Pm_MessageStatus(data) & MIDI_CHN_MASK;

    if (inputInSysex || Pm_MessageStatus(data) == MIDI_SYSEX) {
#define sysex_max 16
        int i;
        PmMessage data_copy = data;
        inputInSysex = true;
        /* look for MIDI_EOX in first 3 bytes
         * if realtime messages are embedded in sysex message, they will
         * be printed as if they are part of the sysex message
         */
        for (i = 0; (i < 4) && ((data_copy & 0xFF) != MIDI_EOX); i++)
            data_copy >>= 8;
        if (i < 4) {
            inputInSysex = false;
            i++; /* include the EOX byte in output */
        }
        showbytes(data, i, verbose);
        if (verbose) log_printf("System Exclusive\n", 0);
    } else if (command == MIDI_ON_NOTE && Pm_MessageData2(data) != 0) {
        showbytes(data, 3, verbose);
        if (verbose) {
        	String s = StringFormat("NoteOn Chan %2d Key %3d %s %s", chan, Pm_MessageData1(data), getNoteName(Pm_MessageData1(data)), StringAsCStr(StringFormat(vel_format, Pm_MessageData2(data))));
        	log_out("%s\n", StringAsCStr(s));
        }
        messages.push_back({data, 0});
    } else if ((command == MIDI_ON_NOTE /* && Pm_MessageData2(data) == 0 */ ||
               command == MIDI_OFF_NOTE)) {
        showbytes(data, 3, verbose);
        if (verbose) {
        	String s = StringFormat("NoteOff Chan %2d Key %3d %s %s", chan, Pm_MessageData1(data), getNoteName(Pm_MessageData1(data)), StringAsCStr(StringFormat(vel_format, Pm_MessageData2(data))));
        	log_out("%s\n", StringAsCStr(s));
        }
        messages.push_back({data, 0});
    } else if (command == MIDI_CH_PROGRAM) {
        showbytes(data, 2, verbose);
        if (verbose) {
            log_printf("  ProgChg Chan %2d Prog %2d\n", chan, Pm_MessageData1(data) + 1);
        }
    } else if (command == MIDI_CTRL) {
               /* controls 121 (MIDI_RESET_CONTROLLER) to 127 are channel
                * mode messages. */
        if (Pm_MessageData1(data) < MIDI_ALL_SOUND_OFF) {
            showbytes(data, 3, verbose);
            if (verbose) {
                log_printf("CtrlChg Chan %2d Ctrl %2d Val %2d\n",
                       chan, Pm_MessageData1(data), Pm_MessageData2(data));
            }
        } else /* channel mode */ /*if (chmode)*/ {
            showbytes(data, 3, verbose);
            if (verbose) {
                switch (Pm_MessageData1(data)) {
                  case MIDI_ALL_SOUND_OFF:
                      log_printf("All Sound Off, Chan %2d\n", chan);
                    break;
                  case MIDI_RESET_CONTROLLERS:
                    log_printf("Reset All Controllers, Chan %2d\n", chan);
                    break;
                  case MIDI_LOCAL:
                    log_printf("LocCtrl Chan %2d %s\n",
                            chan, Pm_MessageData2(data) ? "On" : "Off");
                    break;
                  case MIDI_ALL_OFF:
                    log_printf("All Off Chan %2d\n", chan);
                    break;
                  case MIDI_OMNI_OFF:
                    log_printf("OmniOff Chan %2d\n", chan);
                    break;
                  case MIDI_OMNI_ON:
                    log_printf("Omni On Chan %2d\n", chan);
                    break;
                  case MIDI_MONO_ON:
                    log_printf("Mono On Chan %2d\n", chan);
                    if (Pm_MessageData2(data))
                        log_printf(" to %d received channels\n", Pm_MessageData2(data));
                    else
                        log_printf(" to all received channels\n", 0);
                    break;
                  case MIDI_POLY_ON:
                    log_printf("Poly On Chan %2d\n", chan);
                    break;
                }
            }
        }
    } else if (command == MIDI_POLY_TOUCH) {
        showbytes(data, 3, verbose);
        if (verbose) {
        	String s = StringFormat("P.Touch Chan %2d Key %3d %s %s", chan, Pm_MessageData1(data), getNoteName(Pm_MessageData1(data)), StringAsCStr(StringFormat(vel_format, Pm_MessageData2(data))));
        	log_out("%s\n", StringAsCStr(s));
        }
    } else if (command == MIDI_TOUCH) {
        showbytes(data, 2, verbose);
        if (verbose) {
            log_printf("  A.Touch Chan %2d Val %2d\n", chan, Pm_MessageData1(data));
        }
    } else if (command == MIDI_BEND) {
        showbytes(data, 3, verbose);
        if (verbose) {
            log_printf("P.Bend  Chan %2d Val %2d\n", chan,
                    (Pm_MessageData1(data) + (Pm_MessageData2(data)<<7)));
        }
        messages.push_back({data, 0});
    } else if (Pm_MessageStatus(data) == MIDI_SONG_POINTER) {
        showbytes(data, 3, verbose);
        if (verbose) {
            log_printf("    Song Position %d\n",
                    (Pm_MessageData1(data) + (Pm_MessageData2(data)<<7)));
        }
    } else if (Pm_MessageStatus(data) == MIDI_SONG_SELECT) {
        showbytes(data, 2, verbose);
        if (verbose) {
            log_printf("    Song Select %d\n", Pm_MessageData1(data));
        }
    } else if (Pm_MessageStatus(data) == MIDI_TUNE_REQ) {
        showbytes(data, 1, verbose);
        if (verbose) {
            log_printf("    Tune Request\n", 0);
        }
    } else if (Pm_MessageStatus(data) == MIDI_Q_FRAME/* && realdata */) {
        showbytes(data, 2, verbose);
        if (verbose) {
            log_printf("    Time Code Quarter Frame Type %d Values %d\n",
                    (Pm_MessageData1(data) & 0x70) >> 4, Pm_MessageData1(data) & 0xf);
        }
    } else if (Pm_MessageStatus(data) == MIDI_START/* && realdata */) {
        showbytes(data, 1, verbose);
        if (verbose) {
            log_printf("    Start\n", 0);
        }
    } else if (Pm_MessageStatus(data) == MIDI_CONTINUE/* && realdata */) {
        showbytes(data, 1, verbose);
        if (verbose) {
            log_printf("    Continue\n", 0);
        }
    } else if (Pm_MessageStatus(data) == MIDI_STOP/* && realdata */) {
        showbytes(data, 1, verbose);
        if (verbose) {
            log_printf("    Stop\n", 0);
        }
    } else if (Pm_MessageStatus(data) == MIDI_SYS_RESET/* && realdata */) {
        showbytes(data, 1, verbose);
        if (verbose) {
            log_printf("    System Reset\n", 0);
        }
    } else if (Pm_MessageStatus(data) == MIDI_TIME_CLOCK) {
        showbytes(data, 1, verbose);
        if (verbose) {
            log_printf("    Clock\n", 0);
        }
    } else if (Pm_MessageStatus(data) == MIDI_ACTIVE_SENSING) {
        showbytes(data, 1, verbose);
        if (verbose) {
            log_printf("    Active Sensing\n", 0);
        }
    } else showbytes(data, 3, verbose);
//    fflush(stdout);
}

int32_t getMidiTime(void* userData) {
	int32_t time = getTimeMillis()&(0x7fffffff);
	return time;
}

/* timer interrupt for processing midi data.
   Incoming data is delivered to main program via in_queue.
   Outgoing data from main program is delivered via out_queue.
   Incoming data from midi_in is copied with low latency to  midi_out.
   Sysex messages from either source block messages from the other.
 */
int32_t midihost::processMidi(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround)
{
    PmError result;
    PmEvent buffer; /* just one message at a time */
    /* if (current_timestamp % 1000 == 0)
        log_printf("time %d\n", current_timestamp); */

    int32_t current_timestamp = getMidiTime(nullptr);
    /* do nothing until initialization completes */
    if (!enableProcessing || !isStreaming()) {
//        /* this flag signals that no more midi processing will be done */
//        process_midi_exit_flag = TRUE;
        return 0;
    }

//    /* see if there is any midi input to process */
    if (streamIn) {
    	std::vector<MidiIOEvent> messages;
        do {
            result = Pm_Poll(streamIn);
            if (result) {
                int status;
                int rslt = Pm_Read(streamIn, &buffer, 1);
                if (rslt == pmBufferOverflow)
                    continue;
                assert(rslt == 1);

                /* record timestamp of most recent data */
                last_timestamp = current_timestamp;

                /* the data might be the end of a sysex message that
                   has timed out, in which case we must ignore it.
                   It's a continuation of a sysex message if status
                   is actually a data byte (high-order bit is zero). */
                status = Pm_MessageStatus(buffer.message);
                if (((status & 0x80) == 0) && !inputInSysex) {
                    continue; /* ignore this data */
                }

                handleMessage(buffer.message, messages);

                /* send the message to the application */
                /* you might want to filter clock or active sense messages here
                   to avoid sending a bunch of junk to the application even if
                   you want to send it to MIDI THRU
                 */
//                Pm_Enqueue(in_queue, &buffer);

                /* sysex processing */
                if (status == MIDI_SYSEX) {
                	inputInSysex = TRUE;
                } else if ((status & 0xF8) != 0xF8) {
                    /* not MIDI_SYSEX and not real-time, so */
                    inputInSysex = FALSE;
                }
                if (inputInSysex && /* look for EOX */
                    (((buffer.message & 0xFF) == MIDI_EOX) ||
                     (((buffer.message >> 8) & 0xFF) == MIDI_EOX) ||
                     (((buffer.message >> 16) & 0xFF) == MIDI_EOX) ||
                     (((buffer.message >> 24) & 0xFF) == MIDI_EOX))) {
                    inputInSysex = FALSE;
                }
            }
        } while (result);
        if (!messages.empty()) {
        	for (auto& msg : messages) {
        		msg.timestamp = current_timestamp;
        	}
        	this->midiMsgsIn.insert(this->midiMsgsIn.begin(), messages.cbegin(), messages.cend());
        	log_printf("insert %d messages to this->midiMsgsIn\n", messages.size());
        }
    }


    /* see if there is application midi data to process */
    while (!midiMsgsOut.empty()) {
//        /* see if it is time to output the next message */
    	MidiIOEvent& next = midiMsgsOut.back();
        if (next.timestamp <= current_timestamp) {
            /* time to send a message, first make sure it's not blocked */
            int status = Pm_MessageStatus(next.message);
            if ((status & 0xF8) == 0xF8) {
                ; /* real-time messages are not blocked */
            } else if (inputInSysex) {
                /* maybe sysex has timed out (output becomes unblocked) */
                if (last_timestamp + 5000 < current_timestamp) {
                    inputInSysex = FALSE;
                } else break; /* output is blocked, so exit loop */
            }
            midiMsgsOut.pop_back();
//            Pm_Dequeue(out_queue, &buffer);
            if (streamOut) {
            	Pm_Write(streamOut, &buffer, 1);
            }


            /* inspect message to update app_sysex_in_progress */
            if (status == MIDI_SYSEX) outputInSysex = TRUE;
            else if ((status & 0xF8) != 0xF8) {
                /* not MIDI_SYSEX and not real-time, so */
                outputInSysex = FALSE;
            }
            if (outputInSysex && /* look for EOX */
                (((buffer.message & 0xFF) == MIDI_EOX) ||
                 (((buffer.message >> 8) & 0xFF) == MIDI_EOX) ||
                 (((buffer.message >> 16) & 0xFF) == MIDI_EOX) ||
                 (((buffer.message >> 24) & 0xFF) == MIDI_EOX))) {
                outputInSysex = FALSE;
            }
        } else break; /* wait until indicated timestamp */
    }

	return 0;
}
midihost::midihost() {

}

bool midihost::initPm() {
	if (!pmIsInitalized) {
		PmError err;
		err = Pm_Initialize();
		if (err != pmNoError) {
			Pm_Terminate();
			error("Pa_Initialize", err);
		} else {
		    /* make the message queues */
//		    in_queue = Pm_QueueCreate(IN_QUEUE_SIZE, sizeof(PmEvent));
//		    assert(in_queue != NULL);
//		    out_queue = Pm_QueueCreate(OUT_QUEUE_SIZE, sizeof(PmEvent));
//		    assert(out_queue != NULL);
			pmIsInitalized = true;
		}
	}
	return pmIsInitalized;
}
void midihost::deinitPm() {
	if (pmIsInitalized) {
//	    Pm_QueueDestroy(in_queue);
//	    Pm_QueueDestroy(out_queue);
		Pm_Terminate();
		pmIsInitalized = false;
	}
}
void midihost::onStreamEnd() {

}
bool midihost::startMidi() {

	midiMsgsIn.clear();
	midiMsgsOut.clear();
    printf("MIDI input devices:\n");
    for (int i = 0; i < Pm_CountDevices(); i++) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        if (info->input) log_printf("%d: %s, %s\n", i, info->interf, info->name);
    }
    printf("MIDI output devices:\n");
    for (int i = 0; i < Pm_CountDevices(); i++) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        if (info->output) log_printf("%d: %s, %s\n", i, info->interf, info->name);
    }
	PmError err;
    const PmDeviceInfo *info;
    int id;
    id = 5 < Pm_CountDevices() ? 5 : Pm_GetDefaultOutputDeviceID();
    info = Pm_GetDeviceInfo(id);
    if (info == NULL) {
    	log_printf("Could not open default output device (%d).", id);

    } else {

        log_printf("Opening output device %s %s\n", info->interf, info->name);
        /* use zero latency because we want output to be immediate */
        PmStream* newStream = nullptr;
        err = Pm_OpenOutput(&newStream,
                      id,
                      NULL /* driver info */,
                      OUT_QUEUE_SIZE,
                      &getMidiTime,
                      NULL /* time info */,
                      0 /* Latency */);
        if (err != pmNoError) {
			error("Pm_OpenOutput", err);
			if (newStream) {
				Pm_Close(newStream);
			}
        } else {
			this->streamOut = newStream;
        }
    }

    id = 3 < Pm_CountDevices() ? 3 : Pm_GetDefaultInputDeviceID();
    info = Pm_GetDeviceInfo(id);
    if (info == NULL) {
        log_printf("Could not open default input device (%d).", id);
    } else {
        log_printf("Opening input device %s %s\n", info->interf, info->name);
        PmStream* newStream = nullptr;
        err = Pm_OpenInput(&newStream,
                     id,
                     NULL /* driver info */,
                     0 /* use default input size */,
                     &getMidiTime,
                     NULL /* time info */);
        if (err != pmNoError) {
			error("Pm_OpenOutput", err);
			if (newStream) {
				Pm_Close(newStream);
			}
        } else {
			this->streamIn = newStream;
        }
    }
    /* Note: if you set a filter here, then this will filter what goes
       to the MIDI THRU port. You may not want to do this.
     */
    if (this->streamIn) {
    	Pm_SetFilter(this->streamIn, 0);
    }
    return isStreaming();
}
bool midihost::stopMidi() {
	my_printf("stopMidi.\n", 0);
	bool bRet = this->streamIn || this->streamOut;
	if (this->streamIn) {
		PmError err = Pm_Close(this->streamIn);
		if (err != pmNoError) {
			error("Pm_Close", err);
		}
	}
	if (this->streamOut) {
		PmError err = Pm_Close(this->streamOut);
		if (err != pmNoError) {
			error("Pm_Close", err);
		}
	}
	return bRet;


}
