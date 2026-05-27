#pragma once
#include "config.hpp"
#include "types.hpp"
#include "str_util.hpp"
#include "audio_config.hpp"
#include "daw_channel.hpp"
#include "dsp_util.hpp"
#include "hires_timer.hpp"
#include "note.hpp"
#include "rand.hpp"
#include "saferef.hpp"
#include "seq_time.hpp"
#include "threads/childprocessthread.hpp"
#include "tls.hpp"
#include "util/profiling.hpp"
#include "host/audiobuffer/audioblock.hpp"
#include "host/audiobuffer/audiobuffer.hpp"
#include "host/graph/effect_graph.hpp"
#include "host/graph/track_graph.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/plugin/base/base-plugin.hpp"
#include "host/plugin/modules.hpp"
#include "host/project/project.hpp"
#include "host/track/track.hpp"
#include <atomic>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>
#include <memory>

#ifdef __linux__
#define PLATFORM_PLUGIN_EXT "so"
#endif
#ifdef _WIN32
#define PLATFORM_PLUGIN_EXT "dll"
#endif
#ifdef __APPLE__
#define PLATFORM_PLUGIN_EXT "vst"
#endif
#define PLATFORM_CLAP_PLUGIN_EXT "clap"
#define PLATFORM_VST3_PLUGIN_EXT "vst3"

class clip_notes_t;
class effectbase;
class effect_deferred;
class vstplugin;
class vst3plugin;
class clapplugin;
class lv2plugin;
struct track_impl_t;
struct audio_stage_t;
struct track_audio_src;
struct audio_stage_ref_t;
struct plugin_snapshot_t;
struct track_id_snapshot_t;
class project_controller_t;
class AudioEffectX;
struct handles_t;

typedef AEffect*(VSTPluginMain_t) (audioMasterCallback audioMasterCB);
typedef AudioEffectX* (*FnCreateModule)(audioMasterCallback);

namespace DAW::Host {

struct LoadResultPluginImpl;
struct LoadResultPlugin {
    LoadResultPluginImpl* const impl;
    ~LoadResultPlugin();
    LoadResultPlugin(LoadResultPluginImpl _impl);
    LoadResultPluginImpl& operator*() const;
};

enum ProcessingQuality {
    Q_REALTIME,
    Q_PLAYBACK,
    Q_RENDER,
};

struct builtin_module_reg_t {
    int id = -1;
    bool isSynth;
    String name;
    FnCreateModule fnNewInstance;
};

class PluginHostCallback final : public IHostCallback {
    PluginManager* const host;
    public:
    explicit PluginHostCallback(PluginManager* _host)
    : IHostCallback(), host(_host) {
    }
    void onLatencyChanged(effectbase* effect) override {
        (void) host;
    }
    void onParametersChanged(effectbase* effect, int32_t idx, float val, int flags, int stage) override {

    }
    void onIOConfigChanged(effectbase* effect) override {

    }
    void onUiChanged(effectbase* effect) override;
};


// create tracks with 2 channels
static constexpr channelnum_t DEFAULT_CHANNEL_COUNT = 2;

/**
 * @brief Manages lifetime of plugins and audio stages
 * TODO: refactor this class: audiostages should be managed by separate class
 */
struct PluginLoadParameters {
    String filepath;
    uint32_t uId = 0;
    int32_t globalId = 0;
    uint64_t bugfixFlags = 0;
    ModuleType moduleType = ModuleType::MODULE_TYPE_VST2;
    String uIdVst3 = "";
    /** LV2 instance URI when @c moduleType is @c MODULE_TYPE_LV2. */
    String instanceUri;
};
class PluginManager {
private:
    class pluginmanager_impl;
    pluginmanager_impl* const mgrImpl;
    void registerModules();
    std::vector<audio_stage_t*> allAudioStages;
    std::vector<track_impl_t*> trackAudioStages;
    std::atomic<int32_t> pluginId{ 1 << 16 };
    std::atomic<int32_t> audioStageId{ 0 };
    std::atomic<int32_t> sampleId{ (1 << 30) };//TODO: collides with audiocache::nextIdx
    audioMasterCallback masterCallBackSlot = nullptr;
    int32_t hostSlot    = -1;
    class ModuleManager;
    ModuleManager* moduleMgr;
    std::vector<clapplugin*> pluginInstancesClap;
    std::vector<vstplugin*> pluginInstancesVST2;
    std::vector<vst3plugin*> pluginInstancesVST3;
    std::vector<lv2plugin*> pluginInstancesLv2;
    std::vector<effectbase*> pluginInstancesInternal;
    std::vector<effectbase*> pluginInstances;
    std::vector<effectbase*> pluginsDeferred;
    std::vector<builtin_module_reg_t> builtinModules;
    LoadResultPlugin loadInternalPlugin(int32_t type, int32_t globalId = 0);
    /* These are currently not called */
    void updatePluginWindows();
    int32_t& getTransportStateFlagsVst2();
    const int32_t& getTransportStateFlagsVst2() const;

public:
    virtual void onTrackLayoutChange() {
    }
    static const int FLAG_HOST_FORCELOAD_DISABLED_PLUGINS = 2;
    static bool assignMasterCallback(PluginManager* host);
    static bool assignVST2MasterCallback(PluginManager* host, ::DAW::Host::PluginHostCallback* cb);
    static void assignVST3MasterCallback(PluginManager* host);

