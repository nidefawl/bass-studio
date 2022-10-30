#include <clap/ext/latency.h>
#include <clap/plugin.h>
#include <clap/stream.h>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>

#include "assert_dbg.h"
#include "buildinfo.h"
#include "clap-plugin.h"
#include "gui/plugin/plugin.h"
#include "hires_timer.h"
#include "host/history.h"
#include "host/history.h"
#include "host/host_plugin_window.h"
#include "host/plugin/clap/clap-plugin-param.h"
#include "logging.h"
#include "modules.h"
#include "plugins/synth/IPlugMidi.h"
#include "samplerate.h"
#include "seq_time.h"
#include "snapshot/plugin-snapshot.h"
#include "str_util.h"
#include "thread.h"
#include "track_impl.h"
#include "types.h"

#include <clap/helpers/reducing-param-queue.hxx>
#include <utility>
#include <vector>

#define HLOG "Claphost: "

namespace {
    struct clap_snapshot_ostream : public clap_ostream {
        std::vector<uint8_t>& dataChunk;
        explicit clap_snapshot_ostream(std::vector<uint8_t>& data) : clap_ostream(), dataChunk(data) {
            ctx   = this;
            write = write_cb;
        }
        static int64_t CLAP_ABI write_cb(const struct clap_ostream* stream, const void* voidBuffer, uint64_t size) {
            auto self   = reinterpret_cast<const clap_snapshot_ostream*>(stream);
            auto buffer = reinterpret_cast<const uint8_t*>(voidBuffer);
            auto pos    = self->dataChunk.size();
            self->dataChunk.resize(self->dataChunk.size() + size);
            std::memcpy(self->dataChunk.data() + pos, buffer, size);
            return size;
        }
    };

    void createSnapshot(plugin_snapshot_t& ps, clapplugin* plugin, const clap_plugin* clapPlugin, const clap_plugin_state* _pluginState, const tracksnapshot_store_opts_t& opts) {
        ps.version           = 11;
        ps.slot              = 0;
        ps.projectGlobalId   = plugin->projectGlobalId;
        ps.enabled           = plugin->bIsEnabled;
        ps.ioChannels.input  = plugin->inputChannelsDesc;
        ps.ioChannels.output = plugin->outputChannelsDesc;
        ps.pluginType        = PLUGIN_TYPE_CLAP;
        ps.vendorVersion     = 0;
        ps.uId               = plugin->getClapPluginIndex();
        ps.clapId            = plugin->getClapPluginId();
        ps.localDbId         = plugin->localDbId;
        ps.name              = plugin->sName;
        if (opts.storePluginPreset) {
            if (_pluginState) {
                clap_snapshot_ostream clapByteStream{ ps.dataChunk };
                _pluginState->save(clapPlugin, &clapByteStream);
            }
            auto numParamsReserve = math::min<int32_t>(150, plugin->getNumParameters());
            ps.params.reserve(numParamsReserve);
            plugin->visitParams([&ps](auto& mapEntry) {
                automatable_param_t& param = mapEntry.second;
                if (param.inUse) {
                    float curValue = param.getValue();
                    int paramFlags = param.inUse ? 1 : 0;
                    ps.params.push_back(param_snapshot_t{ param.idx, curValue, paramFlags });
                }
            });
            if (plugin->programNames.size() > 1) {
                uint32_t curProgramNr = 0;
                plugin->getCurrentProgram(curProgramNr);
                ps.currentProgram = curProgramNr;
            }
        }
        if (opts.storeAutomation) {
            storeAutomation(ps.automatedParams, plugin);
        }
    }

    struct clap_snapshot_istream : public clap_istream {
        std::vector<uint8_t> dataChunk;
        mutable int64_t readPos = 0;
        explicit clap_snapshot_istream(std::vector<uint8_t> data) : clap_istream(), dataChunk(std::move(data)) {
            ctx  = this;
            read = read_cb;
        }
        static int64_t CLAP_ABI read_cb(const struct clap_istream* stream, void* buffer, uint64_t size) {
            auto self        = reinterpret_cast<const clap_snapshot_istream*>(stream);
            auto data        = reinterpret_cast<uint8_t*>(buffer);
            auto srcSize     = int64_t(self->dataChunk.size());
            auto dstSize     = math::max<int64_t>(0, int64_t(size));
            auto srcSizeLeft = srcSize - self->readPos;
            auto sizeRead    = math::min(dstSize, srcSizeLeft);
            if (size) {
                std::memset(data, 0, size);
            }
            if (sizeRead > 0) {
                dbgassert(self->readPos + sizeRead <= srcSize && self->readPos + sizeRead >= 0 && self->readPos >= 0);
                std::memcpy(data, self->dataChunk.data() + self->readPos, sizeRead);
                self->readPos += sizeRead;
            }
            return sizeRead;
        }
    };

    double ToPluginParam(PluginParam* param, double f) {
        auto& info = param->info();
        return info.min_value + f * (info.max_value - info.min_value);
    }
    double FromPluginParam(PluginParam* param, double f) {
        auto& info = param->info();
        return (f - info.min_value) / (info.max_value - info.min_value);
    }
}// namespace

void clapplugin::loadSnapshot(const plugin_snapshot_t& pluginSnapshot) {
    this->uiSnapshot                 = pluginSnapshot.uiSnapshot;
    this->uiSnapshot.isValidSnapshot = true;
    if (_pluginState) {
        clap_snapshot_istream clapByteStream{ pluginSnapshot.dataChunk };
        _pluginState->load(_plugin, &clapByteStream);
    }
    DAW::loadEffectParamsFromSnapshot(pluginSnapshot, this);
    if (dawHandles->gui && this->uiSnapshot.isValidSnapshot) {
        dawHandles->gui->loadSnapshot(this->uiSnapshot);
        this->uiSnapshot.isValidSnapshot = false;
    }
}

void clapplugin::makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) {
    createSnapshot(ps, this, _plugin, _pluginState, opts);
    if (dawHandles->gui) {
        dawHandles->gui->makeSnapshot(ps.uiSnapshot, opts);
    }
    ps.slot = this->slot;
}

guiplugin* clapplugin::makeGui() {
    if (!dawHandles->gui) {
        dawHandles->gui = std::make_shared<guiclapplugin>(this);
        if (dawHandles->gui && this->uiSnapshot.isValidSnapshot) {
            dawHandles->gui->loadSnapshot(this->uiSnapshot);
            this->uiSnapshot.isValidSnapshot = false;
        }
        dawHandles->gui->setTitle(StringFormat("%s (Clap)", StringAsCStr(this->sName)));
    }

    return dawHandles->gui.get();
}

guiplugin* clapplugin::getGui() {
    return dawHandles->gui.get();
}

clapplugin::clapplugin(DAW::Host::PluginManager& pluginMgr, String filePath, const String& name, uint32_t uId, int32_t globalId, IHostCallback* hostcallback)
    : effectbase(name, PLUGIN_TYPE_CLAP, globalId, hostcallback), dawHandles{ new clapplugin::daw_handles_t{} },
      _pluginMgr(pluginMgr),
      filePath(std::move(filePath)),
      clapPluginIndex(uId) {
    host_.host_data        = this;
    host_.clap_version     = CLAP_VERSION;
    host_.name             = BuildInfo::BUILD_BINARY_NAME;
    host_.version          = BuildInfo::BUILD_BINARY_VERSION;
    host_.vendor           = "";
    host_.url              = "";
    host_.get_extension    = clapplugin::clapExtension;
    host_.request_callback = clapplugin::clapRequestCallback;
    host_.request_process  = clapplugin::clapRequestProcess;
    host_.request_restart  = clapplugin::clapRequestRestart;

    initThreadPool();
}

clapplugin::~clapplugin() {
    checkForMainThread();
    terminateThreadPool();
    delete dawHandles;
}

void clapplugin::initThreadPool() {
    checkForMainThread();
    _threadPoolStop      = false;
    _threadPoolTaskIndex = 0;
    /* TODO */
    // auto N = QThread::idealThreadCount();
    // _threadPool.resize(N);
    // for (int i = 0; i < N; ++i) {
    //    _threadPool[i].reset(QThread::create(&PluginHost::threadPoolEntry, this));
    //    _threadPool[i]->start(QThread::HighestPriority);
    // }
}

