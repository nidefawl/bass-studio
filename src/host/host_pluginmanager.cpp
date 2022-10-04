#include "host/host_pluginmanager.h"
#include "logging.h"
#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"
#include "track_impl.h"

#ifdef _WIN32
#include <windows.h>
String getModuleName(HMODULE);
#elif defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace DAW::Host {

class PluginManager::ModuleManager {
public:
    ModuleManager() = default;

    void releaseModule(void* module) {
#ifdef _WIN32
        String moduleName = getModuleName((HMODULE)module);
        log_printf("Unload %s\n", StringAsCStr(moduleName));
        FreeLibrary((HMODULE)module);
#endif
#if defined(__linux__) || defined(__APPLE__)
        dlclose(module);
#endif
    }
};


PluginManager::PluginManager() noexcept
    : mgrImpl(new PluginManager::pluginmanager_impl()), moduleMgr{new PluginManager::ModuleManager{}} 
{
    registerModules();
}

PluginManager::~PluginManager() {
    delete moduleMgr;
}

void PluginManager::updateSampleFormat(const sampleformat_t& _sampleFormat) {
    for (auto* audio : this->allAudioStages) {
        audio->sampleFormat = _sampleFormat;
        audio->input.realloc(_sampleFormat.blockSize);
        audio->output.realloc(_sampleFormat.blockSize);
        audio->outputPost.realloc(_sampleFormat.blockSize);
    }
    for (effectbase* plugin : this->pluginInstances) {
        plugin->setSampleFormat(_sampleFormat);
        plugin->initBuffers();
        plugin->initMeters();
    }
    for (effectbase* plugin : this->pluginsDeferred) {
        plugin->setSampleFormat(_sampleFormat);
        plugin->initBuffers();
        plugin->initMeters();
    }
    for (vstplugin* plugin : this->pluginInstancesVST2) {
        plugin->onDisable();
        plugin->dispatch(effSetBlockSize, 0, _sampleFormat.blockSize);
        plugin->dispatch(effSetSampleRate, 0, 0, nullptr, (float) _sampleFormat.sampleRate);
        plugin->onEnable();
    }
    for (auto* stage : this->allAudioStages) {
        stage->pluginsChanged();
    }
}

void PluginHostCallback::onUiChanged(effectbase* effect) {
    if (effect) {
        // NOTE: this loop might kill performance
        effect->visitParams([](auto& mapEntry) {
            automatable_param_t& param = mapEntry.second;
            param.paramValueState |= plugin_param_sync_state::PARAM_FLAG_DIRTY;
            param.paramDisplayValState |= PARAM_FLAG_DIRTY;
        });
    }
}

void PluginManager::setTls(daw_tls::tlsinstance& tls) {
    this->mgrImpl->tls = tls;
}

