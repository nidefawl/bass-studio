#pragma once
#include "config.h"
#include "modules.h"
#include "str_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include <functional>
#include <utility>
#include <vector>
#include <atomic>
#include "threads/childprocessthread.h"
#include "tls.h"
#include "types.h"
#include <map>

#include <vstsdk-host-2.4/aeffectx.h>
#include "note.h"
#include "rand.h"
#include "hires_timer.h"
#include "project.h"
#include "audio_config.h"
#include "audiobuffer.h"
#include "audioblock.h"
#include "saferef.h"
#include "track.h"
#include "track_graph.h"
#include "effect_graph.h"
#include "daw_channel.h"
#include "util/profiling.h"
#include "host/host_pluginmanager.h"


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

class clip_notes_t;
class effectbase;
class effect_deferred;
class vstplugin;
struct track_impl_t;
struct audio_stage_t;
struct track_audio_src;
struct audio_stage_ref_t;
struct plugin_snapshot_t;
struct track_id_snapshot_t;
class project_controller_t;
class AudioEffectX;
class DawInstance;
struct handles_t;

typedef AEffect*(VSTPluginMain_t) (audioMasterCallback audioMasterCB);
typedef AudioEffectX* (*FnCreateModule)(audioMasterCallback);

namespace DAW::Host {

struct builtin_module_reg_t {
    int id = -1;
    bool isSynth;
    String name;
    FnCreateModule fnNewInstance;
};

struct LoadResultVST2Plugin {
public:
    LoadResultVST2Plugin(int32_t _result, vstplugin* _plugin) : result(_result), plugin(_plugin), shellPluginHandle(nullptr){};
    LoadResultVST2Plugin(int32_t _result, vstplugin* _plugin, handles_t* _shellHandle, String _path, String _name)
        : result(_result), plugin(_plugin), shellPluginHandle(_shellHandle), path(std::move(_path)), name(std::move(_name)){};
    int32_t result;
    vstplugin* plugin;
    handles_t* shellPluginHandle;
    String path;
    String name;
};


class PluginHostCallback : public IHostCallback {
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
class PluginManager {
private:
    /**
    * pluginmanager internals
    */
    class pluginmanager_impl {
    public:
        daw_tls::tlsinstance tls;
        std::unique_ptr<ProcessThread> vstscannerProcessThread;
        int scanningState = 0;
        int32_t vst2TransportStateFlags = 0;
    };
    pluginmanager_impl* const mgrImpl;
    void registerModules();
    std::vector<audio_stage_t*> allAudioStages;
    std::vector<track_impl_t*> trackAudioStages;
    std::atomic<int32_t> pluginId{ 1 << 16 };
    std::atomic<int32_t> audioStageId{ 100 };
    std::atomic<int32_t> sampleId{ (1 << 30) };//TODO: collides with audiocache::nextIdx
    audioMasterCallback masterCallBackSlot = nullptr;
    int32_t hostSlot    = -1;
    class ModuleManager;
    ModuleManager* moduleMgr;
    std::vector<vstplugin*> pluginInstancesVST2;
    std::vector<effectbase*> pluginInstancesInternal;
    std::vector<effectbase*> pluginInstances;
    std::vector<effectbase*> pluginsDeferred;
    std::vector<builtin_module_reg_t> builtinModules;
    SafeRefStorage<effectbase> safeRefs;
    std::shared_ptr<PluginHostCallback> pluginHostCallback;
    LoadResultVST2Plugin loadInternalPlugin(int32_t type, int32_t globalId = 0);
    /* These are currently not called */
    void onPluginsChanged(audio_stage_t* stage);
    void updatePluginWindows();
public:
    std::function<void()> onTrackLayoutChange;
    static const int FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY    = 1;
    static const int FLAG_HOST_FORCELOAD_DISABLED_PLUGINS = 2;
    static bool assignMasterCallback(PluginManager* host);
    PluginManager() noexcept;
    ~PluginManager();
    void setTls(daw_tls::tlsinstance& tls);
    daw_tls::tlsinstance& getTls() const {
        return mgrImpl->tls;
    }
    void destroy();
    std::vector<builtin_module_reg_t>& getBuiltinModuleRegistry() {
        return builtinModules;
    }
    IHostCallback* getHostCallback();
    int32_t getNextSampleId(int32_t id);
    int32_t getNextGlobalModuleId(int32_t globalId);

    void unloadPlugin(effectbase* plugin, int flags = 0);
    void removePlugin(effectbase* plugin);
    void unloadTrack(track_t* track);
    effectbase* makeModuleInstance(int32_t moduleType, int32_t moduleId, int32_t globalid = -1);
    LoadResultVST2Plugin loadPlugin(String filepath, uint32_t uId, int32_t globalId = 0, uint64_t bugfixFlags = 0);
    effect_deferred* loadPluginDeferred(const plugin_snapshot_t& snapshot);
    void activateDeferred(effectbase* eff, int flags, effectbase** out_effectLoaded = nullptr);
    void updateSampleFormat(const sampleformat_t& _sampleFormat);
    void UpdateVstTime(VstTimeInfo& timeInfo, const sampleformat_t& sampleFormat, const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state) const;
    void onBeforeBlock(const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state);

    vstplugin* getPlugin(AEffect* aeffect);
    effectbase* getPluginById(int32_t projectGlobalId, bool activeOnly = true) const;
    void getAllInstances(std::vector<effectbase*>& effects);
    std::vector<vstplugin*> getVst2Instances() {
        return pluginInstancesVST2;
    }
    bool addDeferredEffect(effectbase* plugin);
    void getDeferredEffects(std::vector<effectbase*>& effects) {
        effects = pluginsDeferred;
    }
    SafeRefStorage<effectbase>* getSafeRefStore() {
        return &safeRefs;
    }
    int32_t validateIds();

    bool movePlugins(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t dst, int32_t len);
    bool moveEffects(audio_stage_t* trp, int32_t src, int32_t dst, int32_t len);
    bool insertNewPlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst);
    bool postPluginLoaded(audio_stage_t* trp, effectbase* plugin);
    bool replacePlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst, effectbase** prevPlugin);


    void createAudio(track_t* track);
    void releaseAudio(track_t* track);
    audio_stage_t* createAudioStage();
    void releaseAudioStage(audio_stage_t* audioStage);
    audio_stage_t* getAudioStage(const audio_stage_ref_t& ref) const;
    void updateMaximumStageId();
    audio_stage_id_t getNextGlobalAudioStageId(int32_t globalId);
    bool isStageIdInUse(track_id_snapshot_t stageId);
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