void clapplugin::terminateThreadPool() {
    checkForMainThread();
    _threadPoolStop = true;
    /* TODO */
    // _threadPoolSemaphoreProd.release(_threadPool.size());
    // for (auto &thr : _threadPool)
    //    if (thr)
    //       thr->wait();
}

void clapplugin::threadPoolEntry() {
    /* TODO */
    // while (true) {
    //    _threadPoolSemaphoreProd.acquire();
    //    if (_threadPoolStop)
    //       return;

    //    int taskIndex = _threadPoolTaskIndex++;
    //    _pluginThreadPool->exec(_plugin, taskIndex);
    //    _threadPoolSemaphoreDone.release();
    // }
}

bool clapplugin::loadClapPlugin(DAW::Host::LoadResultSharedLibrary& _library) {
    checkForMainThread();

    _pluginEntry = reinterpret_cast<const struct clap_plugin_entry*>(_library.entryPoint);
    dbgassert(_pluginEntry);

    // String path;
    // SplitPath(filePath, &path, nullptr, nullptr, nullptr);
    // _pluginEntry->init(path.c_str());
    _pluginEntry->init(filePath.c_str());

    _pluginFactory = static_cast<const clap_plugin_factory*>(_pluginEntry->get_factory(CLAP_PLUGIN_FACTORY_ID));

    pluginCount = _pluginFactory->get_plugin_count(_pluginFactory);
    if (clapPluginIndex > pluginCount) {
        log_lf(Log::L_ERROR, HLOG "plugin index greater than count (%d/%d)\n", clapPluginIndex, pluginCount);
        return false;
    }

    auto desc = _pluginFactory->get_plugin_descriptor(_pluginFactory, clapPluginIndex);
    if (!desc) {
        log_lf(Log::L_ERROR, HLOG "no plugin descriptor (%d/%d)\n", clapPluginIndex, pluginCount);
        return false;
    }

    if (!clap_version_is_compatible(desc->clap_version)) {
        log_lf(Log::L_ERROR, HLOG "Incompatible clap version: Plugin is %d.%d.%d. Host is %d.%d.%d\n",
               desc->clap_version.major, desc->clap_version.minor, desc->clap_version.revision,
               CLAP_VERSION.major, CLAP_VERSION.minor, CLAP_VERSION.revision);
        return false;
    }

    _plugin = _pluginFactory->create_plugin(_pluginFactory, &host_, desc->id);
    if (!_plugin) {
        log_lf(Log::L_WARN, "could not create the plugin with id: %s\n", desc->id);
        return false;
    }

    if (!_plugin->init(_plugin)) {
        log_lf(Log::L_WARN, "could not init the plugin with id: %s\n", desc->id);
        return false;
    }
    configureIOPorts();

    for (int i = 0; _plugin->desc->features && _plugin->desc->features[i]; ++i) {
        auto entry = _plugin->desc->features[i];
        if (!strcmp(entry, CLAP_PLUGIN_FEATURE_INSTRUMENT)) {
            isSynth = true;
            bCanReceiveMidi = true;
            pluginCategory = 1;
        }
        if (!strcmp(entry, CLAP_PLUGIN_FEATURE_AUDIO_EFFECT)) {
            pluginCategory = 0;
        } 
        if (!strcmp(entry, CLAP_PLUGIN_FEATURE_NOTE_EFFECT)) {
            bCanSendMidi = true;
        }
    }
    

    if (_plugin->desc && _plugin->desc->name) {
        sName = _plugin->desc->name;
    }

    _process.transport = &dawHandles->transport;
    _process.in_events  = _eventListInput.clapInputEvents();
    _process.out_events = _evOut.clapOutputEvents();

    this->dawHandles->library = _library;

    initPluginExtensions();
    scanParams();
    scanQuickControls();
    return true;
}

ClapPluginDescription clapplugin::getDescription() {
    auto clapPlugDesc = _plugin ? _plugin->desc : nullptr;
    if (!clapPlugDesc)
        return {};

    ClapPluginDescription desc;
    desc.clapVerMajor    = clapPlugDesc->clap_version.major;
    desc.clapVerMinor    = clapPlugDesc->clap_version.minor;
    desc.clapVerRevision = clapPlugDesc->clap_version.revision;
    if (clapPlugDesc->id)
        desc.id = clapPlugDesc->id;
    if (clapPlugDesc->name)
        desc.name = clapPlugDesc->name;
    if (clapPlugDesc->vendor)
        desc.vendor = clapPlugDesc->vendor;
    if (clapPlugDesc->url)
        desc.url = clapPlugDesc->url;
    if (clapPlugDesc->manual_url)
        desc.version = clapPlugDesc->manual_url;
    if (clapPlugDesc->support_url)
        desc.version = clapPlugDesc->support_url;
    if (clapPlugDesc->version)
        desc.version = clapPlugDesc->version;
    if (clapPlugDesc->description)
        desc.version = clapPlugDesc->description;
    for (int i = 0; clapPlugDesc->features && clapPlugDesc->features[i]; ++i) {
        desc.features.emplace_back(clapPlugDesc->features[i]);
    }
    return desc;
}

void clapplugin::initPluginExtensions() {
    checkForMainThread();

    if (_pluginExtensionsAreInitialized)
        return;

    initPluginExtension(_pluginParams, CLAP_EXT_PARAMS);
    initPluginExtension(_pluginQuickControls, CLAP_EXT_QUICK_CONTROLS);
    initPluginExtension(_pluginAudioPorts, CLAP_EXT_AUDIO_PORTS);
    initPluginExtension(_pluginGui, CLAP_EXT_GUI);
    initPluginExtension(_pluginTimerSupport, CLAP_EXT_TIMER_SUPPORT);
    initPluginExtension(_pluginPosixFdSupport, CLAP_EXT_POSIX_FD_SUPPORT);
    initPluginExtension(_pluginThreadPool, CLAP_EXT_THREAD_POOL);
    initPluginExtension(_pluginPresetLoad, CLAP_EXT_PRESET_LOAD);
    initPluginExtension(_pluginState, CLAP_EXT_STATE);
    initPluginExtension(_pluginLatency, CLAP_EXT_LATENCY);

    _pluginExtensionsAreInitialized = true;
}

void clapplugin::unloadClapPlugin() {
    checkForMainThread();

    if (_isGuiCreated) {
        if (_isGuiVisible)
            _pluginGui->hide(_plugin);
        _pluginGui->destroy(_plugin);
        _isGuiCreated = false;
        _isGuiVisible = false;
    }

    deactivate();

    if (_plugin) {
        _plugin->destroy(_plugin);
        _plugin = nullptr;
    }
    _pluginGui            = nullptr;
    _pluginTimerSupport   = nullptr;
    _pluginPosixFdSupport = nullptr;
    _pluginThreadPool     = nullptr;
    _pluginPresetLoad     = nullptr;
    _pluginState          = nullptr;
    _pluginAudioPorts     = nullptr;
    _pluginParams         = nullptr;
    _pluginQuickControls  = nullptr;

    _pluginEntry->deinit();
    _pluginEntry = nullptr;
}

bool clapplugin::canActivate() const {
    checkForMainThread();
    if (isPluginActive())
        return false;
    if (_scheduleRestart)
        return false;
    return true;
}

void clapplugin::activate(sampleformat_t sampleFormat) {
    checkForMainThread();

    if (!_plugin || !canActivate())
        return;

    dawHandles->currentLatency = _pluginLatency ? _pluginLatency->get(_plugin) : 0;

    if (!_plugin->activate(_plugin, sampleFormat.sampleRate, sampleFormat.blockSize, sampleFormat.blockSize)) {
        setPluginState(InactiveWithError);
        return;
    }

    _scheduleProcess = true;
    setPluginState(ActiveAndSleeping);
}

