#pragma once
#include <array>
#include <atomic>
#include <clap/events.h>
#include <clap/ext/latency.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <semaphore>

#include <clap/clap.h>
#include <clap/helpers/event-list.hh>
#include <clap/helpers/reducing-param-queue.hh>

#include "host/audiobuffer/audioblock.h"
#include "host/host_plugin_window.h"
#include "host/host_pluginmanager.h"
#include "host/plugin/base/base-plugin.h"
#include "host/plugin/clap/clap-plugin-param.h"
#include "samplerate.h"
#include "types.h"

namespace DAW::Host {
    class PluginManager;
}
class PluginHostSettings;
using WId = WINDOW_HANDLE;
struct ClapPluginDescription {
    uint32_t clapVerMajor;
    uint32_t clapVerMinor;
    uint32_t clapVerRevision;
    String id;
    String name;
    String vendor;
    String url;
    String manualUrl;
    String supportUrl;
    String version;
    String description;
    std::vector<String> features;
};
class clapplugin final : public effectbase {
private:

    struct ParamModulation {
        int32_t index;
        std::vector<float> values;
    };
    struct daw_handles_t {
        std::map<int32_t, std::unique_ptr<guiplugin>> gui;
        samplecount_t currentLatency = 0;
        struct param_editing_t {
            int32_t paramIdx = -1;
            float   valBefore = 0;
        } paramEditing;
        std::vector<clap_audio_buffer> clapInputBuffers;
        std::vector<clap_audio_buffer> clapOutputBuffers;
        std::vector<AudioBlock> dawInputBuffers;
        std::vector<AudioBlock> dawOutputBuffers;
        clap_event_transport_t transport{};
        DAW::Host::LoadResultSharedLibrary library;
        bool bIsStopProcessing = false;
        std::vector<IMidiMsg> midiEvents;
        std::vector<ParamModulation> paramModulations;
        std::vector<ParamModulation> paramAutomations;
    };
    daw_handles_t* const dawHandles;
    int pluginCategory = -1;
    uint64_t lastInputEvent = 0;
    uint32_t pluginCount = 0;
public:
    clapplugin(DAW::Host::PluginManager& pluginMgr, String filePath, const String& name, uint32_t uId, int32_t globalId, IHostCallback* hostcallback);
    ~clapplugin() override;

    // effectbase
    int getModuleType() override { return PLUGIN_TYPE_CLAP; };
    int getModuleCategory() const { return pluginCategory; };
    void* getModuleHandle() { return dawHandles->library.module; }
    String getAutomatableName() override {
        return this->sName;
    }
    clap_event_transport_t& getTransport() { return dawHandles->transport; }
    std::shared_ptr<guiplugin> createGuiPlugin(int32_t uuid) override;
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) override;
    void updateAutomatedParameters(const DAW::Host::PluginManager *const host, tick_t processingPos, playback_state state) override;
    void processMidiMessages(std::vector<IMidiMsg>& midiEvents) override;
    void sendNotesOff() override;
    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    samplecount_t getPluginLatency() override;
    void unload(DAW::Host::PluginManager* host) override;
    void load(DAW::Host::PluginManager* host) override;
    void initBuffers() override;
    void updateFromMainThread() override;
    void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
    automatable_param_t* getParam(int32_t idx) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
    void onWindowResize(ivec2 size) override;
    bool onShow(host_plugin_window* window) override;
    bool onClose() override;
    ivec2 constrainWindowSize(host_plugin_window* window, ivec2 size) override;
    bool hasWindowEditor() override;
    bool showWindow(bool bResetPosition) override;
    void updateWindowSize();
    void configureIOPorts();
    void onEnable() override;
    void onDisable() override;

    // clap host side
    const clap_plugin_entry* getPluginEntry() const {
        return _pluginEntry;
    }
    const clap_plugin_factory* getPluginFactory() const {
        return _pluginFactory;
    }
    uint32_t getPluginCount() const {
        return pluginCount;
    }
    
    ClapPluginDescription getDescription();

    bool loadClapPlugin(DAW::Host::LoadResultSharedLibrary& lib);
    void unloadClapPlugin();

    bool canActivate() const;