void PluginManager::unloadPlugin(effectbase* plugin, int flags) {
    bool notifyUp = !(flags & FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
    if (notifyUp) {
        //TODO: this shouldn't be here!
        if (MainCtrl::get())
            MainCtrl::get()->closeContextMenu();
    }

    plugin->onPreUnload(flags);
    audio_stage_t* audioStage = plugin->getTrackLink();
    if (audioStage) {
        audioStage->removePlugin(plugin, false);
        if (notifyUp) {
            audioStage->pluginsChanged();

        }
    }

    if (notifyUp) {
        plugin->closeWindow();
    }
    plugin->unload(this, flags);

    switch (plugin->getModuleType()) {
    case PLUGIN_TYPE_DEFERRED:
        always_assert(removeEntry(pluginsDeferred, plugin));
        break;
    case PLUGIN_TYPE_INTERNAL_EFFECT:
    case PLUGIN_TYPE_VST:
        always_assert(removeEntry(pluginInstancesVST2, plugin));
        always_assert(removeEntry(pluginInstances, plugin));
        break;
    default:
        always_assert(removeEntry(pluginInstancesInternal, plugin));
        always_assert(removeEntry(pluginInstances, plugin));
        break;
    }

    //PopupCtrl::get()->close(); // Make sure context controls do not reference vst
    if (plugin->getModuleType() == PLUGIN_TYPE_VST || plugin->getModuleType() == PLUGIN_TYPE_INTERNAL_EFFECT) {
        vstplugin* vst = dynamic_cast<vstplugin*>(plugin);
        if (vst->internalModuleId <= 0) {
            moduleMgr->releaseModule(vst->handle->hmodule);
        }
    }
    delete plugin;
    if (notifyUp) {
        if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    }
    dbgassert(validateIds());
}

void PluginManager::updatePluginWindows() {
    for (auto* plugin : pluginInstances) {
        //plugin->dispatch(effEditIdle);
        plugin->updateWindow();
    }
}

IHostCallback* PluginManager::getHostCallback() {
    return pluginHostCallback.get();
}

void PluginManager::releaseProjectResources() {
#if DAW_DEBUG_AUDIOGRAPH
    lastProcessingList = nullptr;
    lastTrackGraph = nullptr;
    //lastProcessingGraphs.clear();
#endif
    dbgassert(pluginInstances.empty());
    dbgassert(pluginInstancesVST2.empty());
    dbgassert(pluginInstancesInternal.empty());
    dbgassert(allAudioStages.empty());
    dbgassert(trackAudioStages.empty());
}

vstplugin* PluginManager::getPlugin(AEffect* aeffect) {
    if (aeffect && aeffect->user) {
        return static_cast<vstplugin*>(aeffect->user);
    }
    //for (auto* current : pluginInstancesVST2) {
    //    if (current->handle->aeffect == aeffect)
    //        return current;
    //}
    return nullptr;
}

effectbase* PluginManager::getPluginById(int32_t projectGlobalId, bool activeOnly) const {
    auto it = std::find_if(pluginInstances.begin(), pluginInstances.end(),
        [projectGlobalId, activeOnly] (const effectbase* ptr) {
            return ptr->projectGlobalId == projectGlobalId && (!activeOnly || ptr->hasTrackLink());
        });
    if (it != pluginInstances.end()) {
        return *it;
    }
    it = std::find_if(pluginsDeferred.begin(), pluginsDeferred.end(),
        [projectGlobalId, activeOnly](const effectbase* ptr) {
            if (activeOnly && !ptr->hasTrackLink())
                return false;
            if (ptr->projectGlobalId == projectGlobalId)
                return true;
            auto plugDeferred = dynamic_cast<const effect_deferred*>(ptr);
            return plugDeferred->getSnapshotConst().projectGlobalId == projectGlobalId;
        });
    if (it != pluginsDeferred.end()) {
        return *it;
    }
    return nullptr;
}

bool PluginManager::addDeferredEffect(effectbase* plugin) {
    plugin->projectGlobalId = getNextGlobalModuleId(plugin->projectGlobalId);
    while (getPluginById(plugin->projectGlobalId, false) != nullptr) {
        plugin->projectGlobalId = getNextGlobalModuleId(0);
    }
    auto it = std::find_if(pluginsDeferred.begin(), pluginsDeferred.end(), [plugin](auto* eff) { return eff->projectGlobalId == plugin->projectGlobalId; });
    if (it != pluginsDeferred.end()) {
        log_lf(Log::L_ERROR, "Duplicate plugin id %d", plugin->projectGlobalId);
        return false;
    }
    pluginsDeferred.push_back(plugin);
    return true;
}

void PluginManager::unloadTrack(track_t* track) {
    dbgassert(track->audio);
    auto audio = track->audio;
    std::vector<effectbase*> effects = audio->effects; // make a copy before unloading plugins
    for (effectbase* effect : effects) {
        unloadPlugin(effect, FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
    }
    dbgassert(audio->deferredEffects.empty());
}

void PluginManager::removePlugin(effectbase* plugin) {
    audio_stage_t* audioStage = plugin->getTrackLink();
    audioStage->removePlugin(plugin, true);
    audioStage->pluginsChanged();
    if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    onTrackLayoutChange();
}

void PluginManager::getAllInstances(std::vector<effectbase*>& effects) {
    effects = pluginInstances;
}

void PluginManager::createAudio(track_t* track) {
    auto audio = new track_impl_t(this,
                                  getNextGlobalAudioStageId(0),
                                  track,
                                  pluginHostCallback->m_sampleFormatInternal,
                                  DAW::Host::DEFAULT_CHANNEL_COUNT);
    allAudioStages.push_back(audio);
    trackAudioStages.push_back(audio);
    track->audio = audio;
}

void PluginManager::releaseAudio(track_t* track) {
    auto audioStage = track->audio;
    dbgassert(audioStage);
    dbgassert(audioStage->effects.empty());
    track->audio = nullptr;
    auto it = std::find(allAudioStages.begin(), allAudioStages.end(), audioStage);
    dbgassert(it != allAudioStages.end());
    allAudioStages.erase(it);
    auto it2 = std::find(trackAudioStages.begin(), trackAudioStages.end(), audioStage);
    dbgassert(it2 != trackAudioStages.end());
    trackAudioStages.erase(it2);
    delete audioStage;
}

audio_stage_t* PluginManager::createAudioStage() {
    auto audio = new audio_stage_t(this,
                                   getNextGlobalAudioStageId(0),
                                   pluginHostCallback->m_sampleFormatInternal,
                                   DAW::Host::DEFAULT_CHANNEL_COUNT);
    allAudioStages.push_back(audio);
    return audio;
}

void PluginManager::releaseAudioStage(audio_stage_t* audioStage) {
    auto it = std::find(allAudioStages.begin(), allAudioStages.end(), audioStage);
    dbgassert(it != allAudioStages.end());
    allAudioStages.erase(it);
    delete audioStage;
}

audio_stage_t* PluginManager::getAudioStage(const audio_stage_ref_t& ref) const {
    if (ref.stageId == TRACKID_INVALID_I32)
        return nullptr;
    dbgassert((int32_t)ref.stageId > -1);
    auto it = std::find_if(allAudioStages.begin(), allAudioStages.end(), [ref] (const audio_stage_t* ptr) {
        return audioStageIdMatches(ptr->stageId, ref.stageId);
    });
    //dbgassert(it != allAudioStages.end());
    if (it != allAudioStages.end()) {
        return *it;
    }
    log_printf("null audio stage for %d\n", static_cast<int32_t>(ref.stageId));
    return nullptr;
}

bool PluginManager::movePlugins(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
    ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    dbgassert(dstTr);
    dbgassert(trp);
    dbgassert(src < (int)trp->effects.size());
    dbgassert(src+len <= (int)trp->effects.size());
    dbgassert(dst-1 <= (int)dstTr->effects.size());
    std::vector<effectbase*> tmpEffects = trp->effects;
    for (int32_t i = 0; i < len; i++) {
        effectbase* tmpPlugin = tmpEffects[src + i];
        trp->removePlugin(tmpPlugin, true);
        dstTr->insertEffect(dst+i, tmpPlugin);
    }
    trp->pluginsChanged();
    dstTr->pluginsChanged();
    onTrackLayoutChange();
    if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    return true;
}

bool PluginManager::moveEffects(audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
    ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
#ifndef NDEBUG
    dbgassert(src >= 0 && dst >= 0);
    dbgassert(src != dst);
    dbgassert((int32_t)trp->effects.size() > src);
    dbgassert((int32_t)trp->effects.size() > dst);
    for (effectbase* effect : trp->effects) {
        dbgassert(effect->getSlot()>=0);
    }
#endif // NDEBUG

    //shift element
    std::vector<effectbase*> curEffects = trp->effects;
    std::vector<effectbase*> tmpEffects;
    tmpEffects.resize(trp->effects.size());
    auto itIn = curEffects.cbegin();
    auto itOut = tmpEffects.begin();
    int32_t src2 = src;
    int32_t dst2 = dst;
    int32_t end = dst+len;
    for (;itOut!=tmpEffects.cend();) {
        if (curEffects.cbegin()+src == itIn) {
            itIn+=len;
        }
        if (dst2 < end && tmpEffects.cbegin()+dst2 == itOut) {
            *itOut++ = curEffects[src2++];
            dst2++;
        } else {
            *itOut++ = *itIn++;
        }
    }
    trp->effects = std::move(tmpEffects);
    int slot = 0;
    for (effectbase* effect : trp->effects) {
        effect->setSlot(slot++);
    }
    onTrackLayoutChange();
    return true;
}

bool PluginManager::replacePlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst, effectbase** prevPlugin) {
    bool retVal = trp->replaceEffect(dst, plugin, prevPlugin);
    onTrackLayoutChange();
    return retVal;
}

bool PluginManager::insertNewPlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst) {
    //if (plugin->isSynth) {
    //    vstplugin* old = trp->setInstrument(plugin);
    //    if (old) {
    //        unloadPlugin(old);
    //    }
    //} else {
        trp->insertEffect(dst, plugin);
    //}
    return true;
}