void clapplugin::deactivate() {
    checkForMainThread();

    if (!isPluginActive())
        return;

    if (_state == ActiveAndProcessing) {
        dawHandles->bIsStopProcessing = true;
        _plugin->stop_processing(_plugin);
        setPluginState(ActiveAndReadyToDeactivate);
        dawHandles->bIsStopProcessing = false;
    } else if (_state == ActiveAndSleeping) {
        setPluginState(ActiveAndReadyToDeactivate);
    }

    _plugin->deactivate(_plugin);
    setPluginState(Inactive);
}


const char* clapplugin::getCurrentClapGuiApi() {
#if defined(__linux__)
    return CLAP_WINDOW_API_X11;
#elif defined(_WIN32)
    return CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
    return CLAP_WINDOW_API_COCOA;
#else
#error "unsupported platform"
#endif
}

static clap_window makeClapWindow(WId window) {
    clap_window w{};
#if defined(__linux__)
    w.api = CLAP_WINDOW_API_X11;
    w.x11 = window;
#elif defined(__APPLE__)
    w.api   = CLAP_WINDOW_API_COCOA;
    w.cocoa = reinterpret_cast<clap_nsview>(window);
#elif defined(_WIN32)
    w.api   = CLAP_WINDOW_API_WIN32;
    w.win32 = reinterpret_cast<clap_hwnd>(window);
#endif

    return w;
}

void clapplugin::setParentWindow(WId parentWindow) {
    checkForMainThread();

    dbgassert(_isGuiVisible == false);

    auto w = makeClapWindow(parentWindow);
    if (_isGuiFloating) {
        _pluginGui->set_transient(_plugin, &w);
        _pluginGui->suggest_title(_plugin, "using clap-host suggested title");
    } else {
        updateWindowSize();

        if (!_pluginGui->set_parent(_plugin, &w)) {
            log_lf(Log::L_WARN, "could embbed the plugin gui\n");
            _isGuiCreated = false;
            _pluginGui->destroy(_plugin);
            return;
        }
    }

    setPluginWindowVisibility(true);
    updateWindowSize();
}

void clapplugin::setPluginWindowVisibility(bool isVisible) {
    checkForMainThread();

    if (!_isGuiCreated)
        return;

    if (isVisible && !_isGuiVisible) {
        _pluginGui->show(_plugin);
        _isGuiVisible = true;
    } else if (!isVisible && _isGuiVisible) {
        _pluginGui->hide(_plugin);
        _isGuiVisible = false;
    }
}

void clapplugin::clapRequestCallback(const clap_host* host) {
    auto h                         = fromHost(host);
    h->_scheduleMainThreadCallback = true;
}

void clapplugin::clapRequestProcess(const clap_host* host) {
    auto h              = fromHost(host);
    h->_scheduleProcess = true;
}

void clapplugin::clapRequestRestart(const clap_host* host) {
    auto h              = fromHost(host);
    h->_scheduleRestart = true;
}

void clapplugin::clapLog(const clap_host* host, clap_log_severity severity, const char* msg) {
    switch (severity) {
        case CLAP_LOG_DEBUG:
            log_lf(Log::L_DEBUG, "%s\n", msg);
            break;

        case CLAP_LOG_INFO:
            log_lf(Log::L_INFO, "%s\n", msg);
            break;

        case CLAP_LOG_WARNING:
        case CLAP_LOG_ERROR:
        case CLAP_LOG_FATAL:
        case CLAP_LOG_HOST_MISBEHAVING:
            log_lf(Log::L_WARN, "%s\n", msg);
            break;
    }
}

template<typename T>
void clapplugin::initPluginExtension(const T*& ext, const char* id) {
    checkForMainThread();

    if (!ext)
        ext = static_cast<const T*>(_plugin->get_extension(_plugin, id));
}

const void* clapplugin::clapExtension(const clap_host* host, const char* extension) {
    checkForMainThread();

    auto h = fromHost(host);

    if (!strcmp(extension, CLAP_EXT_GUI))
        return &h->_hostGui;
    if (!strcmp(extension, CLAP_EXT_LOG))
        return &h->_hostLog;
    if (!strcmp(extension, CLAP_EXT_THREAD_CHECK))
        return &h->_hostThreadCheck;
    if (!strcmp(extension, CLAP_EXT_THREAD_POOL))
        return nullptr;
    //     return &h->_hostThreadPool;
    if (!strcmp(extension, CLAP_EXT_TIMER_SUPPORT))
        return &h->_hostTimerSupport;
    if (!strcmp(extension, CLAP_EXT_POSIX_FD_SUPPORT))
        return nullptr;
    //     return &h->_hostPosixFdSupport;
    if (!strcmp(extension, CLAP_EXT_PARAMS))
        return &h->_hostParams;
    if (!strcmp(extension, CLAP_EXT_QUICK_CONTROLS))
        return &h->_hostQuickControls;
    if (!strcmp(extension, CLAP_EXT_STATE))
        return &h->_hostState;
    return nullptr;
}

clapplugin* clapplugin::fromHost(const clap_host* host) {
    if (!host)
        throw std::invalid_argument("Passed a null host pointer");

    auto h = static_cast<clapplugin*>(host->host_data);
    if (!h)
        throw std::invalid_argument("Passed an invalid host pointer because the host_data is null");

    if (!h->_plugin)
        throw std::logic_error("The plugin can't query for extensions during the create method. Wait "
                               "for clap_plugin.init() call.");

    return h;
}

bool clapplugin::clapIsMainThread(const clap_host* host) {
    return seqthreads::CurrentThreadType() == seqthreads::ThreadType::MainThread;
}

bool clapplugin::clapIsAudioThread(const clap_host* host) {
    return fromHost(host)->getIsAudioTheadOverride();
}
bool clapplugin::getIsAudioTheadOverride() const {
    if (dawHandles->bIsStopProcessing)
        return true;
    return seqthreads::CurrentThreadType() == seqthreads::ThreadType::AudioThread;
}

void clapplugin::checkForMainThread() {
    if (seqthreads::CurrentThreadType() != seqthreads::ThreadType::MainThread) {
        dbgassert(0);
        throw std::logic_error("Requires Main Thread!");
    }
}

void clapplugin::checkForAudioThread() {
    if (seqthreads::CurrentThreadType() != seqthreads::ThreadType::AudioThread) {
        dbgassert(0);
        throw std::logic_error("Requires Audio Thread!");
    }
}

bool clapplugin::clapThreadPoolRequestExec(const clap_host* host, uint32_t num_tasks) {
    checkForAudioThread();

    auto h = fromHost(host);
    if (!h->_pluginThreadPool || !h->_pluginThreadPool->exec)
        throw std::logic_error("Called request_exec() without providing clap_plugin_thread_pool to "
                               "execute the job.");

    dbgassert(!h->_threadPoolStop);
    dbgassert(!h->_threadPool.empty());

    if (num_tasks == 0)
        return true;

    if (num_tasks == 1) {
        h->_pluginThreadPool->exec(h->_plugin, 0);
        return true;
    }

    h->_threadPoolTaskIndex = 0;
    h->_threadPoolSemaphoreProd.release(num_tasks);
    h->_threadPoolSemaphoreDone.acquire(num_tasks);
    return true;
}

bool clapplugin::clapRegisterTimer(const clap_host* host, uint32_t period_ms, clap_id* timer_id) {
    checkForMainThread();

    auto h = fromHost(host);
    h->initPluginExtensions();
    if (!h->_pluginTimerSupport || !h->_pluginTimerSupport->on_timer)
        throw std::logic_error(
                "Called register_timer() without providing clap_plugin_timer_support.on_timer() to "
                "receive the timer event.");

    auto id    = h->_nextTimerId++;
    *timer_id  = id;
    auto timer = std::make_unique<ClapHostPluginTimer>(id, period_ms);
    h->_timers.insert_or_assign(*timer_id, std::move(timer));
    return true;
}

bool clapplugin::clapUnregisterTimer(const clap_host* host, clap_id timer_id) {
    checkForMainThread();

    auto h = fromHost(host);
    if (!h->_pluginTimerSupport || !h->_pluginTimerSupport->on_timer)
        throw std::logic_error(
                "Called unregister_timer() without providing clap_plugin_timer_support.on_timer() to "
                "receive the timer event.");

    auto it = h->_timers.find(timer_id);
    if (it == h->_timers.end())
        throw std::logic_error("Called unregister_timer() for a timer_id that was not registered.");

    h->_timers.erase(it);
    return true;
}

