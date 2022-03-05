#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "str_util.h"
#include "seq_time.h"
#include "automation.h"
#include "logging.h"
#include "platform.h"
#include "meter.h"
#include "snapshot.h"
#include "modules.h"
#include "profiling.h"
#include "saferef.h"
#include "host/daw_channel.h"
#include "gui/table_fwd.h"

struct AudioBlock;
struct handles_t;
class track_t;
class guiplugin;
class vsthost;
struct audio_stage_t;
struct plugin_snapshot_t;
struct plugin_snapshot_t;
class effect_deferred;
class DawInstance;

extern bool storePluginPresetWithSnapshot;// = true;
extern bool loadPluginPresetWithSnapshot; // = false;

class effectbase : public automatable_t {
    friend class vsthost;
    friend class guiplugin;
    SafeRef<effectbase> safeRef;
#ifndef NDEBUG
    //helper indicator in gdb.
    //gdb cannot display std::string when built without clib-debug flag (SLOW)
    const char* szName = nullptr;
#endif
    int nLoadCalls = 0;

public:
    rmsmeterimpl<16000> meterIn;
    rmsmeterimpl<16000> meter;
    sampleformat_t format;
    AudioBlock* blockInputs  = nullptr;// guaranteed to have at least 2 channels
    AudioBlock* blockOutputs = nullptr;// guaranteed to have at least 2 channels
    int32_t pluginType       = 0;
    int32_t projectGlobalId;
    bool bIsEnabled           = false;
    bool bIsSetup             = false;
    bool bEditOpen            = false;
    bool bCaptureGUI          = false;
    bool bCanReceiveMidi      = false;
    int32_t requestCaptureGUI = 0;
    bool isSynth              = false;
    String sName;
    String sProductName;
    audio_stage_t* trackImpl = nullptr;
    int32_t slot             = -1;
    std::unique_ptr<DelayLine> delayLine;
    stats_processing_timings_t procStats;
    int midiEventsDispatched = 0;
    std::vector<DAW::channel_ref_t> inputChannels;

protected:
    vsthost* vstHost = nullptr;
    String currentProgramNameStr = "<no program>";
    bool currentProgramNameSet = false;
public:
    std::vector<String> programNames;
    
    std::vector<DAW::channel_desc> inputChannelsDesc;
    std::vector<DAW::channel_desc> outputChannelsDesc;
public:
    effectbase();
    effectbase(String _sName, int32_t _pluginType, int32_t _projectGlobalId);
    ~effectbase() override;
    SafeRef<effectbase> makeSafeRef();
    String getName() const { return sName; };
    const char* getDebugName() const { return szName; };
    String getProductName() const { return sProductName; };
    void setProductName(String _name) {
        replaceString(_name, "[jBridge]", "");
        this->sProductName = _name;
#ifndef NDEBUG
        this->szName = this->sName.c_str();
#endif
    }
    virtual int getModuleType()  = 0;
    virtual guiplugin* makeGui() = 0;
    virtual guiplugin* getGui()  = 0;

    virtual void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) = 0;
    virtual void process(AudioBlock* in, AudioBlock* out, double tick, int32_t samplePos, int32_t numSamples, playback_state state) = 0;
    virtual void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed);
    virtual bool show()   = 0;
    virtual bool close()  = 0;
    virtual void resume() = 0;
    virtual void sleep()  = 0;
    virtual void unload(vsthost* host, int flags);
    virtual void load(vsthost* host);
    virtual int32_t getPluginLatency()                = 0;
    virtual String getInfo(std::vector<String>& list) = 0;
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

protected:
    virtual void onEnable(){};
    virtual void onDisable(){};
    friend class effect_deferred;

public:
    virtual effect_deferred* toDeferred();
    virtual String formatDisplayValue(int32_t idx);
    void updateOnEnableParam(automatable_param_t* param, bool wasEnable, bool isEnable, int flags);
    virtual void getDeferredEffects(std::vector<effectbase*>& effects){};
    virtual void addPropertiesParameterList(Table::tbl& table);
    virtual void addPropertiesTooltip(Table::tbl& table);
};

struct effect_deferred_impl;
class effect_deferred : public effectbase {
public:
    effect_deferred_impl* mImpl = nullptr;

public:
    ~effect_deferred() override;
    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    int32_t getPluginLatency() override;
    String getInfo(std::vector<String>& list) override;
    int getModuleType() override;
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    guiplugin* makeGui() override;
    guiplugin* getGui() override;
    void process(AudioBlock* in, AudioBlock* out, double tick, int32_t samplePos, int32_t numSamples, playback_state state) override;
    bool show() override;
    bool close() override;
    void resume() override;
    void sleep() override;

    // automatable_t interface
    String getAutomatableName() override;
    float getParamValue(int32_t idx) override;
    void setParamValue(int32_t idx, float val, int flags) override;
    automationlane_snapshot_t toRef() const override;

    static std::shared_ptr<effect_deferred> fromEffect(effectbase* eff);
    String getDfrdPluginName() const;
    const plugin_snapshot_t& getSnapshotConst() const;
    plugin_snapshot_t& getSnapshot();
    void onPreUnload(int flags) override;
    bool isDeferred() override {
        return true;
    }
    void load(vsthost* host) override;
    bool isBypass() override {
        return true;
    }
    int getModuleStoredType() const;
};

effect_deferred* loadPluginDeferred(const plugin_snapshot_t& snapshot);
effectbase* loadEffectModule(vsthost* host, const plugin_snapshot_t& pluginSnapshot, bool isForceRequest);
void loadEffectParamsFromSnapshot(const plugin_snapshot_t& pluginSnapshot, effectbase* effect);
void removePlugin(DawInstance* daw, effectbase* module);