void PluginManager::onPluginsChanged(audio_stage_t* stage) {
    log_printf("Plugins changed on audio stage %d\n", static_cast<int32_t>(stage->stageId.stageId));
    dbgassert(validateIds());
}
void PluginManager::onTick() {
    // Currently no lock
    //ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    for (auto* current : pluginInstances) {
        //TODO: should we skip dispatching if current->bWantsEffIdle == false ?!
        if (current->bEditOpen && !current->bInEditIdle) {
            current->bInEditIdle = true;
            current->bInEditIdle = false;
            if (current->windowHost) {
                //current->window->captureWindowFrame();
                current->updateWindow();
            }
        }
    }
    checkScanner();
}

bool PluginManager::postPluginLoaded(audio_stage_t* trp, effectbase* plugin) {
    trp->pluginsChanged();
    onTrackLayoutChange();
    if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    dbgassert(validateIds());
    return true;
}

int32_t PluginManager::getNextGlobalModuleId(int32_t globalId)
{
    if (globalId <= 0) {
        globalId = ++pluginId;
    } else if (globalId < (1 << 16)) {
        globalId += (1 << 16);
    }

    update_maximum(pluginId, globalId);
    return globalId;
}

audio_stage_id_t PluginManager::getNextGlobalAudioStageId(int32_t globalId) {
    audio_stage_id_t stageId{};
    audiostageid_i32* stageIds[4] = {&stageId.stageId, &stageId.inputStageId, &stageId.outputStageId, &stageId.outputPostStageId };
    auto startId = globalId;
    if (globalId <= 0) {
        startId = ++audioStageId;
    }
    for (audiostageid_i32* id : stageIds) {
        *id = static_cast<audiostageid_i32>(startId++);
    }
    update_maximum(audioStageId, startId);
    return stageId;
}