class ClapHostSocketNotifier {
public:
    int fd    = -1;
    int flags = 0;
    const clap_plugin_posix_fd_support* pluginPosixFdSupport = nullptr;
    std::vector<const clap_plugin_t*> callbacks{};
    ClapHostSocketNotifier(int fd, int flags) : fd(fd), flags(flags) {
    }
    void setEnabled(bool enabled) {
    }
    void runCallbacks() {
        for (auto plugin : callbacks) {
            pluginPosixFdSupport->on_fd(plugin, fd, flags);
        }
    }
};

/* fd events */
struct clapplugin::Notifiers {
    std::unique_ptr<ClapHostSocketNotifier> rd;
    std::unique_ptr<ClapHostSocketNotifier> wr;
};

bool clapplugin::clapRegisterPosixFd(const clap_host* host, int fd, clap_posix_fd_flags_t flags) {
    checkForMainThread();

    auto h = fromHost(host);
    h->initPluginExtensions();
    if (!h->_pluginPosixFdSupport || !h->_pluginPosixFdSupport->on_fd)
        throw std::logic_error("Called register_fd() without providing clap_plugin_fd_support to "
                               "receive the fd event.");

    auto it = h->_fds.find(fd);
    if (it != h->_fds.end())
        throw std::logic_error(
                "Called register_fd() for a fd that was already registered, use modify_fd() instead.");

    h->_fds.insert_or_assign(fd, std::make_shared<Notifiers>());
    h->eventLoopSetFdNotifierFlags(fd, flags);
    return true;
}

bool clapplugin::clapModifyPosixFd(const clap_host* host, int fd, clap_posix_fd_flags_t flags) {
    checkForMainThread();

    auto h = fromHost(host);
    if (!h->_pluginPosixFdSupport || !h->_pluginPosixFdSupport->on_fd)
        throw std::logic_error("Called modify_fd() without providing clap_plugin_fd_support to "
                               "receive the fd event.");

    auto it = h->_fds.find(fd);
    if (it == h->_fds.end())
        throw std::logic_error(
                "Called modify_fd() for a fd that was not registered, use register_fd() instead.");

    h->_fds.insert_or_assign(fd, std::make_shared<Notifiers>());
    h->eventLoopSetFdNotifierFlags(fd, flags);
    return true;
}

bool clapplugin::clapUnregisterPosixFd(const clap_host* host, int fd) {
    checkForMainThread();

    auto h = fromHost(host);
    if (!h->_pluginPosixFdSupport || !h->_pluginPosixFdSupport->on_fd)
        throw std::logic_error("Called unregister_fd() without providing clap_plugin_fd_support to "
                               "receive the fd event.");

    auto it = h->_fds.find(fd);
    if (it == h->_fds.end())
        throw std::logic_error("Called unregister_fd() for a fd that was not registered.");

    h->_fds.erase(it);
    return true;
}

void clapplugin::eventLoopSetFdNotifierFlags(int fd, int flags) {
    checkForMainThread();

    auto it = _fds.find(fd);
    if (!assert_expr(it != _fds.end())) {
        return;
    }

    if (flags & CLAP_POSIX_FD_READ) {
        it->second->rd->callbacks.push_back(this->_plugin);
    } else if (it->second->rd) {
        it->second->rd->callbacks.erase(std::remove(it->second->rd->callbacks.begin(),
                                                    it->second->rd->callbacks.end(),
                                                    this->_plugin),
                                        it->second->rd->callbacks.end());
    }

    if (flags & CLAP_POSIX_FD_WRITE) {
        if (!it->second->wr) {
            it->second->wr->callbacks.push_back(this->_plugin);
        }
    } else if (it->second->rd) {
        it->second->wr->callbacks.erase(std::remove(it->second->wr->callbacks.begin(),
                                                    it->second->wr->callbacks.end(),
                                                    this->_plugin),
                                        it->second->wr->callbacks.end());
    }
}

void clapplugin::clapGuiResizeHintsChanged(const clap_host_t* host) {
    /* TODO */
    log_lf(Log::L_TRACE, "clapGuiResizeHintsChanged\n");
}

bool clapplugin::clapGuiRequestResize(const clap_host* host, uint32_t width, uint32_t height) {
    log_lf(Log::L_TRACE, "clapGuiRequestResize %d %d\n", width, height);
    /* TODO */
    return true;
}

bool clapplugin::clapGuiRequestShow(const clap_host* host) {
    log_lf(Log::L_TRACE, "clapGuiRequestShow\n");
    /* TODO */
    return true;
}

bool clapplugin::clapGuiRequestHide(const clap_host* host) {
    log_lf(Log::L_TRACE, "clapGuiRequestHide\n");
    /* TODO */
    return true;
}

void clapplugin::clapGuiClosed(const clap_host* host, bool wasDestroyed) { checkForMainThread(); }

void clapplugin::processBegin(int nframes) {
    _process.frames_count = nframes;
    _process.steady_time  = steady_time;
}

void clapplugin::processEnd(int nframes) {
    _process.frames_count = nframes;
    _process.steady_time  = steady_time;
    steady_time += nframes;
}

void clapplugin::processNoteOn(int sampleOffset, int channel, int key, int velocity) {
    checkForAudioThread();

    clap_event_note ev{};
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type     = CLAP_EVENT_NOTE_ON;
    ev.header.time     = sampleOffset;
    ev.header.flags    = 0;
    ev.header.size     = sizeof(ev);
    ev.port_index      = 0;
    ev.key             = int16_t(key);
    ev.channel         = int16_t(channel);
    ev.note_id         = -1;
    ev.velocity        = velocity / 127.0;

    pushInputEvent(&ev.header);
}

void clapplugin::processNoteOff(int sampleOffset, int channel, int key, int velocity) {
    checkForAudioThread();

    clap_event_note ev{};
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type     = CLAP_EVENT_NOTE_OFF;
    ev.header.time     = sampleOffset;
    ev.header.flags    = 0;
    ev.header.size     = sizeof(ev);
    ev.port_index      = 0;
    ev.key             = int16_t(key);
    ev.channel         = int16_t(channel);
    ev.note_id         = -1;
    ev.velocity        = velocity / 127.0;

    pushInputEvent(&ev.header);
}

void clapplugin::processNoteAt(int sampleOffset, int channel, int key, int pressure) {
    checkForAudioThread();
}

void clapplugin::processPitchBend(int sampleOffset, int channel, int value) {
    checkForAudioThread();
}

void clapplugin::processCC(int sampleOffset, int channel, int cc, int value) {
    checkForAudioThread();

    clap_event_midi ev{};
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type     = CLAP_EVENT_MIDI;
    ev.header.time     = sampleOffset;
    ev.header.flags    = 0;
    ev.header.size     = sizeof(ev);
    ev.port_index      = 0;
    ev.data[0]         = 0xB0 | channel;
    ev.data[1]         = cc;
    ev.data[2]         = value;

    pushInputEvent(&ev.header);
}

void clapplugin::processClapPlugin() {
    checkForAudioThread();

    if (!_plugin)
        return;

    // Can't process a plugin that is not active
    if (!isPluginActive())
        return;

    // Do we want to deactivate the plugin?
    if (_scheduleDeactivate) {
        _scheduleDeactivate = false;
        if (_state == ActiveAndProcessing)
            _plugin->stop_processing(_plugin);
        setPluginState(ActiveAndReadyToDeactivate);
        return;
    }

    // We can't process a plugin which failed to start processing
    if (_state == ActiveWithError)
        return;


    _evOut.clear();

    clap_event_header_t& transportHeader = dawHandles->transport.header;
    transportHeader = {};
    transportHeader.size = sizeof (clap_event_transport_t);
    transportHeader.space_id = CLAP_CORE_EVENT_SPACE_ID;
    transportHeader.type = CLAP_EVENT_TRANSPORT;

    if (isPluginSleeping()) {
        if (!_scheduleProcess && _eventListInput.empty())
            // The plugin is sleeping, there is no request to wake it up and there are no events to
            // process
            return;

        _scheduleProcess = false;
        if (!_plugin->start_processing(_plugin)) {
            // the plugin failed to start processing
            setPluginState(ActiveWithError);
            return;
        }

        setPluginState(ActiveAndProcessing);
    }

    int32_t status = CLAP_PROCESS_SLEEP;
    if (isPluginProcessing())
        status = _plugin->process(_plugin, &_process);

    (void) status;

    handlePluginOutputEvents();

    _evOut.clear();
    _eventListInput.clear();

    _engineToAppValueQueue.producerDone();

    // TODO: send plugin to sleep if possible
}

