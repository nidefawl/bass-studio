#include "appsettings.h"
#include "assert_dbg.h"
#include "audio_config.h"
#include "audioblock.h"
#include "audiobuffer.h"
#include "audiocache.h"
#include "automation.h"
#include "basectrl.h"
#include "clip.h"
#include "cursor.h"
#include "dsp_util.h"
#include "effect_graph.h"
#include "exceptions.h"
#include "fileio.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/gui.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginctr.h"
#include "gui/track/subtrack.h"
#include "gui/track/trackcontent.h"
#include "gui/track/trackcontrols.h"
#include "gui/track/trackctr.h"
#include "history.h"
#include "host/audio_config.h"
#include "host/daw_channel.h"
#include "host/history.h"
#include "host/host_plugin_window.h"
#include "host/mainctrl.h"
#include "host/plugin/vst_plugin.h"
#include "host/pluginmanager.h"
#include "host/vst_event.h"
#include "host_plugin_window.h"
#include "logging.h"
#include "mainctrl.h"
#include "math/seq_math.h"
#include "meter.h"
#include "midi-defs.h"
#include "midi-msg.h"
#include "midi_host.h"
#include "midiarp.h"
#include "modules.h"
#include "note.h"
#include "platform.h"
#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"
#include "plugindatabase.h"
#include "host/pluginmanager.h"
#include "plugins/gain/gain-plugin.h"
#include "plugins/info/info-plugin.h"
#include "plugins/latency/latency-plugin.h"
#include "plugins/samplecrush/samplecrush-plugin.h"
#include "plugins/sampledelay/sampledelay-plugin.h"
#include "plugins/stereowidth/stereowidth-plugin.h"
#include "plugins/synth/synth-plugin.h"
#include "project.h"
#include "projectcontroller.h"
#include "resampler.h"
#include "saferef.h"
#include "samplerate.h"
#include "seq_time.h"
#include "seq_util.h"
#include "snapshot.h"
#include "sse.h"
#include "str_util.h"
#include "thread.h"
#include "threads/childprocessthread.h"
#include "threads/threadlock.h"
#include "threads/workerthread.h"
#include "tls.h"
#include "track.h"
#include "track_graph.h"
#include "track_impl.h"
#include "types.h"
#include "util/profiling.h"
#include "pluginmanager.h"
#include "wave/waveform_render_impl.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <dlfcn.h>
#include <dr_libs/dr_wav.h>
#include <emmintrin.h>
#include <memory.h>
#include <memory>
#include <memory>
#include <utility>
#include <vector>
#include <vstsdk-host-2.4/aeffectx.h>

#ifdef _WIN32
String getModuleName(HMODULE);
#endif

