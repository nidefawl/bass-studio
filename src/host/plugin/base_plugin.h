#pragma once
#include "types.h"
#include <memory>
#include <utility>
#include <vector>
#include "str_util.h"
#include "seq_time.h"
#include "automation.h"
#include "logging.h"
#include "platform.h"
#include "meter.h"
#include "snapshot.h"
#include "modules.h"
#include "util/profiling.h"
#include "saferef.h"
#include "host/daw_channel.h"
#include "gui/table/table_fwd.h"
#include "plugins/synth/IPlugMidi.h"

namespace DAW::Host {
    class Host;
    class PluginManager;
}
class DawInstance;
class effect_deferred;
class guiplugin;
class host_plugin_window;
class track_t;
struct audio_stage_t;
struct AudioBlock;
struct handles_t;
struct plugin_snapshot_t;
struct plugin_snapshot_t;

extern bool storePluginPresetWithSnapshot;// = true;
extern bool loadPluginPresetWithSnapshot; // = false;
struct midi_events_t;
namespace PluginWrapper {
    class PluginInternalVST2;
}
class effectbase : public automatable_t {
    friend class PluginWrapper::PluginInternalVST2;
    friend class DAW::Host::PluginManager;
    friend class DAW::Host::Host;
    friend class guiplugin;
    friend class effect_deferred;

    std::shared_ptr<DAW::meter_runningsum[]> meterDataInput;
    std::shared_ptr<DAW::meter_runningsum[]> meterDataOutput;
public:
    std::vector<DAW::channel_ref_t> inputChannels;
    std::unique_ptr<DelayLine> delayLine;
    DAW::rmsmeter meter;
    DAW::rmsmeter meterIn;
    sampleformat_t format;
    AudioBlock* blockInputs        = nullptr;// guaranteed to have at least 2 channels
    AudioBlock* blockOutputs       = nullptr;// guaranteed to have at least 2 channels
    int32_t pluginType             = 0;
    int32_t projectGlobalId        = 0;
    IHostCallback* hostCallback  = nullptr;
    bool bIsEnabled                = false;
    bool bEditOpen                 = false;
    bool bCaptureGUI               = false;
    bool bCanReceiveMidi           = false;
    bool bCanSendMidi              = false;
    bool bMPESupport               = false;
    bool bSupportsWindowResize     = false;
    bool isSynth                   = false;
    bool bWindowPosSizeValid       = false;
    bool bInEditIdle               = false;
    int32_t slot                   = -1;
    int midiEventsDispatched       = 0;
    audio_stage_t* trackImpl       = nullptr;
    host_plugin_window* windowHost = nullptr;
#ifndef NDEBUG
    //helper indicator in gdb.
    //gdb cannot display std::string when built without clib-debug flag (SLOW)
    const char* szName = nullptr;
#endif
    ivec4 lastWindowPosSize{};
    String sName;
    String sProductName;
    String sVendorName;
    stats_processing_timings_t procStats;
    plugin_ui_snapshot_t uiSnapshot{};
    SafeRef<effectbase> safeRef;
    int32_t requestCaptureGUI    = 0;
protected:
    int nLoadCalls               = 0;
    DAW::Host::PluginManager* pluginMgr = nullptr;
    String currentProgramNameStr = "<no program>";
    bool currentProgramNameSet   = false;

public:
    std::vector<String> programNames;
    std::vector<DAW::channel_desc> inputChannelsDesc;
    std::vector<DAW::channel_desc> outputChannelsDesc;

protected:
    void initDefaultIODesc();
    void initBuffers();
    void initMeters();

public:
    effectbase(String _sName, int32_t _pluginType, int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~effectbase() override;
    
    SafeRef<effectbase> makeSafeRef();
    String getName() const { return sName; };
    String getProductName() const { return sProductName; };
    void setProductName(String _name) {
        replaceString(_name, "[jBridge]", "");
        this->sProductName = std::move(_name);
#ifndef NDEBUG
        this->szName = this->sName.c_str();
#endif
    }
    String getVendorName() const { return sVendorName; };
    void setVendorName(String _name) {
        this->sVendorName = std::move(_name);
    }
    virtual int32_t getEffectVersion() const { return 1; }
    virtual int32_t getUUID_U32() const { return 1; }
    IHostCallback* getHostCallback() const { return hostCallback; }

    virtual void onEnable(){};
    virtual void onDisable(){};
    virtual int getModuleType()  = 0;
    virtual guiplugin* makeGui() = 0;
    virtual guiplugin* getGui()  = 0;

    virtual void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) = 0;
    virtual void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) = 0;
    virtual void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed);
    virtual void processMidi(midi_events_t& midiEvents);
    virtual void sendNotesOff();
    virtual bool hasWindowEditor() {
        return false;
    }
    virtual bool showWindow(bool bResetPosition);