void clapplugin::generatePluginInputEvents() {
    _appToEngineValueQueue.consume(
            [this](clap_id param_id, const AppToEngineParamQueueValue& value) {
                clap_event_param_value ev{};
                ev.header.time     = 0;
                ev.header.type     = CLAP_EVENT_PARAM_VALUE;
                ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                ev.header.flags    = 0;
                ev.header.size     = sizeof(ev);
                ev.param_id        = param_id;
                ev.cookie          = value.cookie;
                ev.port_index      = 0;
                ev.key             = -1;
                ev.channel         = -1;
                ev.note_id         = -1;
                ev.value           = value.value;
                pushInputEvent(&ev.header);
            });

    _appToEngineModQueue.consume([this](clap_id param_id, const AppToEngineParamQueueValue& value) {
        clap_event_param_mod ev{};
        ev.header.time     = 0;
        ev.header.type     = CLAP_EVENT_PARAM_MOD;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.header.flags    = 0;
        ev.header.size     = sizeof(ev);
        ev.param_id        = param_id;
        ev.cookie          = value.cookie;
        ev.port_index      = 0;
        ev.key             = -1;
        ev.channel         = -1;
        ev.note_id         = -1;
        ev.amount          = value.value;
        pushInputEvent(&ev.header);
    });
}

void clapplugin::handlePluginOutputEvents() {
    for (uint32_t i = 0; i < _evOut.size(); ++i) {
        auto h = _evOut.get(i);
        switch (h->type) {
            case CLAP_EVENT_PARAM_GESTURE_BEGIN: {
                auto ev     = reinterpret_cast<const clap_event_param_gesture*>(h);
                bool& isAdj = _isAdjustingParameter[ev->param_id];

                if (isAdj)
                    throw std::logic_error("The plugin sent BEGIN_ADJUST twice");
                isAdj = true;

                EngineToAppParamQueueValue v;
                v.has_gesture = true;
                v.is_begin    = true;
                _engineToAppValueQueue.setOrUpdate(ev->param_id, v);
                break;
            }

            case CLAP_EVENT_PARAM_GESTURE_END: {
                auto ev     = reinterpret_cast<const clap_event_param_gesture*>(h);
                bool& isAdj = _isAdjustingParameter[ev->param_id];

                if (!isAdj)
                    throw std::logic_error("The plugin sent END_ADJUST without a preceding BEGIN_ADJUST");
                isAdj = false;
                EngineToAppParamQueueValue v;
                v.has_gesture = true;
                v.is_begin    = false;
                _engineToAppValueQueue.setOrUpdate(ev->param_id, v);
                break;
            }

            case CLAP_EVENT_PARAM_VALUE: {
                auto ev = reinterpret_cast<const clap_event_param_value*>(h);
                EngineToAppParamQueueValue v;
                v.has_value = true;
                v.value     = ev->value;
                _engineToAppValueQueue.setOrUpdate(ev->param_id, v);
                break;
            }
        }
    }
}

void clapplugin::paramFlushOnMainThread() {
    checkForMainThread();

    dbgassert(!isPluginActive());

    _scheduleParamFlush = false;

    _eventListInput.clear();
    _evOut.clear();

    generatePluginInputEvents();

    if (canUsePluginParams())
        _pluginParams->flush(_plugin, _eventListInput.clapInputEvents(), _evOut.clapOutputEvents());
    handlePluginOutputEvents();

    _evOut.clear();
    _engineToAppValueQueue.producerDone();
}

void clapplugin::updateClapFromMainThread() {
    checkForMainThread();
    if (!_timers.empty()) {
        for (auto& t : _timers) {
            if (t.second->update()) {
                _pluginTimerSupport->on_timer(_plugin, t.second->id);
            }
        }
    }

    // Try to send events to the audio engine
    _appToEngineValueQueue.producerDone();
    _appToEngineModQueue.producerDone();

    _engineToAppValueQueue.consume(
            [this](clap_id param_id, const EngineToAppParamQueueValue& value) {
                auto it = _params.find(param_id);
                if (it == _params.end()) {
                    log_lf(Log::L_WARN, HLOG "Plugin produced a CLAP_EVENT_PARAM_SET with an unknown param_id: %d\n", param_id);
                    return;
                }

                if (value.has_value)
                    it->second->setValue(value.value);

                if (value.has_gesture) {
                    it->second->setIsAdjusting(value.is_begin);
                }

                int32_t paramIdentifier = PARAM_OFFSET_EXTERNAL + it->first;
                auto param = effectbase::getParam(paramIdentifier);
                if (param)
                {
                    int32_t flags = FLG_PAR_UPDATE_FROM_CLIENT;
                    float valUnscaled = value.has_value ? FromPluginParam(it->second.get(), value.value) : param->getValue();
                    if (value.has_gesture)
                    {
                        if (value.is_begin) {
                            dawHandles->paramEditing = {paramIdentifier, valUnscaled};
                        } else {
                            auto oldVal = dawHandles->paramEditing.valBefore;
                            track_t* track = trackImpl->getTrack();
                            automatable_param_ref_t ref = toRef();
                            parameter_ref_t p  = { track->projectIdx, ref.type, projectGlobalId, paramIdentifier };
                            //TODO: move this into iHostCallback
                            auto daw = pluginMgr->getTls().dawInstance;
                            daw->pushHist(new action_modify_effect_parameter("Modify parameter", p, oldVal, valUnscaled));
                            dawHandles->paramEditing = {};
                            // flags |= FLG_PAR_UPDATE_FINISH;
                        }
                    }
                    setParamEdit(paramIdentifier, valUnscaled, flags);
                    param->paramDisplayValState = PARAM_FLAG_DIRTY;
                }
            });

    if (_scheduleParamFlush && !isPluginActive()) {
        paramFlushOnMainThread();
    }

    if (_scheduleMainThreadCallback) {
        _scheduleMainThreadCallback = false;
        _plugin->on_main_thread(_plugin);
    }

    if (_scheduleRestart) {
        deactivate();
        _scheduleRestart = false;
        activate(format);
    }
}

void clapplugin::setParamValueByHost(PluginParam& param, double value) {
    checkForMainThread();

    param.setValue(value);

    _appToEngineValueQueue.set(param.info().id, { param.info().cookie, value });
    _appToEngineValueQueue.producerDone();
    clapParamsRequestFlush(&host_);
}

void clapplugin::setParamModulationByHost(PluginParam& param, double value) {
    checkForMainThread();

    param.setModulation(value);

    _appToEngineModQueue.set(param.info().id, { param.info().cookie, value });
    _appToEngineModQueue.producerDone();
    clapParamsRequestFlush(&host_);
}

void clapplugin::scanParams() { clapParamsRescan(&host_, CLAP_PARAM_RESCAN_ALL); }