private:
    bool getIsAudioTheadOverride() const;
    void activate(sampleformat_t sampleFormat);
    void deactivate();

    void setPluginWindowVisibility(bool isVisible);

    void setParentWindow(WId parentWindow);

    void processBegin(int nframes);
    void processNoteOn(int sampleOffset, int channel, int key, int velocity);
    void processNoteOff(int sampleOffset, int channel, int key, int velocity);
    void processNoteAt(int sampleOffset, int channel, int key, int pressure);
    void processPitchBend(int sampleOffset, int channel, int value);
    void processCC(int sampleOffset, int channel, int cc, int value);
    void processEnd(int nframes);

    void updateClapFromMainThread();

    void initPluginExtensions();
    void initThreadPool();
    void terminateThreadPool();
    void threadPoolEntry();

    void setParamValueByHost(PluginParam& param, double value);
    void setParamModulationByHost(PluginParam& param, double value);

    auto& params() const { return _params; }
    auto& quickControlsPages() const { return _quickControlsPages; }
    auto& quickControlsPagesIndex() const { return _quickControlsPagesIndex; }
    auto quickControlsSelectedPage() const { return _quickControlsSelectedPage; }
    void setQuickControlsSelectedPageByHost(clap_id page_id);

    bool loadNativePluginPreset(const std::string& path);
    bool loadStateFromFile(const std::string& path);
    bool saveStateToFile(const std::string& path);

    static void checkForMainThread();
    static void checkForAudioThread();

    String paramValueToText(clap_id paramId, double value);

    //previously signals
    void paramsChanged();
    void paramAdjusted(clap_id paramId);
    void quickControlsPagesChanged() { /* TODO */
    }
    void quickControlsSelectedPageChanged() { /* TODO */
    }

private:
    static clapplugin* fromHost(const clap_host* host);
    template<typename T>
    void initPluginExtension(const T*& ext, const char* id);

    /* clap host callbacks */
    static void clapLog(const clap_host* host, clap_log_severity severity, const char* msg);

    static void clapRequestCallback(const clap_host* host);
    static void clapRequestRestart(const clap_host* host);
    static void clapRequestProcess(const clap_host* host);

    static bool clapIsMainThread(const clap_host* host);
    static bool clapIsAudioThread(const clap_host* host);

    static void clapParamsRescan(const clap_host* host, clap_param_rescan_flags flags);
    static void clapParamsClear(const clap_host* host, clap_id param_id, clap_param_clear_flags flags);
    static void clapParamsRequestFlush(const clap_host* host);
    void scanParams();
    void scanParam(int32_t index);
    double getClapParamValue(const clap_param_info &info);
    static bool clapParamsRescanMayValueChange(uint32_t flags) {
        return flags & (CLAP_PARAM_RESCAN_ALL | CLAP_PARAM_RESCAN_VALUES);
    }
    static bool clapParamsRescanMayInfoChange(uint32_t flags) {
        return flags & (CLAP_PARAM_RESCAN_ALL | CLAP_PARAM_RESCAN_INFO);
    }

    void scanQuickControls();
    void quickControlsSetSelectedPage(clap_id pageId);
    static void clapQuickControlsChanged(const clap_host* host);

    static bool clapRegisterTimer(const clap_host* host, uint32_t period_ms, clap_id* timer_id);
    static bool clapUnregisterTimer(const clap_host* host, clap_id timer_id);
    static bool clapRegisterPosixFd(const clap_host* host, int fd, clap_posix_fd_flags_t flags);
    static bool clapModifyPosixFd(const clap_host* host, int fd, clap_posix_fd_flags_t flags);
    static bool clapUnregisterPosixFd(const clap_host* host, int fd);
    void eventLoopSetFdNotifierFlags(int fd, int flags);

    static bool clapThreadPoolRequestExec(const clap_host* host, uint32_t num_tasks);

    static const void* clapExtension(const clap_host* host, const char* extension);

    /* clap host gui callbacks */
    static void clapGuiResizeHintsChanged(const clap_host_t* host);
    static bool clapGuiRequestResize(const clap_host* host, uint32_t width, uint32_t height);
    static bool clapGuiRequestShow(const clap_host* host);
    static bool clapGuiRequestHide(const clap_host* host);
    static void clapGuiClosed(const clap_host* host, bool wasDestroyed);
    static void clapLatencyChanced(const clap_host* host);

    static void clapStateMarkDirty(const clap_host* host);

    bool canUsePluginParams() const noexcept;
    bool canUsePluginGui() const noexcept;
    static const char* getCurrentClapGuiApi();

    void paramFlushOnMainThread();
    void handlePluginOutputEvents();
    void generatePluginInputEvents();

