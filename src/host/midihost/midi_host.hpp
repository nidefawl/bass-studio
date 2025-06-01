#pragma once
#include "config.hpp"
#include "midi-event.hpp"
#include "samplerate.hpp"
#include "seq_time.hpp"
#include "str_util.hpp"
#include "types.hpp"
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
        std::vector<MidiIOEvent> midiBufferInspect;
        PmStream* stream{nullptr};
        String deviceName;
        int32_t deviceIdx{0};
        int32_t direction = 0; // 0 == input 1 == output
        bool preserveInputForInspection;
        bool bIsSoftwareDevice = false;
        bool bIsHiddenDevice = false;
    };

private:
    std::vector<opened_device_t> devicesInput;
    std::vector<opened_device_t> devicesOutput;
    bool pmIsInitalized    = false;
    bool enableProcessing  = true;
    bool inputInSysex      = false;
    double pmTimeBeginMs = 0.0;
    double streamTimeMs = 0.0;
    void handleMessage(PmMessage data, PmTimestamp timestamp, std::vector<MidiIOEvent>& messages);

public:
    midihost() = default;
    static midihost* getInstance();
    int32_t processMidiInput(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop);
    void processMidiOutput();
    std::vector<opened_device_t>& getDevicesInput() {
        return devicesInput;
    }
    std::vector<opened_device_t>& getDevicesOutput() {
        return devicesOutput;
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
    double getMidiStreamTime();
    void incrementMidiStreamTime(double deltaMs);
};