bool PluginManager::isStageIdInUse(track_id_snapshot_t stageId) {
    if (stageId.stageId == -1) {
        return false;
    }
    for (auto* id : {&stageId.stageId, &stageId.inputStageId, &stageId.outputStageId, &stageId.outputPostStageId }) {
        if (static_cast<int32_t>(*id) <= audioStageId)
            return true;
    }
    return false;
}

void PluginManager::updateMaximumStageId() {
    int32_t maximumStageId = 0;
    for (auto* stage : allAudioStages) {
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.stageId));
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.inputStageId));
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.outputStageId));
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.outputPostStageId));
    }
    this->audioStageId = maximumStageId;
}

int32_t PluginManager::getNextSampleId(int32_t id) {
    if (id <= 0) {
        return ++sampleId;
    }
    update_maximum(sampleId, id);
    return id;
}

int32_t PluginManager::validateIds()
{
#ifndef NDEBUG
    /** check for double usage of stageIds across all audiostages */
    for (auto stage : allAudioStages) {
        audiostageid_i32* stageIds[4] = {&stage->stageId.stageId, &stage->stageId.inputStageId, &stage->stageId.outputStageId,
                                         &stage->stageId.outputPostStageId};
        for (auto* pStageId : stageIds) {
            for (auto* pStageId2 : stageIds) {
                if (pStageId2 == pStageId) {
                    always_assert(static_cast<int32_t>(*pStageId) == static_cast<int32_t>(*pStageId2));
                    continue;
                }
                always_assert(static_cast<int32_t>(*pStageId) != static_cast<int32_t>(*pStageId2));
            }
        }
        for (auto stage2 : allAudioStages) {
            if (stage2 == stage)
                continue;
            audiostageid_i32* stageIds2[4] = {&stage2->stageId.stageId, &stage2->stageId.inputStageId, &stage2->stageId.outputStageId,
                                              &stage2->stageId.outputPostStageId};
            for (auto* pStageId : stageIds) {
                for (auto* pStageId2 : stageIds2) {
                    always_assert(static_cast<int32_t>(*pStageId) != static_cast<int32_t>(*pStageId2));
                }
            }
        }
    }
    /** check for collisions of plugin ids between deferred and normal effect instances */
    for (auto plugin : pluginInstances) {
        auto id = plugin->projectGlobalId;
        int32_t count = 0;
        for (auto plugin2 : pluginsDeferred) {
            if (plugin == plugin2)
                continue;
            auto id2 = plugin2->projectGlobalId;
            if (id == id2) {
                count++;
            }
        }
        always_assert(count <= 1);
        for (auto plugin2 : pluginInstances) {
            if (plugin == plugin2)
                continue;
            auto id2 = plugin2->projectGlobalId;
            always_assert(id2 != id);
        }
    }

    for (auto plugin : pluginsDeferred) {
        auto id = plugin->projectGlobalId;
        for (auto stage : allAudioStages) {
            auto stageIds = {&stage->stageId.stageId, &stage->stageId.inputStageId, &stage->stageId.outputStageId,
                                             &stage->stageId.outputPostStageId};
            for (auto* pStageId : stageIds) {
                always_assert(static_cast<int32_t>(*pStageId) != id);
            }

        }
    }

    for (auto plugin : pluginInstances) {
        auto id = plugin->projectGlobalId;
        for (auto stage : allAudioStages) {
            std::array<audiostageid_i32*,4> stageIds = {&stage->stageId.stageId, &stage->stageId.inputStageId, &stage->stageId.outputStageId,
                                             &stage->stageId.outputPostStageId};
            for (auto* pStageId : stageIds) {
                always_assert(static_cast<int32_t>(*pStageId) != id);
            }
        }
    }
#endif // NDEBUG
    return 1;
}