private:
    DAW::Host::PluginManager& _pluginMgr;
    sampleformat_t _sampleFormat{};
    int64_t steady_time = 0;
    clap_host host_{};
    static const constexpr clap_host_log _hostLog = {
        clapplugin::clapLog,
    };
    static const constexpr clap_host_gui _hostGui = {
        clapplugin::clapGuiResizeHintsChanged,
        clapplugin::clapGuiRequestResize,
        clapplugin::clapGuiRequestShow,
        clapplugin::clapGuiRequestHide,
        clapplugin::clapGuiClosed,
    };
    static const constexpr clap_host_latency _hostLatency = {
        clapplugin::clapLatencyChanced,
    };

    // static const constexpr clap_host_audio_ports hostAudioPorts_;
    // static const constexpr clap_host_audio_ports_config hostAudioPortsConfig_;
    static const constexpr clap_host_params _hostParams = {
        clapplugin::clapParamsRescan,
        clapplugin::clapParamsClear,
        clapplugin::clapParamsRequestFlush,
    };
    static const constexpr clap_host_quick_controls _hostQuickControls = {
        clapplugin::clapQuickControlsChanged,
        nullptr,
    };
    static const constexpr clap_host_timer_support _hostTimerSupport = {
        clapplugin::clapRegisterTimer,
        clapplugin::clapUnregisterTimer,
    };
    static const constexpr clap_host_posix_fd_support _hostPosixFdSupport = {
        clapplugin::clapRegisterPosixFd,
        clapplugin::clapModifyPosixFd,
        clapplugin::clapUnregisterPosixFd,
    };

    static const constexpr clap_host_thread_check _hostThreadCheck = {
        clapplugin::clapIsMainThread,
        clapplugin::clapIsAudioThread,
    };
    static const constexpr clap_host_thread_pool _hostThreadPool = {
        clapplugin::clapThreadPoolRequestExec,
    };
    static const constexpr clap_host_state _hostState = {
        clapplugin::clapStateMarkDirty,
    };

    const clap_plugin_entry* _pluginEntry                     = nullptr;
    const clap_plugin_factory* _pluginFactory                 = nullptr;
    const clap_plugin* _plugin                                = nullptr;
    const clap_plugin_params* _pluginParams                   = nullptr;
    const clap_plugin_quick_controls* _pluginQuickControls    = nullptr;
    const clap_plugin_audio_ports* _pluginAudioPorts          = nullptr;
    const clap_plugin_gui* _pluginGui                         = nullptr;
    const clap_plugin_timer_support* _pluginTimerSupport      = nullptr;
    const clap_plugin_posix_fd_support* _pluginPosixFdSupport = nullptr;
    const clap_plugin_thread_pool* _pluginThreadPool          = nullptr;
    const clap_plugin_preset_load* _pluginPresetLoad          = nullptr;
    const clap_plugin_state* _pluginState                     = nullptr;
    const clap_plugin_latency* _pluginLatency                 = nullptr;

    bool _pluginExtensionsAreInitialized = false;

    /* timers */
    class ClapHostPluginTimer {
        public:
        clap_id id;
        uint32_t period_ms;
        hires_timer_t tm;
        ClapHostPluginTimer(clap_id id, uint32_t period_ms)
            : id(id), period_ms(period_ms) {}
        bool update() {
            auto since = tm.getTime();
            if (since >= period_ms * 1000LL) {
                tm.reset();
                return true;
            }
            return false;
        }
    };
    clap_id _nextTimerId = 0;
    std::unordered_map<clap_id, std::unique_ptr<ClapHostPluginTimer>> _timers;

    /* fd events */
    struct Notifiers;
    std::unordered_map<int, std::shared_ptr<Notifiers>> _fds;

    /* thread pool */
    class ClapHostPooledThread {
    public:
        ClapHostPooledThread() = default;
    };
    class ClapHostSemaphore {
    private:
        std::counting_semaphore<32> _semaphore{ 0 };

    public:
        ClapHostSemaphore()                                    = default;
        ClapHostSemaphore(const ClapHostSemaphore&)            = delete;
        ClapHostSemaphore(ClapHostSemaphore&&)                 = delete;
        ClapHostSemaphore& operator=(const ClapHostSemaphore&) = delete;
        ClapHostSemaphore& operator=(ClapHostSemaphore&&)      = delete;
        void acquire(uint32_t value) {
            for (uint32_t i = 0; i < value; ++i) {
                _semaphore.acquire();
            }
        }
        void release(uint32_t value) {
            _semaphore.release(value);
        }
    };
    std::vector<std::unique_ptr<ClapHostPooledThread>> _threadPool;
    std::atomic<bool> _threadPoolStop     = { false };
    std::atomic<int> _threadPoolTaskIndex = { 0 };
    ClapHostSemaphore _threadPoolSemaphoreProd;
    ClapHostSemaphore _threadPoolSemaphoreDone;

    /* process stuff */
    clap::helpers::EventList _eventListInput;
    clap::helpers::EventList _evOut;
    clap_process _process{};

    void pushInputEvent(clap_event_header_t* ev);

    /* param update queues */
    std::unordered_map<clap_id, std::unique_ptr<PluginParam>> _params;

    struct AppToEngineParamQueueValue {
        void* cookie;
        double value;
    };

    struct EngineToAppParamQueueValue {
        void update(const EngineToAppParamQueueValue& v) noexcept {
            if (v.has_value) {
                has_value = true;
                value     = v.value;
            }

            if (v.has_gesture) {
                has_gesture = true;
                is_begin    = v.is_begin;
            }
        }

        bool has_value   = false;
        bool has_gesture = false;
        bool is_begin    = false;
        double value     = 0;
    };

    clap::helpers::ReducingParamQueue<clap_id, AppToEngineParamQueueValue> _appToEngineValueQueue;
    clap::helpers::ReducingParamQueue<clap_id, AppToEngineParamQueueValue> _appToEngineModQueue;
    clap::helpers::ReducingParamQueue<clap_id, EngineToAppParamQueueValue> _engineToAppValueQueue;

    std::unordered_map<clap_id, bool> _isAdjustingParameter;

    std::vector<std::unique_ptr<clap_quick_controls_page>> _quickControlsPages;
    std::unordered_map<clap_id, clap_quick_controls_page*> _quickControlsPagesIndex;
    clap_id _quickControlsSelectedPage = CLAP_INVALID_ID;

    /* delayed actions */
    enum PluginState {
        // The plugin is inactive, only the main thread uses it
        Inactive,

        // Activation failed
        InactiveWithError,

        // The plugin is active and sleeping, the audio engine can call set_processing()
        ActiveAndSleeping,

        // The plugin is processing
        ActiveAndProcessing,

        // The plugin did process but is in error
        ActiveWithError,

        // The plugin is not used anymore by the audio engine and can be deactivated on the main
        // thread
        ActiveAndReadyToDeactivate,
    };

    bool isPluginActive() const;
    bool isPluginProcessing() const;
    bool isPluginSleeping() const;
    void setPluginState(PluginState state);

    PluginState _state = Inactive;
    bool _stateIsDirty = false;

    bool _scheduleRestart    = false;
    bool _scheduleDeactivate = false;

    bool _scheduleProcess = true;

    bool _scheduleParamFlush = false;

    const char* _guiApi = nullptr;
    bool _isGuiCreated  = false;
    bool _isGuiVisible  = false;
    bool _isGuiFloating = false;

    bool _scheduleMainThreadCallback = false;

    String filePath;
    uint32_t clapPluginIndex;
public:
    String getClapPluginPath() const {
        return filePath;
    }
    uint32_t getClapPluginIndex() const {
        return clapPluginIndex;
    }
    String getClapPluginId() const {
        return _plugin && _plugin->desc ? _plugin->desc->id : "";
    }
};