namespace DAW {

class pluginmanager::ModuleManager {
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


pluginmanager::pluginmanager() noexcept
    : mgrImpl(new pluginmanager::pluginmanager_impl()), moduleMgr{new pluginmanager::ModuleManager{}} 
{
    registerModules();
}

pluginmanager::~pluginmanager() {
    delete moduleMgr;
}

void pluginmanager::updateSampleFormat(const sampleformat_t& _sampleFormat) {
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

void DAW::plugin_host_callback::onUiChanged(effectbase* effect) {
    if (effect) {
        // NOTE: this loop might kill performance
        effect->visitParams([](auto& mapEntry) {
            automatable_param_t& param = mapEntry.second;
            param.paramValueState |= plugin_param_sync_state::PARAM_FLAG_DIRTY;
            param.paramDisplayValState |= PARAM_FLAG_DIRTY;
        });
    }
}

void pluginmanager::setTls(daw_tls::tlsinstance& tls) {
    this->mgrImpl->tls = tls;
}

void pluginmanager::unloadPlugin(effectbase* plugin, int flags) {
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

void pluginmanager::updatePluginWindows() {
    for (auto* plugin : pluginInstances) {
        //plugin->dispatch(effEditIdle);
        plugin->updateWindow();
    }
}

i_host_callback* pluginmanager::getHostCallback() {
    return pluginHostCallback.get();
}

void pluginmanager::releaseProjectResources() {
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

vstplugin* pluginmanager::getPlugin(AEffect* aeffect) {
    if (aeffect && aeffect->user) {
        return static_cast<vstplugin*>(aeffect->user);
    }
    //for (auto* current : pluginInstancesVST2) {
    //    if (current->handle->aeffect == aeffect)
    //        return current;
    //}
    return nullptr;
}

effectbase* pluginmanager::getPluginById(int32_t projectGlobalId, bool activeOnly) const {
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

bool pluginmanager::addDeferredEffect(effectbase* plugin) {
    plugin->projectGlobalId = getNextGlobalModuleId(plugin->projectGlobalId);
    while (getPluginById(plugin->projectGlobalId, false) != nullptr) {
        plugin->projectGlobalId = getNextGlobalModuleId(0);
    }
    auto it = std::find_if(pluginsDeferred.begin(), pluginsDeferred.end(), [plugin](auto* eff) { return eff->projectGlobalId == plugin->projectGlobalId; });
    if (it != pluginsDeferred.end()) {
        return false;
    }
    pluginsDeferred.push_back(plugin);
    return true;
}

void pluginmanager::unloadTrack(track_t* track) {
    dbgassert(track->audio);
    auto audio = track->audio;
    std::vector<effectbase*> effects = audio->effects; // make a copy before unloading plugins
    for (effectbase* effect : effects) {
        unloadPlugin(effect, FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
    }
    dbgassert(audio->deferredEffects.empty());
}

void pluginmanager::removePlugin(effectbase* plugin) {
    audio_stage_t* audioStage = plugin->getTrackLink();
    audioStage->removePlugin(plugin, true);
    audioStage->pluginsChanged();
    if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    onTrackLayoutChange();
}


bool pluginmanager::unloadAllPlugins() {
    dbgassert(pluginInstances.empty());
    dbgassert(pluginInstancesVST2.empty());
    dbgassert(pluginInstancesInternal.empty());
    dbgassert(allAudioStages.empty());
    dbgassert(trackAudioStages.empty());
    //int count = list.size();
    //for (int i = 0; i < count; ++i) {
    //    vstplugin* current = list[i];
    //    if (current->trackImpl) {
    //        current->trackImpl->removePlugin(current, false);
    //    }
    //}
    //for (int i = 0; i < count; ++i) {
    //    vstplugin* current = list[i];
    //    current->close();
    //    list[i] = NULL;
    //    current->unload(this);
    //    moduleMgr->releaseModule(current->handle->hmodule);
    //    delete current;
    //}
    //list.clear();
    return true;
}

void pluginmanager::getAllInstances(std::vector<effectbase*>& effects) {
    //for (auto* as : allAudioStages) {
    //    effects.insert(effects.end(), as->effects.begin(), as->effects.end());
    //}
    effects = pluginInstances;
}

void pluginmanager::createAudio(track_t* track) {
    auto audio = new track_impl_t(this,
                                  getNextGlobalAudioStageId(0),
                                  track,
                                  pluginHostCallback->m_sampleFormatInternal,
                                  DAW::DEFAULT_CHANNEL_COUNT);
    allAudioStages.push_back(audio);
    trackAudioStages.push_back(audio);
    track->audio = audio;
}

void pluginmanager::releaseAudio(track_t* track) {
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

audio_stage_t* pluginmanager::createAudioStage() {
    auto audio = new audio_stage_t(this,
                                   getNextGlobalAudioStageId(0),
                                   pluginHostCallback->m_sampleFormatInternal,
                                   DAW::DEFAULT_CHANNEL_COUNT);
    allAudioStages.push_back(audio);
    return audio;
}

void pluginmanager::releaseAudioStage(audio_stage_t* audioStage) {
    auto it = std::find(allAudioStages.begin(), allAudioStages.end(), audioStage);
    dbgassert(it != allAudioStages.end());
    allAudioStages.erase(it);
}

audio_stage_t* pluginmanager::getAudioStage(const audio_stage_ref_t& ref) const {
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

bool pluginmanager::movePlugins(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
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

bool pluginmanager::moveEffects(audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
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

bool pluginmanager::replacePlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst, effectbase** prevPlugin) {
    bool retVal = trp->replaceEffect(dst, plugin, prevPlugin);
    onTrackLayoutChange();
    return retVal;
}

bool pluginmanager::insertNewPlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst) {
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

bool pluginmanager::postPluginLoaded(audio_stage_t* trp, effectbase* plugin) {
    trp->pluginsChanged();
    onTrackLayoutChange();
    if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    dbgassert(validateIds());
    return true;
}
int32_t pluginmanager::getNextGlobalModuleId(int32_t globalId)
{
    if (globalId <= 0) {
        globalId = ++pluginId;
    } else if (globalId < (1 << 16)) {
        globalId += (1 << 16);
    }

    update_maximum(pluginId, globalId);
    return globalId;
}

audio_stage_id_t pluginmanager::getNextGlobalAudioStageId(int32_t globalId) {
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

bool pluginmanager::isStageIdInUse(track_id_snapshot_t stageId) {
    if (stageId.stageId == -1) {
        return false;
    }
    for (auto* id : {&stageId.stageId, &stageId.inputStageId, &stageId.outputStageId, &stageId.outputPostStageId }) {
        if (static_cast<int32_t>(*id) <= audioStageId)
            return true;
    }
    return false;
}

void pluginmanager::updateMaximumStageId() {
    int32_t maximumStageId = 0;
    for (auto* stage : allAudioStages) {
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.stageId));
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.inputStageId));
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.outputStageId));
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.outputPostStageId));
    }
    this->audioStageId = maximumStageId;
}

int32_t pluginmanager::getNextSampleId(int32_t id) {
    if (id <= 0) {
        return ++sampleId;
    }
    update_maximum(sampleId, id);
    return id;
}

int32_t pluginmanager::validateIds()
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
        for (auto plugin2 : pluginsDeferred) {
            if (plugin == plugin2)
                continue;
            auto id2 = plugin2->projectGlobalId;
            always_assert(id2 != id);
        }
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

vstpluginloadres pluginmanager::loadPlugin(String filepath, uint32_t uId, int32_t globalId, uint64_t bugfixFlags) {
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

void pluginmanager::scanPlugins() {
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

void pluginmanager::checkScanner() {
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

void pluginmanager::stopScanner() {
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

bool pluginmanager::isScanning() {
    return mgrImpl->scanningState > 0;
}

} // namespace DAW

namespace {


#define DBG_PRINT_CALLBACKS
#ifdef DBG_PRINT_CALLBACKS
#define MAX_LEN_MY_DBF 512


    bool filterOpCode(int opcode) {
    //    return opcode == audioMasterUpdateDisplay;
    //    if ( opcode == audioMasterSizeWindow)
    //        return true;
    //    if ( opcode == audioMasterBeginEdit)
    //        return true;
    //    if ( opcode == audioMasterEndEdit)
    //        return true;
    //    if ( opcode == audioMasterAutomate)
    //        return true;
    //    if ( opcode == audioMasterGetInputLatency)
    //        return false;
    //    if ( opcode == audioMasterGetOutputLatency)
    //        return false;
        return true;
    }

    void logPluginCb(vstplugin* plugin, const char* fmt, int opcode, int index, int64_t value, float opt = 0);

    void logPluginCb(vstplugin* plugin, const char* fmt, int opcode, int index, int64_t value, float opt)
    {
        if (filterOpCode(opcode)) {
            char buf[MAX_LEN_MY_DBF];
            snprintf(buf, MAX_LEN_MY_DBF - 1, fmt, opcode, index, value, opt);
            log_lf(Log::L_DEBUG, "%s %s", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), buf);
        }
    }

    #else

    void emptyPrinft(vstplugin* plugin, const char *fmt, ...) {
    }
    #define logPluginCb emptyPrinft

    #endif
}
namespace DAW::VST2 {

#define NUM_HOST_CB_SLOTS 4
namespace
{

struct vst_internal_hostslot {
    pluginmanager* g_instance = nullptr;
    DAW::plugin_host_callback* g_hostCallback = nullptr;
};

static vst_internal_hostslot g_hostslots[NUM_HOST_CB_SLOTS];

static double PPQ24TickToSample(double midiTickPPQ24, uint32_t bpm100, samplerate_t samplerate, uint32_t blocksize) {
    double seconds = (midiTickPPQ24/(double)(bpm100*24.0)) * 100.0 * 60.0;
    double samplePos = seconds * samplerate;
    return samplePos;
}
}

bool SetFlag(int& _out, int flag, bool state) {
    bool curState = _out&flag;
    if (state) {
        _out |= flag;
    } else {
        _out &= ~flag;
    }
    return curState != state;
}

//\note VstTimeInfo::samplesToNextClock :
//MIDI Clock Resolution (24 per Quarter Note), can be negative the distance to the next midi clock
//        (24 ppq, pulses per quarter) in samples. unless samplePos falls precicely on a midi clock,
//        this will either be negative such that the previous MIDI clock is addressed,
//        or positive when referencing the following (future) MIDI clock.

void UpdateTime(VstTimeInfo& timeinfo, int32_t transportStateFlags, const sampleformat_t& m_sampleFormatInternal, const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state) {
    static const double fSmpteDiv[] =
    {
        24.f,
        25.f,
        24.f,
        30.f,
        29.97f,
        30.f
    };
    timeinfo.samplePos = samplePos;
    timeinfo.sampleRate = (double) m_sampleFormatInternal.sampleRate;
    timeinfo.nanoSeconds = getTimeMicros() * 1000.0;
    timeinfo.ppqPos = (dTickPos/(double)TICKS_QUARTER);
    timeinfo.tempo = prjGlobals.tempo100/100.0;
    timeinfo.barStartPos = floor(dTickPos / (double) TICKS_BAR) * 4;
    timeinfo.cycleStartPos = (prjGlobals.loopStart/(double)TICKS_QUARTER);
    timeinfo.cycleEndPos = ((prjGlobals.loopStart+prjGlobals.loopLen)/(double)TICKS_QUARTER);
    timeinfo.timeSigNumerator = static_cast<VstInt32>(prjGlobals.signatureNum);
    timeinfo.timeSigDenominator = 1 << prjGlobals.signatureDenom;

    bool loopEnabed = state != playback_state::status_render && prjGlobals.loopEnabled;
    if (!loopEnabed) {
        timeinfo.cycleStartPos = 0;
        timeinfo.cycleEndPos = 0;
    }

    {
        double dPosSeconds = samplePos / timeinfo.sampleRate;
        /* offset in fractions of a second   */
        double dOffsetInSecond = dPosSeconds - floor(dPosSeconds);
        timeinfo.smpteFrameRate = VstSmpteFrameRate::kVstSmpte24fps;
        timeinfo.smpteOffset = math::floordS32(dOffsetInSecond * fSmpteDiv[timeinfo.smpteFrameRate] * 80.);
    }


    double midiTickPPQ24 = timeinfo.ppqPos*24.0;
    double samplePosMidiTick = PPQ24TickToSample(midiTickPPQ24, prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);
    double samplePosPrevMidiTick = PPQ24TickToSample(math::floord(midiTickPPQ24), prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);
    double samplePosNextMidiTick = PPQ24TickToSample(math::ceild(midiTickPPQ24), prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);

    double samplePosClosestPPQ24Tick = math::absMin(samplePosPrevMidiTick - samplePosMidiTick, samplePosNextMidiTick - samplePosMidiTick);
    //TODO: assingn nearest clock (can be negative), not next aka soonest
    timeinfo.samplesToNextClock = math::rounddS32(samplePosClosestPPQ24Tick);

    {
        timeinfo.flags = (transportStateFlags & (kVstTransportPlaying | kVstTransportCycleActive | kVstTransportRecording | kVstTransportChanged));
        SetFlag(timeinfo.flags, kVstAutomationWriting, false);
        SetFlag(timeinfo.flags, kVstAutomationReading, false);
        SetFlag(timeinfo.flags, kVstNanosValid, true);
        SetFlag(timeinfo.flags, kVstPpqPosValid, true);
        SetFlag(timeinfo.flags, kVstTempoValid, true);
        SetFlag(timeinfo.flags, kVstBarsValid, true);
        SetFlag(timeinfo.flags, kVstCyclePosValid, true); //project.loopEnabled
        SetFlag(timeinfo.flags, kVstTimeSigValid, true);
        SetFlag(timeinfo.flags, kVstSmpteValid, true);
        SetFlag(timeinfo.flags, kVstClockValid, true);
    }
}


int32_t HostCanDo(const char* ptr) {
    if ((!strcmp(ptr, HostCanDos::canDoSendVstEvents)) ||
        (!strcmp(ptr, HostCanDos::canDoSendVstMidiEvent)) ||
        (!strcmp(ptr, HostCanDos::canDoSendVstTimeInfo)) ||
        (!strcmp(ptr, HostCanDos::canDoReceiveVstEvents)) ||
        (!strcmp(ptr, HostCanDos::canDoReceiveVstMidiEvent)) ||
        (!strcmp(ptr, HostCanDos::canDoReportConnectionChanges)) ||
        (!strcmp(ptr, HostCanDos::canDoAcceptIOChanges)) ||
        (!strcmp(ptr, HostCanDos::canDoSizeWindow)) ||
        (!strcmp(ptr, HostCanDos::canDoSendVstMidiEventFlagIsRealtime)) ||
        (!strcmp(ptr, HostCanDos::canDoStartStopProcess)) ||
        (!strcmp(ptr, HostCanDos::canDoShellCategory)))
        return 1;
    if (!strcmp(ptr, "NIMKPIVendorSpecificCallbacks")) {
        return -1;
    }
    return 0;
}

/**
 * VST Host AudioMasterCallback
 */
VstIntPtr audioMasterHost(pluginmanager* host, DAW::plugin_host_callback* hostCallback, AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(host);
    // In case a plugin instance outlives the host
    if (!host)
        return 0;

    /**
     * Thread safety is guaranteed by only allowing internal threads to enter the callback
     * TODO: Find out what exact thread we got called from. @see notes
     */
    vstplugin *plugin = host->getPlugin(effect);

    bool bIsKnownThread = false;
    bool bIsInternalThread = false;
    seqthreads::getThreadInfo(bIsKnownThread, bIsInternalThread);
    if (!bIsKnownThread) {
        seqthreads::registerThread("External", false);
        daw_tls::setTls(host->mgrImpl->tls);
        log_lf(Log::L_WARN, "(First) Request from external thread: Plugin '%s' opcode %d %d %zd %f\n", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), opcode, index, value, opt);
        bIsInternalThread = false;
    }
    /* if (!bIsInternalThread) {
        log_lf(Log::L_WARN, "Request from external thread: Plugin '%s' opcode %d %d %zd %f\n", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), opcode, index, value, opt);
    } */

    /**
     * TODO: Detect reentrance and guard against it. @see notes
     */
    bool throttleLog = false;
    bool validProcessingState = false;
    if (plugin) {
        //TODO: getOpCodeStats is not a threadsafe implementation
        vst_opcode_stats_t& opcodeStats = plugin->getOpCodeStats(true, opcode);
        opcodeStats.numDispatches++;
        int32_t tmMillisS32 = static_cast<int32_t>(static_cast<uint64_t>(getTimeMillis()) & (0x7FFF'FFFFLL));
        int32_t tmSince = tmMillisS32 - opcodeStats.tmMillis;
        if (tmSince < 2000) {
            throttleLog = tmSince > 50 && opcodeStats.numDispatches > 20;
        } else {
            opcodeStats.tmMillis = tmMillisS32;
        }
        /**
         * Validate that the plugin is currently fully loaded and setup and connected to an audiostage that is valid.
         * Currently plugins have an extended lifetime after removal inside the edithistory.
         *
         * TODO: This check is not well implemented
         * Add a lock free thread-safe way to check (from the callback) if a plugin is ready for processing
         */
        /* Ignore audioMasterVersion, audioMasterGetVendorString and audioMasterGetProductString */
        if (opcode == audioMasterVersion || opcode == audioMasterGetVendorString || opcode == audioMasterGetProductString) {
            validProcessingState = true;
        }
        if (!validProcessingState && plugin->hasTrackLink()) {
            auto parent = plugin->trackImpl;
            while (parent->parent) parent = parent->parent;
            // get this from the host instead of the tls
            auto projCtrl = project_controller_t::get();
            if (projCtrl && projCtrl->getTracks().resolveTrack(parent->toRef())) {
                validProcessingState = true;
            }
        }
        if (!validProcessingState) {
            log_lf(Log::L_WARN, "%s opCode %s in !validProcessingState\n", StringAsCStr(plugin->sName), getMasterOpcodeName(opcode));
        }
    }
    switch (opcode)
    {
    case audioMasterAutomate:
        if (plugin) {
            auto* effParam = plugin->getEffectParam(index);
            // log_printf("%s audioMasterAutomate param index %d %zd %f\n", StringAsCStr(plugin->getName()), index, value, opt);
            if (!effParam) {
                if (!throttleLog)
                    log_printf("%s audioMasterAutomate unknown param index %d %zd %f\n", StringAsCStr(plugin->getName()), index, value, opt);
            } else {
                // call to deactivateAutomation is not thread safe,
                plugin->deactivateAutomation(effParam->idx);
                effParam->value = opt;
                effParam->paramValueState = PARAM_FLAG_SET;
                effParam->paramDisplayValState |= PARAM_FLAG_DIRTY;
                effParam->inUse = true;
            }
        }
        return 1;
    case audioMasterVersion:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterVersion %d %d %zd\n", opcode, index, value, 0);
        return 2400; //VST 2.4
    case audioMasterCurrentId:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterCurrentId %d %d %zd\n", opcode, index, value, 0);
        //return OnGetCurrentUniqueId(nEffect);
        if (plugin) {
            return (VstIntPtr)plugin->getLocalCurrentUniqueId();
        }
        return hostCallback->vstShellCurrentUniqueId;
    case audioMasterIdle:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterIdle %d %d %zd\n", opcode, index, value, 0);
        //return OnIdle(nEffect);
        return 0;
    case audioMasterGetTime:
        //{
        //    int32_t playThreadId = host->getPlayThreadId();
        //    int32_t localThreadId = getCurrentThreadId();
        //    if (localThreadId == playThreadId) {
        //        return (VstIntPtr)plugin->getLocalTimeInfoPtr();
        //    }
        //}
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetTime %d %d %zd\n", opcode, index, value);
        if (plugin) {
            return (VstIntPtr)plugin->getLocalTimeInfoPtr();
        }
        return (VstIntPtr)&hostCallback->m_vstTimeInfo;

    case audioMasterProcessEvents:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterProcessEvents %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterIOChanged:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterIOChanged %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterNeedIdle:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterNeedIdle %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            plugin->bWantsEffIdle = true;
        }
        return 0;
    case audioMasterSizeWindow:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterSizeWindow %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            plugin->updateWindowSize();
        }
        return 1;
    case audioMasterGetSampleRate:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetSampleRate %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            return plugin->format.sampleRate;
        }
        if (host) {
            return hostCallback->m_sampleFormatInternal.sampleRate;
        }
        return 0;
    case audioMasterGetBlockSize:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetBlockSize %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            return plugin->format.blockSize;
        }
        if (host) {
            return hostCallback->m_sampleFormatInternal.blockSize;
        }
        return 0;
    case audioMasterGetInputLatency:
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetInputLatency %d %d %zd\n", opcode, index, value);
        //TODO: find out if other hosts provide this info
        // IL Harmor requests this info
        return 0;
    case audioMasterGetOutputLatency:
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetOutputLatency %d %d %zd\n", opcode, index, value);
        //TODO: find out if other hosts provide this info
        // IL Harmor requests this info
        return 0;
    case audioMasterGetCurrentProcessLevel:
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetCurrentProcessLevel %d %d %zd\n", opcode, index, value);
        if (hostCallback->isOfflineRendering){
            return VstProcessLevels::kVstProcessLevelOffline;
        }
        return VstProcessLevels::kVstProcessLevelRealtime;
    case audioMasterGetAutomationState:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetAutomationState %d %d %zd\n", opcode, index, value, 0);
        return kVstAutomationRead;
    case audioMasterOfflineStart:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineStart %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineRead:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineRead %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineWrite:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineWrite %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineGetCurrentPass:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineGetCurrentPass %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineGetCurrentMetaPass:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineGetCurrentMetaPass %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterGetVendorString:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetVendorString %d %d %zd\n", opcode, index, value, 0);
        if (ptr) {
            strcpy(static_cast<char*>(ptr), "NFMH");
            return 1;
        }
        return 0;
    case audioMasterGetProductString:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetProductString %d %d %zd\n", opcode, index, value, 0);
        if (ptr) {
            strcpy(static_cast<char*>(ptr), "DAW");
            return 1;
        }
        return 0;
    case audioMasterGetVendorVersion:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetVendorVersion %d %d %zd\n", opcode, index, value, 0);
        return 1;
    case audioMasterVendorSpecific:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterVendorSpecific %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterCanDo:
        if (!throttleLog) {
            log_lf(Log::L_DEBUG, "%s audioMasterCanDo %s\n", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), (const char*)ptr);
        }
        return DAW::VST2::HostCanDo((const char*)ptr);
    case audioMasterGetLanguage:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetLanguage %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterGetDirectory:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetDirectory %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            return (VstIntPtr) plugin->getDir();
        }
        return 0;
    case audioMasterUpdateDisplay:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterUpdateDisplay %d %d %zd\n", opcode, index, value, 0);
        if (plugin && validProcessingState && !plugin->bIsLoadingProgram) {
            plugin->recvProgramNameUpdate();
            // NOTE: this loop might kill performance
            plugin->visitParams([](auto& mapEntry) {
                automatable_param_t& param = mapEntry.second;
                param.paramValueState |= PARAM_FLAG_DIRTY;
                param.paramDisplayValState |= PARAM_FLAG_DIRTY;
            });
            return 1;
        }
        return 0;
#ifdef VST_2_1_EXTENSIONS
    case audioMasterBeginEdit:
        if (plugin && validProcessingState && !plugin->bIsLoadingProgram) {
            if (!throttleLog)
                logPluginCb(plugin, "audioMasterBeginEdit %d %d %zd %f\n", opcode, index, value, opt);
            auto* effParam = plugin->getEffectParam(index);
            if (!effParam) {
                if (!throttleLog)
                    log_printf("%s audioMasterBeginEdit unknown param index %d %zd %f\n", StringAsCStr(plugin->getName()), index, value, opt);
            } else {
              plugin->handle->paramEditing = {effParam->internalIdx, effParam->value};
            }
        }
        return 1;
    case audioMasterEndEdit:
        // if (!throttleLog)
        //     logPluginCb(plugin, "audioMasterEndEdit %d %d %zd %f\n", opcode, index, value, opt);
        if (plugin && validProcessingState && !plugin->bIsLoadingProgram && plugin->handle->paramEditing.paramIdx > -1) {
            if (!throttleLog)
                logPluginCb(plugin, "audioMasterEndEdit %d %d %zd %f\n", opcode, index, value, opt);
            auto* effParam = plugin->getEffectParam(index);
            if (!effParam) {
                if (!throttleLog)
                    log_printf("%s audioMasterEndEdit unknown param index %d %zd %f\n", StringAsCStr(plugin->getName()), index, value, opt);
            } else {
                dbgassert(plugin->trackImpl->getTrack());
                auto newVal = effParam->value;
                auto oldVal = plugin->handle->paramEditing.valBefore;
                track_t* track                = plugin->trackImpl->getTrack();
                automationlane_snapshot_t ref = plugin->toRef();
                parameter_ref_t p             = { track->projectIdx, ref.type, plugin->projectGlobalId, effParam->idx };
                DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, oldVal, newVal));

            }
        }
        if (plugin) {
            plugin->handle->paramEditing = {-1, 0.0f};
        }
        return 1;
    case audioMasterOpenFileSelector:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOpenFileSelector %d %d %zd\n", opcode, index, value, 0);
        return 0;
#endif
#ifdef VST_2_2_EXTENSIONS
    case audioMasterCloseFileSelector:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterCloseFileSelector %d %d %zd\n", opcode, index, value, 0);
        return 0;
#endif
    case audioMasterWantMidi:
        if (!throttleLog)
            logPluginCb(plugin, "depr audioMasterWantMidi %d %d %zd\n", opcode, index, value, 0);
        return 0;
    default:
        if (!throttleLog)
            logPluginCb(plugin, "unhandled %d %d %zd %f\n", opcode, index, value, opt);

    }
    return 0;
}

