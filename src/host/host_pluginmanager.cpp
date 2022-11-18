#include "host/host_pluginmanager.h"
#include "assert_dbg.h"
#include "fileio.h"
#include "host/daw_channel.h"
#include "logging.h"
#include "host/plugin/modules.h"
#include "host/plugin/base/base-plugin.h"
#include "host/plugin/clap/clap-plugin.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/plugin/vst/vstplugin-handles.h"
#include "str_util.h"
#include "host/track/track_impl.h"
#include <clap/clap.h>
#include <memory>

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
        // String moduleName = getModuleName((HMODULE)module);
        // log_printf("Unload %s\n", StringAsCStr(moduleName));
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
    delete mgrImpl;
    delete moduleMgr;
}

void PluginManager::updateSampleFormat(const sampleformat_t& _sampleFormat) {
    for (auto stage : this->allAudioStages) {
        stage->sampleFormat = _sampleFormat;
        stage->input.realloc(_sampleFormat.blockSize);
        stage->output.realloc(_sampleFormat.blockSize);
        stage->outputPost.realloc(_sampleFormat.blockSize);
    }
    for (auto plugin : this->pluginInstancesVST2) {
        plugin->onDisable();
    }
    for (auto plugin : this->pluginInstancesClap) {
        plugin->onDisable();
    }
    for (auto plugin : this->pluginInstances) {
        plugin->setSampleFormat(_sampleFormat);
        plugin->initBuffers();
        plugin->initMeters();
    }
    for (auto plugin : this->pluginsDeferred) {
        plugin->setSampleFormat(_sampleFormat);
        plugin->initBuffers();
        plugin->initMeters();
    }
    for (auto plugin : this->pluginInstancesVST2) {
        plugin->dispatch(effSetBlockSize, 0, _sampleFormat.blockSize);
        plugin->dispatch(effSetSampleRate, 0, 0, nullptr, (float) _sampleFormat.sampleRate);
        plugin->onEnable();
    }
    for (auto plugin : this->pluginInstancesClap) {
        plugin->onEnable();
    }
    for (auto stage : this->allAudioStages) {
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

/* Make sure opened context menu controls do not reference effectbase or its gui
   instance after this call */
void PluginManager::unloadPlugin(effectbase* plugin) {
    plugin->onPreUnload();
    audio_stage_t* audioStage = plugin->getTrackLink();
    if (audioStage) {
        audioStage->removePlugin(plugin);
        audioStage->pluginsChanged();
    }

    plugin->unload(this);

    switch (plugin->getModuleType()) {
    case PLUGIN_TYPE_DEFERRED:
        always_assert(removeEntry(pluginsDeferred, plugin));
        break;
    case PLUGIN_TYPE_CLAP:
        always_assert(removeEntry(pluginInstancesClap, plugin));
        always_assert(removeEntry(pluginInstances, plugin));
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
    void* moduleHandleOpt = nullptr;
    if (plugin->getModuleType() == PLUGIN_TYPE_VST || plugin->getModuleType() == PLUGIN_TYPE_INTERNAL_EFFECT) {
        vstplugin* vst = static_cast<vstplugin*>(plugin);
        if (vst->internalModuleId <= 0) {
            moduleHandleOpt = vst->handle->hmodule;
        }
    }
    if (plugin->getModuleType() == PLUGIN_TYPE_CLAP) {
        moduleHandleOpt = static_cast<clapplugin*>(plugin)->getModuleHandle();
    }
    delete plugin;
    if (moduleHandleOpt) {
        moduleMgr->releaseModule(moduleHandleOpt);
    }
    validateIds();
}

void PluginManager::updatePluginWindows() {
    for (auto* plugin : pluginInstances) {
        //plugin->dispatch(effEditIdle);
        plugin->updateFromMainThread();
    }
}

IHostCallback* PluginManager::getHostCallback() {
    return pluginHostCallback.get();
}

void PluginManager::unload() {
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
        log_lf(Log::L_ERROR, "Duplicate plugin id %d\n", plugin->projectGlobalId);
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
        unloadPlugin(effect);
    }
    dbgassert(audio->deferredEffects.empty());
}

void PluginManager::removePlugin(effectbase* plugin) {
    audio_stage_t* audioStage = plugin->getTrackLink();
    audioStage->removePlugin(plugin);
    audioStage->pluginsChanged();
}

void PluginManager::getAllInstances(std::vector<effectbase*>& effects) {
    effects = pluginInstances;
}

void PluginManager::createAudio(track_t* track, std::optional<audio_stage_id_t> stageId) {
    int32_t stageIdFirst = -1;
    if (stageId) {
        stageIdFirst = static_cast<int32_t>(stageId->stageId);
    }
    auto audio = new track_impl_t(this,
                                  getNextGlobalAudioStageId(stageIdFirst),
                                  track,
                                  pluginHostCallback->m_sampleFormatInternal,
                                  DAW::Host::DEFAULT_CHANNEL_COUNT);
    allAudioStages.push_back(audio);
    trackAudioStages.push_back(audio);
    track->audio = audio;
    validateIds();
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
    validateIds();
}

audio_stage_t* PluginManager::createAudioStage() {
    auto audio = new audio_stage_t(this,
                                   getNextGlobalAudioStageId(),
                                   pluginHostCallback->m_sampleFormatInternal,
                                   DAW::Host::DEFAULT_CHANNEL_COUNT);
    allAudioStages.push_back(audio);
    validateIds();
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

bool PluginManager::movePluginsToStage(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
    ThreadLock lock = mgrImpl->tls.dawInstance->lockPlayThread();
    dbgassert(dstTr);
    dbgassert(trp);
    dbgassert(src < (int)trp->effects.size());
    dbgassert(src+len <= (int)trp->effects.size());
    dbgassert(dst-1 <= (int)dstTr->effects.size());
    std::vector<effectbase*> tmpEffects = trp->effects;
    for (int32_t i = 0; i < len; i++) {
        effectbase* tmpPlugin = tmpEffects[src + i];
        trp->removePlugin(tmpPlugin);
        dstTr->insertEffect(dst+i, tmpPlugin);
    }
    trp->pluginsChanged();
    dstTr->pluginsChanged();
    return true;
}

bool PluginManager::movePluginsOnStage(audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
    ThreadLock lock = mgrImpl->tls.dawInstance->lockPlayThread();
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
    trp->pluginsChanged();
    return true;
}

bool PluginManager::replacePlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst, effectbase** prevPlugin) {
    bool retVal = trp->replaceEffect(dst, plugin, prevPlugin);
    trp->pluginsChanged();
    return retVal;
}

bool PluginManager::insertNewPlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst) {
    trp->insertEffect(dst, plugin);
    trp->pluginsChanged();
    return true;
}

void PluginManager::onTick() {
    // Currently no lock
    for (auto* current : pluginInstances) {
        //TODO: should we skip dispatching if current->bWantsEffIdle == false ?!
        if (current->bEditOpen && !current->bInEditIdle) {
            current->bInEditIdle = true;
            current->bInEditIdle = false;
            if (current->windowHost) {
                //current->window->captureWindowFrame();
                current->updateFromMainThread();
            }
        }
    }
    ThreadLock lock = mgrImpl->tls.dawInstance->lockPlayThread();
    for (auto* clapPlugin : pluginInstancesClap) {
        clapPlugin->updateFromMainThread();
    }
    checkScanner();
}

bool PluginManager::postPluginLoaded(audio_stage_t* trp, effectbase* plugin) {
    onTrackLayoutChange();
    validateIds();
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
        startId = audioStageId;
    }
    for (audiostageid_i32* id : stageIds) {
        *id = static_cast<audiostageid_i32>(startId);
        startId++;
    }
    update_maximum(audioStageId, startId);
    return stageId;
}