#ifdef _WIN32
HMODULE safeLoadLib(const char* szLibName);
int32_t loadLib(String filepath, VSTPluginMain_t** out_fn, HMODULE* out_hmodule) {
    if (!FileExists(filepath)) {
        return -2;
    }
    HMODULE hmodule = safeLoadLib(StringAsCStr(filepath));
    if (!hmodule) {
        return -3;
    }

    auto fn = reinterpret_cast<VSTPluginMain_t*>(GetProcAddress(hmodule, "VSTPluginMain"));
    if (!fn) fn = reinterpret_cast<VSTPluginMain_t*>(GetProcAddress(hmodule, "main"));

    if (!fn)
    {
        FreeLibrary(hmodule);
        return -4;
    }

    *out_hmodule = hmodule;
    *out_fn = fn;

    return 0;
}

#define CLOSE_MODULE_HANDLE(handle) FreeLibrary(handle)

#endif

#if defined(__APPLE__)

int32_t loadLib(String filepath, VSTPluginMain_t** out_fn, void** out_hmodule);

#define CLOSE_MODULE_HANDLE(handle) dlclose(handle)

#endif

#if defined(__linux__)
int32_t loadLib(String filepath, VSTPluginMain_t** out_fn, void** out_hmodule) {
    if (!FileExists(filepath)) {
        return -2;
    }
    void* module = dlopen(StringAsCStr(filepath), RTLD_NOW);
    if (!module) {
        auto dl_err = dlerror();
        if (dl_err) {
            log_lf(Log::L_ERROR, "dlopen failed: %s\n", dl_err);
        }
        return -3;
    }

    VSTPluginMain_t* fn = (VSTPluginMain_t*)dlsym(module, "VSTPluginMain");
    if (fn == NULL)
    {
        fn = (VSTPluginMain_t*)dlsym(module, "main");
    }
    if (fn == NULL)
    {
        dlclose(module);
        return -4;
    }
    *out_hmodule = module;
    *out_fn = fn;

    return 0;
}

#define CLOSE_MODULE_HANDLE(handle) dlclose(handle)

#endif
#ifdef _WIN32
int loadPlugin_jbridge(audioMasterCallback audiomasterCallback, const String& filepath, HMODULE* hmodule, AEffect** aeffect, uint64_t bugfixFlags);
#endif //_WIN32

