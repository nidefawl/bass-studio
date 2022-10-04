#include "midi_host.h"
#include "appsettings.h"
#include "logging.h"
#include "midi-defs.h"
#include "platform.h"
#include "samplerate.h"
#include "str_util.h"
#include <portmidi.h>
#include "tls.h"
#include "types.h"

#define IN_QUEUE_SIZE  1024
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

const char nib_to_hex[] = "0123456789ABCDEF";

void showbytes(PmMessage data, int len) {
    int count = 0;
    int i;

    String s;
    for (i = 0; i < len; i++) {
        s += (nib_to_hex[(data >> 4) & 0xF]);
        s += (nib_to_hex[data & 0xF]);
        count += 2;
        if (count > 72) {
            s += ('.');
            s += ('.');
            s += ('.');
            break;
        }
        data >>= 8;
    }
    s += '\t';
    log_out("%s", StringAsCStr(s));
}


const char vel_format[] = "    Vel %d";

void midihost::handleMessage(PmMessage data, std::vector<MidiIOEvent>& messages) {
    bool verbose = false;
    int command; /* the current command */
    int chan;    /* the midi channel of the current event */

    /* log_lf(Log::L_DEBUG, "output data %8x; ", data); */

    command = Pm_MessageStatus(data) & MIDI_CODE_MASK;
    chan = Pm_MessageStatus(data) & MIDI_CHN_MASK;

    if (inputInSysex || Pm_MessageStatus(data) == MIDI_SYSEX) {
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
        if (verbose) showbytes(data, i);
        if (verbose) log_lf(Log::L_DEBUG, "System Exclusive\n");
    } else if (command == MIDI_ON_NOTE && Pm_MessageData2(data) != 0) {
        if (verbose) showbytes(data, 3);
        if (verbose) {
            String s = StringFormat("NoteOn Chan %2d Key %3d %s %s",
                                    chan,
                                    Pm_MessageData1(data),
                                    noteName(Pm_MessageData1(data)),
                                    StringAsCStr(StringFormat(vel_format, Pm_MessageData2(data))));
            log_out("%s\n", StringAsCStr(s));
        }
        messages.push_back({data, 0});
    } else if ((command == MIDI_ON_NOTE /* && Pm_MessageData2(data) == 0 */ || command == MIDI_OFF_NOTE)) {
        if (verbose) showbytes(data, 3);
        if (verbose) {
            String s = StringFormat("NoteOff Chan %2d Key %3d %s %s",
                                    chan,
                                    Pm_MessageData1(data),
                                    noteName(Pm_MessageData1(data)),
                                    StringAsCStr(StringFormat(vel_format, Pm_MessageData2(data))));
            log_out("%s\n", StringAsCStr(s));
        }
        messages.push_back({data, 0});
    } else if (command == MIDI_CH_PROGRAM) {
        if (verbose) showbytes(data, 2);
        if (verbose) {
            log_lf(Log::L_DEBUG, "  ProgChg Chan %2d Prog %2d\n", chan, Pm_MessageData1(data) + 1);
        }
    } else if (command == MIDI_CTRL) {
        /* controls 121 (MIDI_RESET_CONTROLLER) to 127 are channel
         * mode messages. */
        if (Pm_MessageData1(data) < MIDI_ALL_SOUND_OFF) {
            if (verbose) showbytes(data, 3);
            if (verbose) {
                log_lf(Log::L_DEBUG, "CtrlChg Chan %2d Ctrl %2d Val %2d\n", chan, Pm_MessageData1(data), Pm_MessageData2(data));
            }
        } else /* channel mode */ /*if (chmode)*/ {
            if (verbose) showbytes(data, 3);
            if (verbose) {
                switch (Pm_MessageData1(data)) {
                    case MIDI_ALL_SOUND_OFF:
                        log_lf(Log::L_DEBUG, "All Sound Off, Chan %2d\n", chan);
                        break;
                    case MIDI_RESET_CONTROLLERS:
                        log_lf(Log::L_DEBUG, "Reset All Controllers, Chan %2d\n", chan);
                        break;
                    case MIDI_LOCAL:
                        log_lf(Log::L_DEBUG, "LocCtrl Chan %2d %s\n", chan, Pm_MessageData2(data) ? "On" : "Off");
                        break;
                    case MIDI_ALL_OFF:
                        log_lf(Log::L_DEBUG, "All Off Chan %2d\n", chan);
                        break;
                    case MIDI_OMNI_OFF:
                        log_lf(Log::L_DEBUG, "OmniOff Chan %2d\n", chan);
                        break;
                    case MIDI_OMNI_ON:
                        log_lf(Log::L_DEBUG, "Omni On Chan %2d\n", chan);
                        break;
                    case MIDI_MONO_ON:
                        log_lf(Log::L_DEBUG, "Mono On Chan %2d\n", chan);
                        if (Pm_MessageData2(data)) log_lf(Log::L_DEBUG, " to %d received channels\n", Pm_MessageData2(data));
                        else
                            log_lf(Log::L_DEBUG, " to all received channels\n");
                        break;
                    case MIDI_POLY_ON:
                        log_lf(Log::L_DEBUG, "Poly On Chan %2d\n", chan);
                        break;
                }
            }
        }
    } else if (command == MIDI_POLY_TOUCH) {
        if (verbose) showbytes(data, 3);
        if (verbose) {
            String s = StringFormat("P.Touch Chan %2d Key %3d %s %s",
                                    chan,
                                    Pm_MessageData1(data),
                                    noteName(Pm_MessageData1(data)),
                                    StringAsCStr(StringFormat(vel_format, Pm_MessageData2(data))));
            log_out("%s\n", StringAsCStr(s));
        }
    } else if (command == MIDI_TOUCH) {
        if (verbose) showbytes(data, 2);
        if (verbose) {
            log_lf(Log::L_DEBUG, "  A.Touch Chan %2d Val %2d\n", chan, Pm_MessageData1(data));
        }
    } else if (command == MIDI_BEND) {
        if (verbose) showbytes(data, 3);
        if (verbose) {
            log_lf(Log::L_DEBUG, "P.Bend  Chan %2d Val %2d\n", chan, (Pm_MessageData1(data) + (Pm_MessageData2(data) << 7)));
        }
        messages.push_back({data, 0});
    } else if (Pm_MessageStatus(data) == MIDI_SONG_POINTER) {
        if (verbose) showbytes(data, 3);
        if (verbose) {
            log_lf(Log::L_DEBUG, "    Song Position %d\n", (Pm_MessageData1(data) + (Pm_MessageData2(data) << 7)));
        }
    } else if (Pm_MessageStatus(data) == MIDI_SONG_SELECT) {
        if (verbose) showbytes(data, 2);
        if (verbose) {
            log_lf(Log::L_DEBUG, "    Song Select %d\n", Pm_MessageData1(data));
        }
    } else if (Pm_MessageStatus(data) == MIDI_TUNE_REQ) {
        if (verbose) showbytes(data, 1);
        if (verbose) {
            log_lf(Log::L_DEBUG, "    Tune Request\n");
        }
    } else if (Pm_MessageStatus(data) == MIDI_Q_FRAME /* && realdata */) {
        if (verbose) showbytes(data, 2);
        if (verbose) {
            log_lf(Log::L_DEBUG, "    Time Code Quarter Frame Type %d Values %d\n",
                       (Pm_MessageData1(data) & 0x70) >> 4,
                       Pm_MessageData1(data) & 0xf);
        }
    } else if (Pm_MessageStatus(data) == MIDI_START /* && realdata */) {
        if (verbose) showbytes(data, 1);
        if (verbose) {
            log_lf(Log::L_DEBUG, "    Start\n");
        }
    } else if (Pm_MessageStatus(data) == MIDI_CONTINUE /* && realdata */) {
        if (verbose) showbytes(data, 1);
        if (verbose) {
            log_lf(Log::L_DEBUG, "    Continue\n");
        }
    } else if (Pm_MessageStatus(data) == MIDI_STOP /* && realdata */) {
        if (verbose) showbytes(data, 1);
        if (verbose) {
            log_lf(Log::L_DEBUG, "    Stop\n");
        }
    } else if (Pm_MessageStatus(data) == MIDI_SYS_RESET /* && realdata */) {
        if (verbose) showbytes(data, 1);
        if (verbose) {
            log_lf(Log::L_DEBUG, "    System Reset\n");
        }
    } else if (Pm_MessageStatus(data) == MIDI_TIME_CLOCK) {
        if (verbose) showbytes(data, 1);
        if (verbose) {
            log_lf(Log::L_DEBUG, "    Clock\n");
        }
    } else if (Pm_MessageStatus(data) == MIDI_ACTIVE_SENSING) {
        if (verbose) showbytes(data, 1);
        if (verbose) {
            log_lf(Log::L_DEBUG, "    Active Sensing\n");
        }
    } else if (verbose)
        showbytes(data, 3);
    //    fflush(stdout);
}