    PluginManager() noexcept;
    virtual ~PluginManager();
    void setTls(daw_tls::tlsinstance& tls);
    daw_tls::tlsinstance& getTls() const;
    void destroy();
    void destroyVST2();
    void destroyVST3();
    std::vector<builtin_module_reg_t>& getBuiltinModuleRegistry() {
        return builtinModules;
    }
    IHostCallback* getHostCallback();
    int32_t getNextSampleId(int32_t id);
    int32_t getNextGlobalModuleId(int32_t globalId);

    void unloadPlugin(effectbase* plugin);
    void removePlugin(effectbase* plugin);
    void unloadTrack(track_t* track);
    effectbase* makeModuleInstance(int32_t moduleType, int32_t moduleId, int32_t globalid = -1);
    LoadResultPlugin loadPlugin(const PluginLoadParameters& req);
    effect_deferred* loadPluginDeferred(const plugin_snapshot_t& snapshot);
    void activateDeferred(effectbase* eff, int flags, effectbase** out_effectLoaded = nullptr);
    void updateSampleFormat(const sampleformat_t& _sampleFormat);
    void setProcessingQuality(ProcessingQuality quality);
    void UpdateVstTime(VstTimeInfo& timeInfo, const sampleformat_t& sampleFormat, const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state) const;

    void onBeforeBlock(const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state);
    virtual void onAudioStageChanged(audio_stage_t*) {
    }

    vstplugin* getPlugin(AEffect* aeffect);
    effectbase* getPluginById(int32_t projectGlobalId, bool activeOnly = true) const;
    void getAllInstances(std::vector<effectbase*>& effects);
    std::vector<vstplugin*> getVst2Instances() {
        return pluginInstancesVST2;
    }
    size_t getNumAudioStages() const {
        return allAudioStages.size();
    }
    size_t getNumPluginsLoaded() const {
        return pluginInstances.size();
    }
    bool addDeferredEffect(effectbase* plugin);
    void getDeferredEffects(std::vector<effectbase*>& effects) {
        effects = pluginsDeferred;
    }
    SafeRefStorage<effectbase>* getSafeRefStore();
    int32_t validateIds();

    bool movePluginsToStage(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t dst, int32_t len);
    bool movePluginsOnStage(audio_stage_t* trp, int32_t src, int32_t dst, int32_t len);
    bool insertNewPlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst);
    bool postPluginLoaded(audio_stage_t* trp, effectbase* plugin);
    bool replacePlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst, effectbase** prevPlugin);
    void assignNewStageId(audio_stage_t* trp, audio_stage_id_t newId);

    void createAudio(track_t* track, std::optional<audio_stage_id_t> stageId = std::nullopt);
    void releaseAudio(track_t* track);
    audio_stage_t* createAudioStage();
    void releaseAudioStage(audio_stage_t* audioStage);
    audio_stage_t* getAudioStage(const audio_stage_ref_t& ref) const;
    void updateMaximumStageId();
    audio_stage_id_t getNextGlobalAudioStageId(int32_t globalId = -1);
    bool isStageIdInUse(track_id_snapshot_t stageId);
    bool isStageIdInUse(const audio_stage_id_t& stageId);
    void checkScanner();
    void scanPlugins();
    bool isScanning();
    void stopScanner();
    void unload();
    void onTick();
    template<typename Functor>
    void visitAudioStageInstances(Functor f) {
        std::for_each(allAudioStages.begin(), allAudioStages.end(), f);
    }
    template<typename Functor>
    void visitTrackAudioStageInstances(Functor f) {
        std::for_each(trackAudioStages.begin(), trackAudioStages.end(), f);
    }
    template<typename Functor>
    void visitEffectbaseInstances(Functor f) {
        std::for_each(pluginInstances.begin(), pluginInstances.end(), f);
    }
};

} // namespace DAW::Host