LoadResultVST2Plugin PluginManager::loadPlugin(String filepath, uint32_t uId, int32_t globalId, uint64_t bugfixFlags) {
    dbgassert(masterCallBackSlot);

    String path, name, nameWithoutExt;
    SplitPath(filepath, &path, &nameWithoutExt, nullptr, &name);

    VSTPluginMain_t* fn = nullptr;
    void* moduleHandle = nullptr;
    AEffect* aeffect = nullptr;

#ifdef _WIN32
    HMODULE hmodule = nullptr;
#else
    void* hmodule = nullptr;
#endif //_WIN32

    int32_t ret = loadLib(filepath, &fn, &hmodule);
    moduleHandle = hmodule;

    if (uId != 0) {
        pluginHostCallback->vstShellCurrentUniqueId = static_cast<VstInt32>(uId);
    } else {
        pluginHostCallback->vstShellCurrentUniqueId = static_cast<VstInt32>(0);
    }

    if (ret == 0) {
        aeffect = fn(masterCallBackSlot);
#ifdef _WIN32
    } else if (ret == -3) {
        ret = loadPlugin_jbridge(masterCallBackSlot, filepath, &hmodule, &aeffect, bugfixFlags);
        moduleHandle = hmodule;
#endif //_WIN32
    }

    pluginHostCallback->vstShellCurrentUniqueId = static_cast<VstInt32>(0);
    
    if (ret != 0) {
        return {ret, nullptr};
    }

    if (!aeffect) {
        CLOSE_MODULE_HANDLE(hmodule);
        return {-5, nullptr};
    }

    if (aeffect->magic != kEffectMagic) {
        CLOSE_MODULE_HANDLE(hmodule);
        return {-6, nullptr};
    }

    if (uId == 0) {
        // this branch is only reached by the vst scanner application when passing uId == 0
        VstIntPtr vstIntPtr = aeffect->dispatcher(aeffect, effGetPlugCategory, 0, 0, nullptr, 0);
        auto pluginCategory = static_cast<VstPlugCategory>(vstIntPtr);
        if (pluginCategory == VstPlugCategory::kPlugCategShell) {
            return {1, nullptr, new handles_t(nullptr, aeffect, moduleHandle), filepath, nameWithoutExt};
        }
    }

    if (aeffect->user) {
        CLOSE_MODULE_HANDLE(hmodule);
        return {-7, nullptr};
    }

    //NOTE: Plugins with no inputs and outputs might exists
    if (aeffect->numOutputs <= 0 && aeffect->numInputs <= 0) {
        CLOSE_MODULE_HANDLE(hmodule);
        return {-8, nullptr};
    }

    globalId = getNextGlobalModuleId(globalId);

    auto* plugin = new vstplugin(new handles_t(nullptr, aeffect, moduleHandle), globalId, getHostCallback(), path, nameWithoutExt, -1, bugfixFlags);

    aeffect->user = plugin;
    plugin->handle->localCurrentUniqueId = uId;

    pluginInstancesVST2.push_back(plugin);
    pluginInstances.push_back(plugin);

    plugin->load(this);

    dbgassert(plugin->handle && plugin->handle->aeffect);
    return {0, plugin, plugin->handle, filepath, nameWithoutExt};
};

void PluginManager::scanPlugins() {
    if (mgrImpl->scanningState == 0) {
        try {
            mgrImpl->vstscannerProcessThread = std::make_unique<ProcessThread>();
            String nameScannerExe = "daw-vstscanner.exe";
            if (!FileExists(nameScannerExe)) {
                nameScannerExe = "vstscanner-Clang-debug.exe";
            }
            if (!FileExists(nameScannerExe)) {
                nameScannerExe = "vstscanner-MSVC-debug.exe";
            }
            mgrImpl->vstscannerProcessThread->startProcess(nameScannerExe, "-server -auto", "");
            seqthreads::threadSleep(200);
            if (!mgrImpl->vstscannerProcessThread->isRunning()) {
                mgrImpl->vstscannerProcessThread->checkException();
                log_lf(Log::L_ERROR, "Failed starting vstscanner\n");
            } else {
                mgrImpl->scanningState = 1;
                log_lf(Log::L_DEBUG, "vstscanner is running\n");
            }
        } catch (std::exception& e) {
            log_lf(Log::L_ERROR, "Failed starting vstscanner: %s\n", e.what());
        }
    }
}

void PluginManager::checkScanner() {
    try {
        static int nCalls = 0;
        if (mgrImpl->scanningState && mgrImpl->vstscannerProcessThread) {
            if (!mgrImpl->vstscannerProcessThread->isRunning()) {
                mgrImpl->vstscannerProcessThread->joinProcess();
                mgrImpl->vstscannerProcessThread.reset();
                DawInstance::get()->getPluginDatabase().reopen();
                this->mgrImpl->scanningState = 0;
            } else {
                if (++nCalls >= 10) {
                    nCalls = 0;
                    DawInstance::get()->getPluginDatabase().reopen();
                }
            }

        }
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "checkScanner: %s\n", e.what());
    }
}

void PluginManager::stopScanner() {
    try {
        if (mgrImpl->scanningState && mgrImpl->vstscannerProcessThread) {
            if (mgrImpl->vstscannerProcessThread->isRunning()) {
                mgrImpl->vstscannerProcessThread->killProcess();
                this->mgrImpl->scanningState = 0;
                if (DawInstance::get()) {
                    DawInstance::get()->getPluginDatabase().reopen();
                }

            }

        }
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "stopScanner: %s\n", e.what());
    }
}

bool PluginManager::isScanning() {
    return mgrImpl->scanningState > 0;
}

} // namespace DAW::Host