protected:
    bool openWindow(bool bResetPosition, ivec2 defaultSize);
public:
    virtual bool closeWindow();
    virtual void onWindowDestroy();
    virtual void onWindowResize(ivec2 size);
    virtual bool onShow(host_plugin_window* window);
    virtual bool onClose();
    virtual void updateWindow();
    virtual ivec2 constrainWindowSize(host_plugin_window* window, ivec2 size) {
        return size;
    };
    virtual void unload(DAW::Host::PluginManager* host, int flags);
    virtual void load(DAW::Host::PluginManager* host);
    virtual samplecount_t getPluginLatency() = 0;
    virtual String getInfo(std::vector<String>& list) { return ""; };
    track_t* getTrack() override;
    virtual void onTick(double since);
    virtual void setSampleFormat(sampleformat_t sampleFormat) {
        format = sampleFormat;
        if (blockInputs && blockInputs->samples != sampleFormat.blockSize)
            blockInputs->realloc(sampleFormat.blockSize);

        if (blockOutputs && blockOutputs->samples != sampleFormat.blockSize)
            blockOutputs->realloc(sampleFormat.blockSize);
    }
    virtual sampleformat_t getSampleFormat();
    virtual void getChildAudioStages(std::vector<audio_stage_t*>& targets) {
    }
    virtual void loadSnapshot(const plugin_snapshot_t& snapshot) = 0;
    virtual void breakTrackLink();
    virtual void setTrackLink(audio_stage_t* audioStage);
    virtual void onPreUnload(int flags) {
    }
    virtual bool isBypass() {
        return !this->bIsEnabled;
    }
    virtual void setSlot(int32_t i) {
        slot = i;
    }
    virtual int32_t getSlot() {
        return slot;
    }
    virtual audio_stage_t* getTrackLink() {
        return trackImpl;
    }
    virtual bool isDeferred() {
        return false;
    }
    virtual bool getCurrentProgramName(String& out) {
        return false;
    }
    virtual bool setCurrentProgram(uint32_t index) {
        return false;
    }
    virtual bool getCurrentProgram(uint32_t& index) {
        return false;
    }
    virtual bool getNumberOfPrograms(uint32_t& index) {
        return false;
    }
    bool hasTrackLink() const {
        return trackImpl != nullptr;
    }

    void storeWindowPos(ivec2 posSize) {
        this->lastWindowPosSize.x = posSize.x;
        this->lastWindowPosSize.y = posSize.y;
    }
    void storeWindowPosSize(ivec4 posSize) {
        this->lastWindowPosSize = posSize;
        this->bWindowPosSizeValid = true;
    }
    bool getLastWindowPosSize(ivec4& posSize) {
        posSize = this->lastWindowPosSize;
        return this->bWindowPosSizeValid;
    }

public:
    virtual effect_deferred* toDeferred();
    void updateOnEnableParam(automatable_param_t* param, bool wasEnable, bool isEnable, int flags);
    virtual void getDeferredEffects(std::vector<effectbase*>& effects){};
    virtual void addPropertiesParameterList(Table::tbl& table);
    virtual void addPropertiesTooltip(Table::tbl& table);
    virtual void addPropertiesParameterTooltip(Table::tbl& table, int idx);
};

struct effect_deferred_impl;
class effect_deferred : public effectbase {
public:
    effect_deferred_impl* mImpl = nullptr;

public:
    effect_deferred(int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~effect_deferred() override;
    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    samplecount_t getPluginLatency() override;
    String getInfo(std::vector<String>& list) override;
    int getModuleType() override;
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    guiplugin* makeGui() override;
    guiplugin* getGui() override;
    void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    bool hasWindowEditor() override {
        return false;
    }
    // automatable_t interface
    String getAutomatableName() override;
    float getParamValue(int32_t idx) override;
    void setParamValue(int32_t idx, float val, int flags) override;
    automationlane_snapshot_t toRef() const override;

    static std::shared_ptr<effect_deferred> fromEffect(effectbase* eff);
    String getDfrdPluginName() const;
    const plugin_snapshot_t& getSnapshotConst() const;
    plugin_snapshot_t& getSnapshot();
    bool isDeferred() override {
        return true;
    }
    bool isBypass() override {
        return true;
    }
    int getModuleStoredType() const;
};
namespace DAW {
    effectbase* loadEffectModule(PluginManager* host, const plugin_snapshot_t& pluginSnapshot, bool isForceRequest);
    void loadEffectParamsFromSnapshot(const plugin_snapshot_t& pluginSnapshot, effectbase* effect);
    void removePlugin(DawInstance* daw, effectbase* module);
}
