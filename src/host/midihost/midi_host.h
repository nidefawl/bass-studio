#pragma once
#include "config.h"
#include "midi-event.h"
#include "samplerate.h"
#include "seq_time.h"
#include "str_util.h"
#include "types.h"
#include <vector>

class project_controller_t;
using PmStream = void;
using PmQueue = void;
using PmMessage = uint32_t;
using PmTimestamp = int32_t;

int32_t getMidiTime(void* userData);

class midihost {
public:
    struct opened_device_t {
        std::vector<MidiIOEvent> midiMsgs;
        std::vector<MidiIOEvent> temporaryNotes;
        std::vector<MidiIOEvent> midiBufferInspect;
        PmStream* stream{nullptr};
        String deviceName;
        int32_t deviceIdx{0};
        int32_t direction = 0; // 0 == input 1 == output
        bool preserveInputForInspection;
    };

private:
    std::vector<opened_device_t> devicesInput;
    std::vector<opened_device_t> devicesOutput;
    bool pmIsInitalized    = false;
    bool enableProcessing  = true;
    bool inputInSysex      = false;
    bool outputInSysex     = false;
    int32_t last_timestamp = 0;
    void handleMessage(PmMessage data, PmTimestamp timestamp, std::vector<MidiIOEvent>& messages);

public:
    midihost() = default;
    static midihost* getInstance();
    //void enqueue(AudioBuffer*);
    int32_t processMidi(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop,
                        bool isLoopAround);
    bool hasInputMessages() {
        return std::any_of(devicesInput.cbegin(), devicesInput.cend(), [](auto &dev) {
            return dev.midiMsgs.size() > 0;
        });
    }
    std::vector<MidiIOEvent> getInputMessages();
    std::vector<opened_device_t>& getDevicesInput() {
        return devicesInput;
    }
    void reopenAllConfiguredDevices(bool forceClose);
    bool initPm();
    void deinitPm();
    bool isInitialized() const;
    void onStreamEnd();
    bool startMidi();
    bool stopMidi();
    bool isStreaming() {
        bool ret = !this->devicesInput.empty() || !this->devicesInput.empty();
        return ret;
    }

    // HACK: inject midi preview note
    int32_t triggerNote(int32_t deviceIdx, int32_t channel, int32_t pitch, int32_t velocity);
    int32_t killNote(int32_t deviceIdx, int32_t channel, int32_t pitch);

    void setInspection(bool bInput, bool bInspectionEnabled);
    std::vector<MidiIOEvent> getInspectionInputMessages();
};