bool PluginManager::isStageIdInUse(track_id_snapshot_t stageId) {
    if (stageId.stageId == -1) {
        return false;
    }
    for (auto* id : {&stageId.stageId, &stageId.inputStageId, &stageId.outputStageId, &stageId.outputPostStageId }) {
        if (static_cast<int32_t>(*id) < audioStageId)
            return true;
    }
    return false;
}
bool PluginManager::isStageIdInUse(const audio_stage_id_t& stageId) {
    if (stageId.stageId == TRACKID_INVALID_I32) {
        return false;
    }
    for (auto* id : {&stageId.stageId, &stageId.inputStageId, &stageId.outputStageId, &stageId.outputPostStageId }) {
        for (auto* stage : allAudioStages) {
            for (auto* id2 : {&stage->stageId.stageId, &stage->stageId.inputStageId, &stage->stageId.outputStageId, &stage->stageId.outputPostStageId }) {
                if (*id == *id2)
                    return true;
            }
        }
    }
    return false;
}

void PluginManager::updateMaximumStageId() {
    int32_t maximumStageId = 0;
    for (auto* stage : allAudioStages) {
        maximumStageId = math::max<int32_t>(maximumStageId, 1 + static_cast<int32_t>(stage->stageId.stageId));
        maximumStageId = math::max<int32_t>(maximumStageId, 1 + static_cast<int32_t>(stage->stageId.inputStageId));
        maximumStageId = math::max<int32_t>(maximumStageId, 1 + static_cast<int32_t>(stage->stageId.outputStageId));
        maximumStageId = math::max<int32_t>(maximumStageId, 1 + static_cast<int32_t>(stage->stageId.outputPostStageId));
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
    auto curMaxAudioStageId = this->audioStageId.load();
    for (auto stage : allAudioStages) {
        audiostageid_i32* stageIds[4] = {&stage->stageId.stageId, &stage->stageId.inputStageId, &stage->stageId.outputStageId,
                                         &stage->stageId.outputPostStageId};
        for (auto* pStageId : stageIds) {
            dbgassert(curMaxAudioStageId > *pStageId);
            for (auto* pStageId2 : stageIds) {
                if (pStageId2 == pStageId) {
                    always_assert(*pStageId == *pStageId2);
                    continue;
                }
                always_assert(*pStageId != *pStageId2);
            }
        }
        for (auto stage2 : allAudioStages) {
            if (stage2 == stage)
                continue;
            audiostageid_i32* stageIds2[4] = {&stage2->stageId.stageId, &stage2->stageId.inputStageId, &stage2->stageId.outputStageId,
                                              &stage2->stageId.outputPostStageId};
            for (auto* pStageId : stageIds) {
                for (auto* pStageId2 : stageIds2) {
                    always_assert(*pStageId != *pStageId2);
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

void PluginManager::assignNewStageId(audio_stage_t* trp, audio_stage_id_t newId) {
    trp->stageId = newId;
    updateMaximumStageId();
    validateIds();
}

#ifdef _WIN32
HMODULE safeLoadLib(const char* szLibName);
#define CLOSE_MODULE_HANDLE(handle) FreeLibrary(handle)
LoadResultSharedLibrary loadLib(const String& filepath, int32_t moduleFmt) {
    if (!FileExists(filepath)) {
        return LoadResultSharedLibrary::FromError(SharedLibState::FILE_NOT_FOUND, "File not found");
    }
    HMODULE module = safeLoadLib(StringAsCStr(filepath));
    if (!module) {
        auto dwErr = GetLastError();
        return LoadResultSharedLibrary::FromError(SharedLibState::DL_OPEN_FAILED, FormatErrorMessage(dwErr, "LoadLibrary failed"));
    }
    if (moduleFmt == -1 || moduleFmt == 1) {
        auto symbolClapEntry = reinterpret_cast<void*>(GetProcAddress(module, "clap_entry"));
        if (symbolClapEntry) {
            return LoadResultSharedLibrary::FromSuccess(SharedLibPluginType ::CLAP, module, symbolClapEntry);
        }
    }
    if (moduleFmt == -1 || moduleFmt == 0) {
        auto symbolVSTPluginMain= reinterpret_cast<void*>(GetProcAddress(module, "VSTPluginMain"));
        if (symbolVSTPluginMain) {
            return LoadResultSharedLibrary::FromSuccess(SharedLibPluginType::VST2, module, symbolVSTPluginMain);
        }
        auto symbolMain = reinterpret_cast<void*>(GetProcAddress(module, "main"));
        if (symbolMain) {
            return LoadResultSharedLibrary::FromSuccess(SharedLibPluginType::VST2, module, symbolMain);
        }
    }
    CLOSE_MODULE_HANDLE(module);
    return LoadResultSharedLibrary::FromError(SharedLibState::DL_UNKNOWN_FORMAT, "Entry point not found");
}


#endif

#if defined(__linux__) || defined(__APPLE__)

#define CLOSE_MODULE_HANDLE(handle) dlclose(handle)

LoadResultSharedLibrary loadLib(const String& filepath, int32_t moduleFmt) {
    if (!FileExists(filepath)) {
        return LoadResultSharedLibrary::FromError(SharedLibState::FILE_NOT_FOUND, "File not found");
    }
    void* module = dlopen(StringAsCStr(filepath), RTLD_NOW);
    if (!module) {
        auto dl_err = dlerror();
        return LoadResultSharedLibrary::FromError(SharedLibState::DL_OPEN_FAILED, StringFormat("dlopen failed: %s", dl_err));
    }
    if (moduleFmt == -1 || moduleFmt == 1) {
        auto symbolClapEntry = dlsym(module, "clap_entry");
        if (symbolClapEntry) {
            return LoadResultSharedLibrary::FromSuccess(SharedLibPluginType::CLAP, module, symbolClapEntry);
        }
    }
    if (moduleFmt == -1 || moduleFmt == 0) {
        auto symbolVSTPluginMain= dlsym(module, "VSTPluginMain");
        if (symbolVSTPluginMain) {
            return LoadResultSharedLibrary::FromSuccess(SharedLibPluginType::VST2, module, symbolVSTPluginMain);
        }
        auto symbolMain = dlsym(module, "main");
        if (symbolMain) {
            return LoadResultSharedLibrary::FromSuccess(SharedLibPluginType::VST2, module, symbolMain);
        }
    }
    CLOSE_MODULE_HANDLE(module);
    return LoadResultSharedLibrary::FromError(SharedLibState::DL_UNKNOWN_FORMAT, "Entry point not found");
}

#endif
#ifdef _WIN32
int loadPlugin_jbridge(audioMasterCallback audiomasterCallback, const String& filepath, HMODULE* hmodule, AEffect** aeffect, uint64_t bugfixFlags);
#endif //_WIN32
LoadResultPlugin PluginManager::loadPlugin(const PluginLoadParameters& req) {
    dbgassert(masterCallBackSlot);
    const auto& filepath = req.filepath;
    const auto uId = req.uId;
    int32_t globalId = req.globalId;

    String path, name, nameWithoutExt, fileExt;
    SplitPath(filepath, &path, &nameWithoutExt, &fileExt, &name);
    auto moduleFormat = req.moduleFormat;
    if (fileExt == "clap") {
        moduleFormat = 1;
    }
    auto libResult = loadLib(filepath, moduleFormat);
    void* moduleHandle = libResult.module;
#ifdef _WIN32
    HMODULE hmodule = reinterpret_cast<HMODULE>(libResult.module);
#else
    void* hmodule = libResult.module;
#endif //_WIN32

    struct RAIIModuleDeleter {
#ifdef _WIN32
        HMODULE moduleToClose;
#else
        void* moduleToClose;
#endif
        ~RAIIModuleDeleter() { if (moduleToClose) CLOSE_MODULE_HANDLE(moduleToClose);}
    } moduleCloser{hmodule};

    if (libResult.state == SharedLibState::SUCCESS && libResult.type == SharedLibPluginType::CLAP) {
        globalId = getNextGlobalModuleId(globalId);
        auto plugin = new clapplugin(*this, filepath, nameWithoutExt, uId, globalId, getHostCallback());
        if (!plugin->loadClapPlugin(libResult)) {
            delete plugin;
            libResult.state = SharedLibState::FAILED;
            return LoadResultPlugin{libResult};
        }
        pluginInstancesClap.push_back(plugin);
        pluginInstances.push_back(plugin);

        plugin->load(this);
        moduleCloser.moduleToClose = nullptr;
        return {libResult, plugin, filepath, nameWithoutExt};
    }

    if (uId != 0) {
        pluginHostCallback->vstShellCurrentUniqueId = static_cast<VstInt32>(uId);
    } else {
        pluginHostCallback->vstShellCurrentUniqueId = static_cast<VstInt32>(0);
    }

    AEffect* aeffect = nullptr;
    if (libResult.state == SharedLibState::SUCCESS && libResult.type == SharedLibPluginType::VST2) {
        dbgassert(libResult.entryPoint);
        VSTPluginMain_t* fn = reinterpret_cast<VSTPluginMain_t*>(libResult.entryPoint);
        aeffect = fn(masterCallBackSlot);
#ifdef _WIN32
    } else if (libResult.state == SharedLibState::DL_OPEN_FAILED && moduleFormat <= 0) {
        auto ret = loadPlugin_jbridge(masterCallBackSlot, filepath, &hmodule, &aeffect, req.bugfixFlags);
        libResult.state = ret == 0 ? SharedLibState::SUCCESS : SharedLibState::DL_UNKNOWN_FORMAT;
        libResult.type = ret == 0 ? SharedLibPluginType::VST2 : SharedLibPluginType::UNKNOWN;
        moduleHandle = hmodule;
#endif //_WIN32
    }

    pluginHostCallback->vstShellCurrentUniqueId = static_cast<VstInt32>(0);
    
    if (libResult.state != SharedLibState::SUCCESS) {
        return LoadResultPlugin{libResult};
    }
    if (!aeffect || libResult.type != SharedLibPluginType::VST2) {
        libResult.state = SharedLibState::DL_UNKNOWN_FORMAT;
        return LoadResultPlugin{libResult};
    }

    if (aeffect->magic != kEffectMagic) {
        libResult.state = SharedLibState::DL_UNKNOWN_FORMAT;
        return LoadResultPlugin{libResult};
    }

    if (uId == 0) {
        // this branch is only reached by the vst scanner application when passing uId == 0
        VstIntPtr vstIntPtr = aeffect->dispatcher(aeffect, effGetPlugCategory, 0, 0, nullptr, 0);
        auto pluginCategory = static_cast<VstPlugCategory>(vstIntPtr);
        if (pluginCategory == VstPlugCategory::kPlugCategShell) {
            libResult.type = SharedLibPluginType::VST2_SHELL;
            return {libResult, nullptr, new handles_t(nullptr, aeffect, moduleHandle), filepath, nameWithoutExt};
        }
    }

    if (aeffect->user) {
        libResult.state = SharedLibState::DL_UNKNOWN_FORMAT;
        return LoadResultPlugin{libResult};
    }

    //NOTE: Plugins with no inputs and outputs might exists
    if (aeffect->numOutputs <= 0 && aeffect->numInputs <= 0) {
        libResult.state = SharedLibState::DL_UNKNOWN_FORMAT;
        return LoadResultPlugin{libResult};
    }

    globalId = getNextGlobalModuleId(globalId);

    auto* plugin = new vstplugin(new handles_t(nullptr, aeffect, moduleHandle), globalId, getHostCallback(), path, nameWithoutExt, -1, req.bugfixFlags);

    aeffect->user = plugin;
    plugin->handle->localCurrentUniqueId = uId;

    pluginInstancesVST2.push_back(plugin);
    pluginInstances.push_back(plugin);

    plugin->load(this);

    dbgassert(plugin->handle && plugin->handle->aeffect);
    moduleCloser.moduleToClose = nullptr;
    return {libResult, plugin, plugin->handle, filepath, nameWithoutExt};
};

void PluginManager::scanPlugins() {
    if (mgrImpl->scanningState == 0) {
        try {
            mgrImpl->threadPluginScannerProcess = std::make_unique<ProcessThread>();
            
            auto scannerNames = {
                "daw-pluginscanner", 
                "pluginscanner-Clang-debug",
                "pluginscanner-MSVC-debug"
            };
            String filename = "daw-pluginscanner";
            for (auto* name : scannerNames) {
                if (FileExists(name)){
                    filename = name;
                    break;
                }
            }
#ifdef _WIN32
            filename += ".exe";
#endif //_WIN32
            mgrImpl->threadPluginScannerProcess->startProcess(filename, "-server -auto", "");
            seqthreads::threadSleep(200);
            if (!mgrImpl->threadPluginScannerProcess->isRunning()) {
                mgrImpl->threadPluginScannerProcess->checkException();
                log_lf(Log::L_ERROR, "Failed starting daw-pluginscanner\n");
            } else {
                mgrImpl->scanningState = 1;
                log_lf(Log::L_DEBUG, "daw-pluginscanner is running\n");
            }
        } catch (std::exception& e) {
            log_lf(Log::L_ERROR, "Failed starting daw-pluginscanner: %s\n", e.what());
        }
    }
}

void PluginManager::checkScanner() {
    try {
        static int nCalls = 0;
        if (mgrImpl->scanningState && mgrImpl->threadPluginScannerProcess) {
            if (!mgrImpl->threadPluginScannerProcess->isRunning()) {
                mgrImpl->threadPluginScannerProcess->joinProcess();
                mgrImpl->threadPluginScannerProcess.reset();
                mgrImpl->tls.dawInstance->getPluginDatabase().reopen();
                this->mgrImpl->scanningState = 0;
            } else {
                if (++nCalls >= 10) {
                    nCalls = 0;
                    mgrImpl->tls.dawInstance->getPluginDatabase().reopen();
                }
            }

        }
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "checkScanner: %s\n", e.what());
    }
}

void PluginManager::stopScanner() {
    try {
        if (mgrImpl->scanningState && mgrImpl->threadPluginScannerProcess) {
            if (mgrImpl->threadPluginScannerProcess->isRunning()) {
                mgrImpl->threadPluginScannerProcess->killProcess();
                this->mgrImpl->scanningState = 0;
                if (mgrImpl->tls.dawInstance)
                    mgrImpl->tls.dawInstance->getPluginDatabase().reopen();
            }

        }
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "stopScanner: %s\n", e.what());
    }
}

bool PluginManager::isScanning() {
    return mgrImpl->scanningState > 0;
}

LoadResultPlugin::LoadResultPlugin(LoadResultSharedLibrary _lib, vstplugin* _plugin, handles_t* _shellHandle, String _path, String _name)
    : library(std::move(_lib)), plugin(_plugin), vstPlugin(_plugin), shellPluginHandle(_shellHandle), path(std::move(_path)), name(std::move(_name)){};
LoadResultPlugin::LoadResultPlugin(LoadResultSharedLibrary _lib, clapplugin* _plugin, String _path, String _name)
    : library(std::move(_lib)), plugin(_plugin), clapPlugin(_plugin), path(std::move(_path)), name(std::move(_name)){};
LoadResultPlugin::LoadResultPlugin(LoadResultSharedLibrary _lib, vstplugin* _plugin) : library(std::move(_lib)), plugin(_plugin), vstPlugin(_plugin){};
}// namespace DAW::Host