int32_t getMidiTime(void* userData) {
    int32_t time = static_cast<int32_t>(static_cast<uint64_t>(getTimeMillis()) & (0x7FFF'FFFFLL));
    return time;
}
#define TIMEOUT_TEMP_NOTES 2000
// HACK: inject midi preview note
int32_t midihost::triggerNote(int32_t deviceIdx, int32_t channel, int32_t pitch, int32_t velocity) {
    //    log_lf(Log::L_DEBUG, "trigger %d\n", pitch);
    int32_t status = 0x90 | (channel & 0xF);
    PmMessage msg = Pm_Message(status, pitch, velocity);
    int32_t current_timestamp = getMidiTime(nullptr);
    MidiIOEvent evt{msg, current_timestamp};
    for (auto& dev : devicesInput) {
        if (deviceIdx < 0 || dev.deviceIdx == deviceIdx) {
            dev.temporaryNotes.push_back(evt);
            dev.midiMsgs.insert(dev.midiMsgs.begin(), evt);
        }
    }
    return current_timestamp;
}
int32_t midihost::killNote(int32_t deviceIdx, int32_t channel, int32_t pitch) {

    //    log_lf(Log::L_DEBUG, "kill %d\n", pitch);
    int32_t status = 0x80 | (channel & 0xF);
    PmMessage msg = Pm_Message(status, pitch, 0);
    int32_t current_timestamp = getMidiTime(nullptr);
    MidiIOEvent evt{msg, current_timestamp};
    for (auto& dev : devicesInput) {
        if (deviceIdx < 0 || dev.deviceIdx == deviceIdx) {
            dev.midiMsgs.insert(dev.midiMsgs.begin(), evt);
        }
    }

    return current_timestamp;
}
/* timer interrupt for processing midi data.
   Incoming data is delivered to main program via in_queue.
   Outgoing data from main program is delivered via out_queue.
   Incoming data from midi_in is copied with low latency to  midi_out.
   Sysex messages from either source block messages from the other.
 */
int32_t midihost::processMidi(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state,
                              bool inLoop, bool isLoopAround) {
    PmError result;
    PmEvent buffer; /* just one message at a time */
    /* if (current_timestamp % 1000 == 0)
        log_lf(Log::L_DEBUG, "time %d\n", current_timestamp); */

    auto current_timestamp = getMidiTime(nullptr);
    /* do nothing until initialization completes */
    if (!enableProcessing || !isStreaming()) {
        //        /* this flag signals that no more midi processing will be done */
        //        process_midi_exit_flag = TRUE;
        return 0;
    }

    //    /* see if there is any midi input to process */
    for (auto& dev : devicesInput) {
        auto streamIn = dev.stream;
        assert(streamIn);
        std::vector<MidiIOEvent> messages;
        do {
            result = Pm_Poll(streamIn);
            if (result) {
                int status;
                int rslt = Pm_Read(streamIn, &buffer, 1);
                if (rslt == pmBufferOverflow) continue;
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
                    (((buffer.message & 0xFF) == MIDI_EOX) || (((buffer.message >> 8) & 0xFF) == MIDI_EOX) ||
                     (((buffer.message >> 16) & 0xFF) == MIDI_EOX) || (((buffer.message >> 24) & 0xFF) == MIDI_EOX))) {
                    inputInSysex = FALSE;
                }
            }
        } while (result);
        if (!messages.empty()) {
            for (auto& msg : messages) {
                assert(0 == msg.timestamp);
                msg.timestamp = current_timestamp;
            }
            dev.midiMsgs.insert(dev.midiMsgs.begin(), messages.cbegin(), messages.cend());
        }
        // kill temporary notes after timeout
        if (!dev.temporaryNotes.empty()) {
            auto it = dev.temporaryNotes.begin();
            while (it != dev.temporaryNotes.end()) {
                auto& msg = *it;
                if (current_timestamp - msg.timestamp >= TIMEOUT_TEMP_NOTES) {

//                    int32_t channel = Pm_MessageStatus(msg.message) & MIDI_CHN_MASK;
//                    int32_t status = 0x80 | (channel & 0xF);
//                    int32_t pitch = Pm_MessageData1(msg.message);
//                    int32_t velocity = 0;
//                    PmMessage noteOffMessage = Pm_Message(status, pitch, velocity);
//                    MidiIOEvent noteOffEvt {
//                        noteOffMessage,
//                        current_timestamp
//                    };
//                    dev.midiMsgs.insert(dev.midiMsgs.begin(), noteOffEvt);

                    it = dev.temporaryNotes.erase(it);
                } else {
                    it++;
                }
            }
        }
    }


    for (auto& dev : devicesOutput) {
        assert(dev.stream);

        /* see if there is application midi data to process */
        while (!dev.midiMsgs.empty()) {
            //        /* see if it is time to output the next message */
            MidiIOEvent& next = dev.midiMsgs.back();
            if (next.timestamp <= current_timestamp) {
                /* time to send a message, first make sure it's not blocked */
                int status = Pm_MessageStatus(next.message);
                bool isRealTime = (status & 0xF8) == 0xF8;
                /* real-time messages are not blocked */
                if (!isRealTime && inputInSysex) {
                    /* maybe sysex has timed out (output becomes unblocked) */
                    if (last_timestamp + 5000 < current_timestamp) {
                        inputInSysex = FALSE;
                    } else
                        break; /* output is blocked, so exit loop */
                }
                dev.midiMsgs.pop_back();
//                Pm_Dequeue(out_queue, &buffer);
                Pm_Write(dev.stream, &buffer, 1);


                /* inspect message to update app_sysex_in_progress */
                if (status == MIDI_SYSEX) outputInSysex = TRUE;
                else if ((status & 0xF8) != 0xF8) {
                    /* not MIDI_SYSEX and not real-time, so */
                    outputInSysex = FALSE;
                }
                if (outputInSysex && /* look for EOX */
                    (((buffer.message & 0xFF) == MIDI_EOX) || (((buffer.message >> 8) & 0xFF) == MIDI_EOX) ||
                     (((buffer.message >> 16) & 0xFF) == MIDI_EOX) || (((buffer.message >> 24) & 0xFF) == MIDI_EOX))) {
                    outputInSysex = FALSE;
                }
            } else
                break; /* wait until indicated timestamp */
        }
    }

    return 0;
}

bool midihost::initPm() {
    if (!pmIsInitalized) {
        PmError err = Pm_Initialize();
        if (err != pmNoError) {
            Pm_Terminate();
            error("Pa_Initialize", err);
        } else {
            pmIsInitalized = true;
        }
    }
    return pmIsInitalized;
}
void midihost::deinitPm() {
    if (pmIsInitalized) {
        Pm_Terminate();
        pmIsInitalized = false;
    }
}
bool midihost::isInitialized() const {
    return pmIsInitalized;
}
void midihost::onStreamEnd() {}
template <typename T, typename T2>
std::vector<midi_channel> syncOpenCloseDeviceList(T& cfg, T2& openedDevs) {
    std::vector<midi_channel> toOpen;
    for (auto& input : cfg) {
        auto it = std::find_if(openedDevs.cbegin(), openedDevs.cend(), [&](const auto& openedDevice) {
            return openedDevice.deviceName == input.deviceName;
        });
        if (it == openedDevs.cend()) {
            toOpen.push_back(input);
        }
    }
    auto it = openedDevs.begin();
    while (it != openedDevs.end()) {
        midihost::opened_device_t& dev = *it;
        auto it2 = std::find_if(cfg.cbegin(), cfg.cend(), [&dev](const midi_channel& iocfg) {
            return iocfg.deviceName == dev.deviceName;
        });
        if (it2 == cfg.cend()) {
            assert(dev.stream);
            Pm_Close(dev.stream);
            dev.stream = nullptr;
            it = openedDevs.erase(it);
        } else {
            it++;
        }
    }
    return toOpen;
}
void midihost::reopenAllConfiguredDevices(bool forceClose) {
    if (forceClose) {
        std::vector<midi_channel> empty;
        syncOpenCloseDeviceList(empty, this->devicesInput);
        syncOpenCloseDeviceList(empty, this->devicesOutput);
    }
    auto& settings = daw_tls::getSettings();
    app_iomidiconfig& midiSettings = settings.iosettings.getIOConfigMidi("stdmidi");
    {

        std::vector<midi_channel> toOpen = syncOpenCloseDeviceList(midiSettings.inputs, this->devicesInput);
        for (midi_channel& port : toOpen) {
            for (int deviceIdx = 0; deviceIdx < Pm_CountDevices(); deviceIdx++) {
                const PmDeviceInfo* info = Pm_GetDeviceInfo(deviceIdx);
                if (info->input) {
                    if (port.deviceName == info->name) {
                        log_printf("Opening input device %s %s\n", info->interf, info->name);
                        PmStream* newStream = nullptr;
                        auto err = Pm_OpenInput(&newStream,
                                                deviceIdx,
                                                nullptr /* driver info */,
                                                0 /* use default input size */,
                                                &getMidiTime,
                                                nullptr /* time info */);
                        if (err != pmNoError) {
                            error("Pm_OpenInput", err);
                            if (newStream) {
                                Pm_Close(newStream);
                            }
                        } else {
                            /* Note: if you set a filter here, then this will filter what goes
                               to the MIDI THRU port. You may not want to do this.
                             */
                            Pm_SetFilter(newStream, 0);
                            this->devicesInput.push_back(opened_device_t{{}, {}, newStream, info->name, deviceIdx, 0});
                        }
                        break;
                    }
                }
            }
        }
    }
    {

        std::vector<midi_channel> toOpen = syncOpenCloseDeviceList(midiSettings.outputs, this->devicesOutput);
        for (midi_channel& port : toOpen) {
            for (int deviceIdx = 0; deviceIdx < Pm_CountDevices(); deviceIdx++) {
                const PmDeviceInfo* info = Pm_GetDeviceInfo(deviceIdx);
                if (info->output) {
                    if (port.deviceName == info->name) {
                        log_printf("Opening output device %s %s\n", info->interf, info->name);
                        PmStream* newStream = nullptr;
                        auto err = Pm_OpenOutput(&newStream,
                                                 deviceIdx,
                                                 nullptr /* driver info */,
                                                 OUT_QUEUE_SIZE,
                                                 &getMidiTime,
                                                 nullptr /* time info */,
                                                 0 /* Latency */);
                        if (err != pmNoError) {
                            error("Pm_OpenOutput", err);
                            if (newStream) {
                                Pm_Close(newStream);
                            }
                        } else {
                            this->devicesOutput.push_back(opened_device_t{{}, {}, newStream, info->name, deviceIdx, 1});
                        }
                        break;
                    }
                }
            }
        }
    }
}
bool midihost::startMidi() {
    log_printf("MIDI input devices:\n");
    for (int i = 0; i < Pm_CountDevices(); i++) {
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        if (info->input) log_printf("%d: %s, %s\n", i, info->interf, info->name);
    }
    log_printf("MIDI output devices:\n");
    for (int i = 0; i < Pm_CountDevices(); i++) {
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        if (info->output) log_printf("%d: %s, %s\n", i, info->interf, info->name);
    }
    assert(this->devicesInput.empty());
    assert(this->devicesOutput.empty());
    reopenAllConfiguredDevices(false);
    return isStreaming();
}
bool midihost::stopMidi() {
    log_printf("stopMidi.\n");
    bool ret = !this->devicesInput.empty() || !this->devicesInput.empty();
    std::vector<midi_channel> empty;
    syncOpenCloseDeviceList(empty, this->devicesInput);
    syncOpenCloseDeviceList(empty, this->devicesOutput);
    return ret;
}

std::vector<MidiIOEvent> midihost::getInputMessages() {
    std::vector<MidiIOEvent> ret;
    for (auto& dev : devicesInput) {
        ret.insert(ret.end(), dev.midiMsgs.cbegin(), dev.midiMsgs.cend());
        dev.midiMsgs.clear();
    }
    std::sort(ret.begin(), ret.end(), [](auto& a, auto& b) {
        if (a.timestamp == b.timestamp) {
            // Put note on before note off
            bool isNoteOnA = (Pm_MessageStatus(a.message) & MIDI_CODE_MASK) == 0x90;
            bool isNoteOnB = (Pm_MessageStatus(b.message) & MIDI_CODE_MASK) == 0x90;
            if (isNoteOnA != isNoteOnB) {
                return isNoteOnA;
            }
        }
        return a.timestamp < b.timestamp;
    });
    //    if (ret.size() > 0) {
    //        int idx = 0;
    //        for (auto &a : ret) {
    //            int32_t status = Pm_MessageStatus(a.message) & MIDI_CODE_MASK;
    //            if (status & 0x80) {
    //                log_lf(Log::L_DEBUG, "note[%d] %s %s %d\n", idx, (status==0x80)!=0?"kill":"trig",
    //                noteName(Pm_MessageData1(a.message)), a.timestamp);
    //            }
    //            idx++;
    //        }
    //    }
    return ret;
}