VstIntPtr VSTCALLBACK audioMaster1(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[0].g_instance);
    dbgassert(g_hostslots[0].g_hostCallback);
    return audioMasterHost(g_hostslots[0].g_instance, g_hostslots[0].g_hostCallback, effect, opcode, index, value, ptr, opt);
}

VstIntPtr VSTCALLBACK audioMaster2(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[1].g_instance);
    dbgassert(g_hostslots[1].g_hostCallback);
    return audioMasterHost(g_hostslots[1].g_instance, g_hostslots[1].g_hostCallback, effect, opcode, index, value, ptr, opt);
}

VstIntPtr VSTCALLBACK audioMaster3(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[2].g_instance);
    dbgassert(g_hostslots[2].g_hostCallback);
    return audioMasterHost(g_hostslots[2].g_instance, g_hostslots[2].g_hostCallback, effect, opcode, index, value, ptr, opt);
}

VstIntPtr VSTCALLBACK audioMaster4(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[3].g_instance);
    dbgassert(g_hostslots[3].g_hostCallback);
    return audioMasterHost(g_hostslots[3].g_instance, g_hostslots[3].g_hostCallback, effect, opcode, index, value, ptr, opt);
}
} // namespace DAW::VST2

namespace DAW {

void pluginmanager::onBeforeBlock(const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state) {
    auto loopEnabed = state != playback_state::status_render && prjGlobals.loopEnabled;
    auto& vst2TransportState = mgrImpl->vst2TransportStateFlags;
    bool changed = DAW::VST2::SetFlag(vst2TransportState, kVstTransportPlaying, DAW::isPlaybackState(state));
    changed |= DAW::VST2::SetFlag(vst2TransportState, kVstTransportCycleActive, loopEnabed);
    changed |= DAW::VST2::SetFlag(vst2TransportState, kVstTransportRecording, false);
    DAW::VST2::SetFlag(vst2TransportState, kVstTransportChanged, changed);
}
void pluginmanager::UpdateVstTime(VstTimeInfo& timeInfo, const sampleformat_t& sampleFormat, const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state) const {
    DAW::VST2::UpdateTime(timeInfo,
                            mgrImpl->vst2TransportStateFlags,
                            sampleFormat,
                            prjGlobals,
                            samplePos,
                            dTickPos,
                            state);
}

void pluginmanager::destroy() {
    stopScanner();
    dbgassert(hostSlot > -1);
    dbgassert(DAW::VST2::g_hostslots[hostSlot].g_instance);
    DAW::VST2::g_hostslots[hostSlot].g_instance = nullptr;
}

bool pluginmanager::assignMasterCallback(pluginmanager* host)
{
    host->pluginHostCallback = std::make_shared<DAW::plugin_host_callback>(host);
    for (int i = 0; i < NUM_HOST_CB_SLOTS; i++) {
        if (DAW::VST2::g_hostslots[i].g_instance == nullptr) {
            DAW::VST2::g_hostslots[i].g_instance = host;
            DAW::VST2::g_hostslots[i].g_hostCallback = host->pluginHostCallback.get();
            host->hostSlot = i;
            if (i == 0) {
                host->masterCallBackSlot = DAW::VST2::audioMaster1;
            }
            if (i == 1) {
                host->masterCallBackSlot = DAW::VST2::audioMaster2;
            }
            if (i == 2) {
                host->masterCallBackSlot = DAW::VST2::audioMaster3;
            }
            if (i == 3) {
                host->masterCallBackSlot = DAW::VST2::audioMaster4;
            }
            return true;
        }
    }
    dbgassert(0&&"Out of host slots");
    return false;
}
} // namespace DAW