void clapplugin::clapParamsRescan(const clap_host* host, uint32_t flags) {
    checkForMainThread();
    auto plugin = fromHost(host);

    if (!plugin->canUsePluginParams())
        return;

    // 1. it is forbidden to use CLAP_PARAM_RESCAN_ALL if the plugin is active
    if (plugin->isPluginActive() && (flags & CLAP_PARAM_RESCAN_ALL)) {
        log_lf(Log::L_ERROR, HLOG "clap_host_params.recan(CLAP_PARAM_RESCAN_ALL) was called while the plugin is active!\n");
#ifndef NDEBUG
            dbgassert(0);
#endif // !NDEBUG
        return;
    }

    // 2. scan the params.
    auto count = plugin->_pluginParams->count(plugin->_plugin);
    std::unordered_set<clap_id> paramIds(count * 2ULL);

    for (uint32_t iPluginIndex = 0; iPluginIndex < count; ++iPluginIndex) {
        clap_param_info info{};
        if (!plugin->_pluginParams->get_info(plugin->_plugin, iPluginIndex, &info)) {
            log_lf(Log::L_WARN, HLOG "clap_plugin_params.get_info returned false for index %d\n", iPluginIndex);
            continue;
        }

        if (info.id == CLAP_INVALID_ID) {
            log_lf(Log::L_WARN, HLOG "clap_plugin_params.get_info reported a parameter with id = CLAP_INVALID_ID\n");
            log_lf(Log::L_WARN, HLOG " 2. name: %s, module: %s\n", info.name, info.module);
#ifndef NDEBUG
            dbgassert(0);
#endif // !NDEBUG
            continue;
        }

        auto it = plugin->_params.find(info.id);

        // check that the parameter is not declared twice
        if (paramIds.count(info.id) > 0) {
            dbgassert(it != plugin->_params.end());
            log_lf(Log::L_WARN, HLOG "the parameter with id: %d was declared twice.\n", info.id);
            log_lf(Log::L_WARN, HLOG " 1. name: %s, module: %s\n", it->second->info().name, it->second->info().module);
            log_lf(Log::L_WARN, HLOG " 2. name: %s, module: %s\n", info.name, info.module);
#ifndef NDEBUG
            dbgassert(0);
#endif // !NDEBUG
            continue;
        }
        paramIds.insert(info.id);

        if (it == plugin->_params.end()) {
#ifndef NDEBUG
            if (!(flags & CLAP_PARAM_RESCAN_ALL)) {
                log_lf(Log::L_WARN, HLOG "a new parameter was declared, but the flag CLAP_PARAM_RESCAN_ALL was not "
                        "specified; id: %d, name: %s, module: %s\n", info.id, info.name, info.module);
            }
#endif // !NDEBUG

            double value = plugin->getClapParamValue(info);
            auto param   = std::make_unique<PluginParam>(*plugin, info, value);
            if (!param->isValueValid(value)) {
                log_lf(Log::L_WARN, HLOG "invalud value %f for parameter. id: %d, name: %s, module: %s\n", value,
                        info.id, info.name, info.module);
            }
            plugin->_params.insert_or_assign(info.id, std::move(param));
        } else {
            // update param info
            if (!it->second->isInfoEqualTo(info)) {
#ifndef NDEBUG
                if (!clapParamsRescanMayInfoChange(flags)) {
                    log_lf(Log::L_WARN, HLOG "a parameter's info did change, but the flag CLAP_PARAM_RESCAN_INFO was not "
                            "specified; id: %d, name: %s, module: %s\n", info.id, info.name, info.module);
                }
                if (!(flags & CLAP_PARAM_RESCAN_ALL) && !it->second->isInfoCriticallyDifferentTo(info)) {
                    log_lf(Log::L_WARN, HLOG "a parameter's info has critical changes, but the flag CLAP_PARAM_RESCAN_ALL was not "
                            "specified; id: %d, name: %s, module: %s\n", info.id, info.name, info.module);
                }
#endif // !NDEBUG

                it->second->setInfo(info);
            }

            double value = plugin->getClapParamValue(info);
            if (it->second->value() != value) {
#ifndef NDEBUG
                if (!clapParamsRescanMayValueChange(flags)) {
                    log_lf(Log::L_WARN, HLOG "a parameter's value did change but, but the flag CLAP_PARAM_RESCAN_VALUES was not "
                            "specified; id: %d, name: %s, module: %s\n", info.id, info.name, info.module);
                }
#endif // !NDEBUG

                // update param value
                if (!it->second->isValueValid(value)) {
                    log_lf(Log::L_WARN, HLOG "invalud value %f for parameter. id: %d, name: %s, module: %s\n", value,
                            info.id, info.name, info.module);
                }
                it->second->setValue(value);
                it->second->setModulation(value);
                auto param = plugin->getParamUnchecked(it->first + PARAM_OFFSET_EXTERNAL);
                if (param) {
                    float valUnscaled = FromPluginParam(it->second.get(), value);
                    param->paramDisplayValState = PARAM_FLAG_DIRTY;
                    param->setValue(valUnscaled);
                }
            }
        }
    }

    // remove parameters which are gone
    for (auto it = plugin->_params.begin(); it != plugin->_params.end();) {
        if (paramIds.find(it->first) != paramIds.end())
            ++it;
        else {
#ifndef NDEBUG
            if (!(flags & CLAP_PARAM_RESCAN_ALL)) {
                auto& info = it->second->info();
                log_lf(Log::L_WARN, HLOG "a parameter was removed, but the flag CLAP_PARAM_RESCAN_ALL was not "
                        "specified; id: %d, name: %s, module: %s\n", info.id, info.name, info.module);
            }
#endif // !NDEBUG
            it = plugin->_params.erase(it);
        }
    }

    if (flags & CLAP_PARAM_RESCAN_ALL)
        plugin->paramsChanged();
}

void clapplugin::paramAdjusted(clap_id paramId) {
    int32_t paramIdentifier = PARAM_OFFSET_EXTERNAL + paramId;
    auto param              = effectbase::getParam(paramIdentifier);
    if (param) {
        param->paramDisplayValState = PARAM_FLAG_DIRTY;
        param->paramValueState      = PARAM_FLAG_DIRTY;
    }
}

void clapplugin::paramsChanged() {
    for (auto& [clapId, pParam] : _params) {
        auto paramId            = static_cast<int32_t>(clapId);
        int32_t paramIdentifier = PARAM_OFFSET_EXTERNAL + paramId;
        auto param              = effectbase::getParam(paramIdentifier);
        if (!param)
            param = registerParam(paramIdentifier);
        param->internalIdx          = paramId;
        param->paramDisplayValState = PARAM_FLAG_DIRTY;
        param->paramValueState      = PARAM_FLAG_DIRTY;
        auto& info                  = pParam->info();
        param->shortLabel           = info.name;
        auto paramrange             = info.max_value - info.min_value;
        if (paramrange != 0.0) {
            param->setInitial((info.default_value - info.min_value) / paramrange);
        }
        param->name = info.name;
        // param->name = info.module;
        // param->name += info.name;
    }
}

void clapplugin::postSetParameter(int32_t idx, float preVal, float val, int flags) {
    effectbase::postSetParameter(idx, preVal, val, flags);
    automatable_param_t* param = getParamUnchecked(idx);
    if (param->internalIdx >= 0) {
        auto& pParam = *_params[param->internalIdx].get();
        auto scaled  = ToPluginParam(&pParam, val);
        if (!(flags & FLG_PAR_UPDATE_MODULATED)) {
            setParamValueByHost(pParam, scaled);
        } else {
            setParamModulationByHost(pParam, scaled);
        }
        param->paramDisplayValState |= PARAM_FLAG_DIRTY;
        param->paramValueState = PARAM_FLAG_SET;
    }
}

param_unit_t clapplugin::convertParamValueToDisplay(int32_t idx, float value) {
    automatable_param_t* param = getParamUnchecked(idx);
    if (param->internalIdx >= 0) {
        if (!(param->paramDisplayValState & PARAM_FLAG_DIRTY))
            return { param->paramDisplayValStr, "" };
        auto& pParam = *_params[param->internalIdx].get();
        auto& info   = pParam.info();
        auto scaled  = info.min_value + value * (info.max_value - info.min_value);
        String data;
        data.resize(256);
        if (_pluginParams->value_to_text(_plugin, param->internalIdx, scaled, data.data(), data.size())) {
            param->paramDisplayValStr = data;
            return param_unit_t{ std::move(data), "" };
        }
    }
    return effectbase::convertParamValueToDisplay(idx, value);
}

void clapplugin::clapParamsClear(const clap_host* host,
                                 clap_id param_id,
                                 clap_param_clear_flags flags) {
    checkForMainThread();
}

void clapplugin::clapParamsRequestFlush(const clap_host* host) {
    auto self = fromHost(host);

    if (!self->isPluginActive() && clapIsMainThread(host)) {
        // Perform the flush immediately
        self->paramFlushOnMainThread();
        return;
    }

    self->_scheduleParamFlush = true;
    return;
}

double clapplugin::getClapParamValue(const clap_param_info& info) {
    checkForMainThread();

    if (!canUsePluginParams())
        return 0;

    double value = 0.0;
    if (_pluginParams->get_value(_plugin, info.id, &value))
        return value;

    log_lf(Log::L_ERROR, HLOG "Failed to get the param value, id: %d, name: %s, module: %s\n",
           info.id, info.name, info.module);
    dbgassert(0);
    return 0;
}

void clapplugin::scanQuickControls() {
    checkForMainThread();

    if (!_pluginQuickControls)
        return;

    if (!_pluginQuickControls->get || !_pluginQuickControls->count) {
        log_lf(Log::L_ERROR, HLOG "clap_plugin_quick_controls is partially implemented.\n");
        return;
    }

    quickControlsSetSelectedPage(CLAP_INVALID_ID);
    _quickControlsPages.clear();
    _quickControlsPagesIndex.clear();

    const auto N = _pluginQuickControls->count(_plugin);
    if (N == 0)
        return;

    _quickControlsPages.reserve(N);
    _quickControlsPagesIndex.reserve(N);

    clap_id firstPageId = CLAP_INVALID_ID;
    for (uint32_t iControlIndex = 0; iControlIndex < N; ++iControlIndex) {
        auto page = std::make_unique<clap_quick_controls_page>();
        if (!_pluginQuickControls->get(_plugin, iControlIndex, page.get())) {
            log_lf(Log::L_ERROR, HLOG "clap_plugin_quick_controls.get_page(%d) failed, while the page count is %d\n", iControlIndex, N);
            continue;
        }

        if (page->id == CLAP_INVALID_ID) {
            log_lf(Log::L_ERROR, HLOG "clap_plugin_quick_controls.get_page(%d) gave an invalid page_id\n", iControlIndex);
            continue;
        }

        if (iControlIndex == 0)
            firstPageId = page->id;

        auto it = _quickControlsPagesIndex.find(page->id);
        if (it != _quickControlsPagesIndex.end()) {
            log_lf(Log::L_ERROR, HLOG "clap_plugin_quick_controls.get_page(%d) gave twice the same page_id: %d\n", iControlIndex, page->id);
            log_lf(Log::L_ERROR, HLOG " 1. name: %s\n", it->second->name);
            log_lf(Log::L_ERROR, HLOG " 2. name: %s\n", page->name);
            continue;
        }

        _quickControlsPagesIndex.insert_or_assign(page->id, page.get());
        _quickControlsPages.emplace_back(std::move(page));
    }

    quickControlsPagesChanged();
    quickControlsSetSelectedPage(firstPageId);
}

void clapplugin::quickControlsSetSelectedPage(clap_id pageId) {
    checkForMainThread();
    if (pageId == _quickControlsSelectedPage)
        return;

    if (pageId != CLAP_INVALID_ID) {
        auto it = _quickControlsPagesIndex.find(pageId);
        if (it == _quickControlsPagesIndex.end()) {
            log_lf(Log::L_ERROR, HLOG "quick control page_id %d not found\n", pageId);
            dbgassert(0);
        }
    }

    _quickControlsSelectedPage = pageId;
    quickControlsSelectedPageChanged();
}

void clapplugin::setQuickControlsSelectedPageByHost(clap_id page_id) {
    checkForMainThread();
    dbgassert(page_id != CLAP_INVALID_ID);

    checkForMainThread();

    _quickControlsSelectedPage = page_id;
}

void clapplugin::clapQuickControlsChanged(const clap_host* host) {
    checkForMainThread();

    auto h = fromHost(host);
    if (!h->_pluginQuickControls) {
        log_lf(Log::L_ERROR, HLOG "Plugin called clap_host_quick_controls.changed() but does not provide "
                             "clap_plugin_quick_controls\n");
        dbgassert(0);
        return;
    }

    h->scanQuickControls();
}

bool clapplugin::loadNativePluginPreset(const std::string& path) {
    checkForMainThread();

    if (!_pluginPresetLoad)
        return false;

    if (!_pluginPresetLoad->from_file) {
        log_lf(Log::L_ERROR, HLOG "clap_plugin_preset_load does not implement load_from_file\n");
        return false;
    }

    return _pluginPresetLoad->from_file(_plugin, path.c_str());
}

void clapplugin::clapStateMarkDirty(const clap_host* host) {
    checkForMainThread();

    auto h = fromHost(host);

    if (!h->_pluginState || !h->_pluginState->save || !h->_pluginState->load) {
        
        log_lf(Log::L_ERROR, HLOG "Plugin called clap_host_state.set_dirty() but the host does not "
                               "provide a complete clap_plugin_state interface.\n");
        return;
    }

    h->_stateIsDirty = true;
}

void clapplugin::setPluginState(PluginState state) {
    switch (state) {
        case Inactive:
            dbgassert(_state == ActiveAndReadyToDeactivate);
            break;

        case InactiveWithError:
            dbgassert(_state == Inactive);
            break;

        case ActiveAndSleeping:
            dbgassert(_state == Inactive || _state == ActiveAndProcessing);
            break;

        case ActiveAndProcessing:
            dbgassert(_state == ActiveAndSleeping);
            break;

        case ActiveWithError:
            dbgassert(_state == ActiveAndProcessing);
            break;

        case ActiveAndReadyToDeactivate:
            dbgassert(_state == ActiveAndProcessing || _state == ActiveAndSleeping ||
                      _state == ActiveWithError);
            break;

        default:
            std::terminate();
    }

    _state = state;
}

bool clapplugin::isPluginActive() const {
    switch (_state) {
        case Inactive:
        case InactiveWithError:
            return false;
        default:
            return true;
    }
}

bool clapplugin::isPluginProcessing() const { return _state == ActiveAndProcessing; }

bool clapplugin::isPluginSleeping() const { return _state == ActiveAndSleeping; }

String clapplugin::paramValueToText(clap_id paramId, double value) {
    std::array<char, 256> buffer{};

    if (!canUsePluginParams())
        return "-";

    if (_pluginParams->value_to_text(_plugin, paramId, value, buffer.data(), buffer.size()))
        return buffer.data();

    return std::to_string(value);
}

bool clapplugin::canUsePluginParams() const noexcept {
    return _pluginParams && _pluginParams->count && _pluginParams->flush &&
           _pluginParams->get_info && _pluginParams->get_value && _pluginParams->text_to_value &&
           _pluginParams->value_to_text;
}

bool clapplugin::canUsePluginGui() const noexcept {
    return _pluginGui && _pluginGui->create && _pluginGui->destroy && _pluginGui->can_resize &&
           _pluginGui->get_size && _pluginGui->adjust_size && _pluginGui->set_size &&
           _pluginGui->set_scale && _pluginGui->hide && _pluginGui->show &&
           _pluginGui->suggest_title && _pluginGui->is_api_supported;
}

void clapplugin::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
    lastInputEvent = 0;
    processBegin(numSamples);
    processClapPlugin();
}

void clapplugin::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
    effectbase::postProcess(out, samples, hasProcessed);
}

void clapplugin::processMidiMessages(std::vector<IMidiMsg>& midiEvents) {
    /* NOTE: input events need to be inserted in chronological order */
    /* Process queued input events (t=0) */
    generatePluginInputEvents();
    /* Process midi events (t >= 0)*/
    for (auto& evt : midiEvents) {
    
        uint8_t eventType    = evt.mStatus >> 4;
        uint8_t channel      = evt.mStatus & 0xf;
        switch (eventType) {
            case IMidiMsg::EStatusMsg::kNoteOn:
                processNoteOn(evt.mOffset, channel, evt.mData1, evt.mData2);
                break;

            case IMidiMsg::EStatusMsg::kNoteOff:
                processNoteOff(evt.mOffset, channel, evt.mData1, evt.mData2);
                break;
            default:{
                    clap_event_midi midiEvent{};
                    midiEvent.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                    midiEvent.header.type     = CLAP_EVENT_MIDI;
                    midiEvent.header.time     = evt.mOffset;
                    midiEvent.header.flags    = 0;
                    midiEvent.header.size     = sizeof(midiEvent);
                    midiEvent.port_index      = 0;
                    midiEvent.data[0]         = evt.mStatus;
                    midiEvent.data[1]         = evt.mData1;
                    midiEvent.data[2]         = evt.mData2;
                    // Don't push note events as midi
                    pushInputEvent(&midiEvent.header);
                }
                break;
        }
    }
}

void clapplugin::sendNotesOff() {
    effectbase::sendNotesOff();
}

samplecount_t clapplugin::getPluginLatency() {
    return dawHandles->currentLatency;
}

void clapplugin::onDisable() { deactivate(); }

void clapplugin::onEnable() { activate(format); }

void clapplugin::unload(DAW::Host::PluginManager* host, int flags) {
    deactivate();
    unloadClapPlugin();
    effectbase::unload(host, flags);
}

void clapplugin::load(DAW::Host::PluginManager* host) {
    effectbase::load(host);
    activate(format);
}

void clapplugin::configureIOPorts() {
    auto inputCount = channelnum_t(0);
    auto outputCount = channelnum_t(0);
    inputChannelsDesc.clear();
    outputChannelsDesc.clear();
    if (!isPluginActive() && _pluginAudioPorts) {
        inputCount  = static_cast<channelnum_t>(math::clamp(_pluginAudioPorts->count(_plugin, true), 0U, 255U));
        outputCount = static_cast<channelnum_t>(math::clamp(_pluginAudioPorts->count(_plugin, false), 0U, 255U));
        channelnum_t portOffsetInput = 0;
        for (channelnum_t i = 0; i < inputCount; ++i) {
            clap_audio_port_info_t info{};
            if (_pluginAudioPorts->get(_plugin, i, true, &info)) {
                DAW::channel_desc desc;
                desc.offset = portOffsetInput;
                desc.count = info.channel_count;
                desc.name = info.name;
                inputChannelsDesc.push_back(desc);
                portOffsetInput += desc.count;
            }
        }
        channelnum_t portOffsetOutput = 0;
        for (channelnum_t i = 0; i < outputCount; ++i) {
            clap_audio_port_info_t info{};
            if (_pluginAudioPorts->get(_plugin, i, false, &info)) {
                DAW::channel_desc desc;
                desc.offset = portOffsetOutput;
                desc.count = info.channel_count;
                desc.name = info.name;
                outputChannelsDesc.push_back(desc);
                portOffsetOutput += desc.count;
            }
        }
    }
}

void clapplugin::initBuffers() {
    configureIOPorts();
    effectbase::initBuffers();
    dawHandles->clapInputBuffers.resize(inputChannelsDesc.size());
    dawHandles->clapOutputBuffers.resize(outputChannelsDesc.size());
    // dawHandles->clapOutputBuffers[0].channel_count = 2;
    // dawHandles->clapOutputBuffers[0].data32 = blockOutputs->buf;
    dawHandles->dawInputBuffers.resize(inputChannelsDesc.size());
    dawHandles->dawOutputBuffers.resize(outputChannelsDesc.size());

    for (size_t i = 0; i < inputChannelsDesc.size(); ++i) {
        auto& ch = inputChannelsDesc[i];
        auto& dawBlock = dawHandles->dawInputBuffers[i];
        dawBlock = blockInputs->SubChannelsBlock(ch.offset, ch.count);
        auto& clapBuffer = dawHandles->clapInputBuffers[i];
        clapBuffer = {};
        clapBuffer.channel_count = dawBlock.channels;
        clapBuffer.data32        = dawBlock.buf;
    }
    for (size_t i = 0; i < outputChannelsDesc.size(); ++i) {
        auto& ch = outputChannelsDesc[i];
        auto& dawBlock = dawHandles->dawOutputBuffers[i];
        dawBlock = blockOutputs->SubChannelsBlock(ch.offset, ch.count);
        auto& clapBuffer = dawHandles->clapOutputBuffers[i];
        clapBuffer = {};
        clapBuffer.channel_count = dawBlock.channels;
        clapBuffer.data32        = dawBlock.buf;
    }

    _process.audio_inputs        = dawHandles->clapInputBuffers.data();
    _process.audio_inputs_count  = dawHandles->clapInputBuffers.size();
    _process.audio_outputs       = dawHandles->clapOutputBuffers.data();
    _process.audio_outputs_count = dawHandles->clapOutputBuffers.size();

}

void clapplugin::updateFromMainThread() {
    effectbase::updateFromMainThread();
    updateClapFromMainThread();
}

bool clapplugin::hasWindowEditor() {
    return canUsePluginGui();
}

bool clapplugin::showWindow(bool bResetPosition) {
    checkForMainThread();

    if (!canUsePluginGui())
        return false;

    if (_isGuiCreated) {
        _pluginGui->destroy(_plugin);
        _isGuiCreated = false;
        _isGuiVisible = false;
    }

    _guiApi = getCurrentClapGuiApi();

    _isGuiFloating = false;
    if (!_pluginGui->is_api_supported(_plugin, _guiApi, false)) {
        if (!_pluginGui->is_api_supported(_plugin, _guiApi, true)) {
            log_lf(Log::L_WARN, "could find a suitable gui api\n");
            return false;
        }
        _isGuiFloating = true;
    }

    if (!_pluginGui->create(_plugin, _guiApi, _isGuiFloating)) {
        log_lf(Log::L_WARN, "could not create the plugin gui\n");
        return false;
    }
    bSupportsWindowResize = _pluginGui->can_resize(_plugin);

    _isGuiCreated = true;

    uint32_t width  = 320;
    uint32_t height = 240;
    // if (!_pluginGui->get_size(_plugin, &width, &height)) {
    //     log_lf(Log::L_WARN, "could not get the size of the plugin gui\n");
    //     _isGuiCreated = false;
    //     _pluginGui->destroy(_plugin);
    //     return false;
    // }
    this->openWindow(bResetPosition, { width, height });
    return true;
}

bool clapplugin::onClose() {
    if (this->windowHost != nullptr && bEditOpen) {
        // this->dispatch(effEditClose);
    }
    bEditOpen = false;
    return true;
}

ivec2 clapplugin::constrainWindowSize(host_plugin_window* window, ivec2 size) {
    if (!bSupportsWindowResize) {
        // ERect* prc = nullptr;
        // this->dispatch(effEditGetRect, 0, 0, (void*) &prc);
        // if (prc) {
        //     if (size.x > (prc->right - prc->left)) {
        //         size.x = prc->right - prc->left;
        //     }
        //     if (size.y > (prc->bottom - prc->top)) {
        //         size.y = prc->bottom - prc->top;
        //     }
        // }
    }
    return size;
}

void clapplugin::onWindowResize(ivec2 size) {
    // axEffect->onWindowResize(size);
}

bool clapplugin::onShow(host_plugin_window* _window) {
    if (this->windowHost == _window) {
        bEditOpen = true;
        setParentWindow(_window->getHWND());
        this->updateFromMainThread();
    }
    return true;
}

void clapplugin::updateWindowSize() {
    uint32_t width  = 0;
    uint32_t height = 0;

    if (!_pluginGui->get_size(_plugin, &width, &height)) {
        log_lf(Log::L_WARN, HLOG "pluginGui->get_size returned false\n");
        return;
    }
    windowHost->resize({ width, height });
}

automatable_param_t* clapplugin::getParam(int32_t idx) {
    auto param = effectbase::getParam(idx);
    if (param && param->internalIdx >= 0) {
        if (param->paramValueState & PARAM_FLAG_DIRTY) {
            // param->setValue(_params[param->internalIdx]->value());
            auto& info        = _params[param->internalIdx]->info();
            param->shortLabel = info.name;
            auto paramrange   = info.max_value - info.min_value;
            if (paramrange != 0.0) {
                param->setValue((_params[param->internalIdx]->value() - info.min_value) / paramrange);
            } else {
                param->setValue(_params[param->internalIdx]->value());
            }
            param->paramValueState = PARAM_FLAG_SET;
        }
    }
    return param;
}

void clapplugin::pushInputEvent(clap_event_header_t* ev) {
    dbgassert(ev->time >= lastInputEvent);
    _eventListInput.push(ev);
    lastInputEvent = ev->time;
}
