#include <algorithm>

#include "audioblock.h"
#include "automation.h"
#include "config.h"
#include "host/audio_config.h"
#include "host/daw_channel.h"
#include "host/effect_graph.h"
#include "host/host.h"
#include "host/plugin/internal/internal-plugin.h"
#include "math/seq_math.h"
#include "exceptions.h"
#include "logging.h"
#include "platform.h"
#include "plugins/synth/IPlugMidi.h"
#include "samplerate.h"
#include "seq_util.h"
#include "seq_time.h"

#include "gui/plugin/pluginctr.h"
#include "gui/track/trackctr.h"
#include "gui/track/trackcontrols.h"
#include "gui/track/trackcontent.h"

#include "note.h"
#include "clip.h"
#include "midiarp.h"
#include "track.h"
#include "cursor.h"
#include "audiocache.h"
#include "host/plugin/base/base-plugin.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/plugin/vst/vstplugin-handles.h"
#include "types.h"
#include "host_pluginmanager.h"
#include "track_impl.h"

#include "modules.h"
#include "project.h"
#include "projectcontroller.h"
#include "snapshot/snapshot.h"
#include "mainctrl.h"
#include "history.h"
#include "plugindatabase.h"
#include "wave/waveform_render_impl.h"
#include "gui/track/subtrack.h"
#include "midi-event.h"
#include "fileio.h"
#include "clip.h"
#include "assert_dbg.h"


const tick_t INVALID_TICK = 1 << 31;

void releaseClipResources(clip_t* cl, delete_cb* cb) {
    if (cb)
        cb->preClipDelete(cl);

    std::vector<track_gui_entry_t*> copyEntries = cl->trackEntries;
    for (track_gui_entry_t* entry : copyEntries) {
        track_t* track = entry->track;
        if (track) {
            if (entry->clipsGuis.count(cl)) {
                auto* pGui = entry->clipsGuis[cl];
                dbgassert(pGui);
                entry->content->remove(pGui);
                delete pGui;
            } else {
                dbgassert(0);
            }
        }
    }
}
void releaseTrackResources(track_t* tr, delete_cb* cb) {
    dbgassert(tr && tr->audio);
    if (cb)
        cb->preTrackDelete(tr);
    auto* mgr = daw_tls::getTls().pluginManager;
    mgr->unloadTrack(tr);
    tr->getMidi().deleteClips(cb);
    dbgassert(tr->audio->guiInstances.empty());
    mgr->releaseAudio(tr);
    dbgassert(tr && !tr->audio);
}

std::vector<clip_t*>::iterator trackdata_midi_t::removeClip(clip_t* clip) {
    auto it = std::find(clips.begin(), clips.end(), clip);
    if (it == clips.end()) {
        throw applogicexception("track - attempt to remove non-present clip");
    }
    return clips.erase(it);
}
std::pair<clip_t*, clip_t*> trackdata_midi_t::getMinMax() {
    auto minmax = std::minmax_element(clips.begin(), clips.end(),
                                      [](clip_t* const& lhs, clip_t* const& rhs) {
                                          return lhs->time < rhs->time;
                                      });
    std::pair<clip_t*, clip_t*> pairPtr;
    if (minmax.first != clips.end()) {
        pairPtr.first = *minmax.first;
    }
    if (minmax.second != clips.end()) {
        pairPtr.second = *minmax.second;
    }
    return pairPtr;
}
tick_t trackdata_midi_t::start() {
    auto minmax = getMinMax();
    return minmax.first ? minmax.first->time : 0;
}
tick_t trackdata_midi_t::end() {
    auto minmax = getMinMax();
    return minmax.second ? minmax.second->end() : 0;
}

track_t& track_t::operator=(const track_snapshot_t& obj) {
    *static_cast<tracksettings_t*>(this) = obj.trackSettings;
    if (obj.storeOpts.storeClips) {
        dbgassert(midi.getConstClips().empty());
        for (const clip_t& clip : obj.clips) {
            midi.addClip(new clip_t(clip));
        }
        midi.sortClips();
    }
    if (obj.storeOpts.storeLayouts) {
        scrolloffset = 0;
    }
    return *this;
}
void track_t::updateAudioClipLengths(int32_t bpm100, samplerate_t oldSampleRate, samplerate_t newSampleRate) {
    double conversionFactor = newSampleRate / double(oldSampleRate);
    for (clip_t* clip : midi.getClips()) {
        if (clip->clipType == CLIP_AUDIO) {
            dbgassert(clip->lenSamples > 0 || clip->len > 0);
            if (clip->lenSamples > 0) {
                clip->lenSamples = math::ceildS64(clip->lenSamples * conversionFactor);
                clip->len = sampleToTickConvert<tick_t, roundmode::round>(clip->lenSamples, bpm100, newSampleRate);
            } else if (clip->len > 0) {
                clip->lenSamples = tickToSampleConvert<samplecount_t, roundmode::round>(clip->len, bpm100, newSampleRate);
            }
        }

    }
}
track_t::track_t(const track_snapshot_t& obj)
    : tracksettings_t(obj.trackSettings), localIdxFlat(obj.localIdx) {
    if (obj.storeOpts.storeClips) {
        dbgassert(midi.getConstClips().empty());
        for (const clip_t& clip : obj.clips) {
            midi.addClip(new clip_t(clip));
        }
    }
}

track_impl_snapshot_t::track_impl_snapshot_t(track_impl_t* p, const tracksnapshot_store_opts_t& opts) {
    if (p) {
        if (p->arp) {
            p->arp->createSnapshot(trackArp, opts);
        }
        p->mixer.createSnapshot(trackParams, opts);
        p->createIOSnapshot(trackIO);
        p->createRoutingSnapshot(effectRouting);
        p->createModulationRoutingSnapshot(modulationRouting);
        std::vector<effectbase*> effects = p->effects;
        pluginSnapshots.reserve(p->effects.size());
        for (effectbase* effect : p->effects) {
            plugin_snapshot_t ps;
            effect->makeSnapshot(ps, opts);
            pluginSnapshots.push_back(std::move(ps));
        }
    }
}

void saveSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, track_layout_snapshot_t& snapshot);
void saveTrackLayoutSettings(guictr_tracks* guiTracks, track_gui_entry_t* entry, tracklayout_settings_t& settings);

track_id_snapshot_t saveTrackIdSnapshot(const audio_stage_id_t& stageId) {
    return {
        static_cast<int32_t>(stageId.stageId),
        static_cast<int32_t>(stageId.inputStageId),
        static_cast<int32_t>(stageId.outputStageId),
        static_cast<int32_t>(stageId.outputPostStageId)
    };
}
audio_stage_id_t loadTrackIdSnapshot(const track_id_snapshot_t& stageId) {
    return {
        static_cast<audiostageid_i32>(stageId.stageId),
        static_cast<audiostageid_i32>(stageId.inputStageId),
        static_cast<audiostageid_i32>(stageId.outputStageId),
        static_cast<audiostageid_i32>(stageId.outputPostStageId)
    };
}
track_snapshot_t::track_snapshot_t(const track_t* track, const tracksnapshot_store_opts_t& opts)
    : storeOpts(opts),
      trackSettings(*track),
      stageIds(track->audio ? saveTrackIdSnapshot(track->audio->stageId) : track_id_snapshot_t{}),
      localIdx(track->localIdxFlat),
      data(track->audio, opts) {

    auto& otherClips = track->getConstMidi().getConstClips();
    for (auto clip : otherClips) {
        clips.emplace_back(*clip);
    }

    track_impl_t* p = track->audio;
    if (p) {
        // get all trackcointainer instances
        std::vector<guictr_tracks*> trackContainers;
        DawInstance::get()->getTrackContainers(trackContainers);
        int32_t trackCtrIdx = 0;
        for (auto* ctr : trackContainers) {
            if (ctr) {
                track_gui_entry_t* out;
                if (ctr->guiMgr.getTrackEntry(track, &out)) {
                    track_layout_snapshot_t snapshot;
                    saveTrackLayoutSettings(ctr, out, snapshot.layout);
                    saveSubtrackLayout(ctr, out, snapshot);
                    layouts[trackCtrIdx] = snapshot;
                }
            }
            trackCtrIdx++;
        }
    }
}


void track_t::loadSnapshot(const track_snapshot_t& snapshot) {
    auto audio = this->audio;
    dbgassert(audio);
    const auto& implSnapshot = snapshot.data;
    // if the snapshot holds a stage id then use it, otherwise keep current stageId
    if (snapshot.stageIds.inputStageId != -1) {
        audio->stageId.stageId           = static_cast<audiostageid_i32>(snapshot.stageIds.stageId);
        audio->stageId.inputStageId      = static_cast<audiostageid_i32>(snapshot.stageIds.inputStageId);
        audio->stageId.outputStageId     = static_cast<audiostageid_i32>(snapshot.stageIds.outputStageId);
        audio->stageId.outputPostStageId = static_cast<audiostageid_i32>(snapshot.stageIds.outputPostStageId);
    }
    //TODO: test if stageId is in use. Caller is responsible for generating new stageId

    audio->mixer.loadSnapshot(implSnapshot.trackParams);
    if (audio->arp) {
        audio->arp->loadSnapshot(implSnapshot.trackArp);
    }
    const std::vector<plugin_snapshot_t>& trPluginList = implSnapshot.pluginSnapshots;
    audio->loadPlugins(trPluginList);
    audio->loadIOConfiguration(implSnapshot.trackIO);
    audio->loadRoutingSnapshot(implSnapshot.effectRouting);
    audio->loadModulationRoutingSnapshot(implSnapshot.modulationRouting);
    if (audio->routingState == audiostagerouting_state_t::INVALID) {
        audio->configureDefaultRoutings();
    }
}

void track_t::releaseTrackContent() {
}

void trackdata_midi_t::deleteClips(delete_cb* cb) {
    for (auto clip : clips) {
        releaseClipResources(clip, cb);
    }
    for (auto clip : clips) {
        delete clip;
    }
    clips.clear();
}

void trackdata_midi_t::deleteEmptyClips(delete_cb* cb) {
    auto it = clips.begin();
    while (it != clips.end()) {
        clip_t* c = *it;
        if (c->getLen() <= 0) {
            it = removeClip(c);
            releaseClipResources(c, cb);
            delete c;
        } else {
            it++;
        }
    }
    sortClips();
}

void trackdata_midi_t::getClipsInRange(tick_t start, tick_t end, std::vector<clip_t*>& _clips) {
    for (clip_t* clip : clips) {
        if (clip->end() <= start || clip->start() > end) {
            continue;
        }
        if (clip->clipType == CLIP_AUDIO) {
            _clips.push_back(clip);
        }
    }
}

void trackdata_midi_t::getNotesInRange(tick_t start, tick_t end, tick_t cutStart, tick_t cutEnd, std::vector<note_t>& notes) {
    for (clip_t* clip : clips) {
        if (clip->end() <= start || clip->start() > end) {
            continue;
        }
        clip->getInTimeRange(start, end, cutStart, cutEnd, notes);
    }
}


void trackdata_midi_t::getEventsInRange(tick_t start, tick_t end, tick_t cutStart, tick_t cutEnd, std::vector<note_t>& notes, std::vector<DAW::Host::midievent_ctrl_t>& ctrlEvents) {
    for (clip_t* clip : clips) {
        if (clip->end() <= start || clip->start() > end) {
            continue;
        }
        clip->getInTimeRange(start, end, cutStart, cutEnd, notes);
        clip->controlData.getInTimeRange(clip, start, end, cutStart, cutEnd, ctrlEvents);
    }
}

audio_stage_ref_t audio_stage_t::toRef() const {
    return { this->stageId.stageId };
}

effectbase* audio_stage_t::getPluginById(int32_t projectGlobalId) const {
    if (projectGlobalId < 1 << 16) {
        projectGlobalId += 1 << 16;
    }
    for (effectbase* effect : effects) {
        if (effect->projectGlobalId == projectGlobalId) {
            return effect;
        }
    }
    for (audio_stage_t* t : children) {
        auto* effect = t->getPluginById(projectGlobalId);
        if (effect) return effect;
    }
    return nullptr;
}

void track_impl_t::removePlugin(effectbase* _effect, bool notifyUp) {
    audio_stage_t::removePlugin(_effect, notifyUp);
}

void audio_stage_t::removePlugin(effectbase* _effect, bool notifyUp) {
    auto stage = _effect->getTrackLink();
    if (stage && notifyUp) {
        auto trackFromStage = stage->getTrack();
        if (trackFromStage && trackFromStage->getStage()) {
            for (auto trackentry : trackFromStage->getStage()->guiInstances) {
                trackentry->state.selectedAutomationCtr = nullptr;
                trackentry->parent->removeAllAutomationLanes(trackentry, _effect);
            }
        }
    }
    removeEntry(deferredEffects, _effect);
    if (!removeEntry(effects, _effect)) {
        return;
    }
    int slot = 0;
    for (effectbase* effect : effects) {
        effect->setSlot(slot++);
    }
    _effect->breakTrackLink();
    if (stage && notifyUp) {
        stage->notifyPluginContainers();
    }
}

bool audio_stage_t::replaceEffect(int32_t idx, effectbase* _effect, effectbase** _prevEffect) {
    dbgassert(idx >= 0 && idx < (int32_t) effects.size());
    if (idx >= 0 && idx < (int32_t) effects.size()) {
        auto cur   = effects[idx];
        auto stage = cur->getTrackLink();
        for (auto trackentry : cur->getTrackLink()->getTrack()->getStage()->guiInstances) {
            trackentry->state.selectedAutomationCtr = nullptr;
            trackentry->parent->removeAllAutomationLanes(trackentry, _effect);
        }
        cur->breakTrackLink();
        if (stage) {
            stage->notifyPluginContainers();
        }
        *_prevEffect = cur;
        effects[idx] = _effect;
        _effect->setTrackLink(this);
        int slot = 0;
        for (effectbase* effect : effects) {
            effect->setSlot(slot++);
        }
        this->notifyPluginContainers();
        return true;
    }
    return false;
}

void audio_stage_t::insertEffect(int32_t idx, effectbase* _effect) {
    std::vector<effectbase*>::iterator it;
    if (idx == -2 || idx >= (int32_t) effects.size()) {
        it = effects.end();
    } else if (idx <= 0) {
        it = effects.begin();
    } else {
        it = effects.begin() + idx;
    }
    effects.insert(it, _effect);
    if (_effect->isDeferred()) {
        deferredEffects.push_back(_effect);
    }
    _effect->setTrackLink(this);
    int slot = 0;
    for (effectbase* effect : effects) {
        effect->setSlot(slot++);
    }
    this->notifyPluginContainers();
}

track_impl_t::~track_impl_t() {
    delete arp;
    delete midiValidation;
}

samplecount_t audio_stage_t::getInternalLatency() const {
    samplecount_t latency = 0;
    for (effectbase* effect : effects) {
        latency += effect->getPluginLatency();
    }
    return latency;
}

samplecount_t audio_stage_t::getOutputLatency() const {
    return latencyOuput;
}

samplecount_t audio_stage_t::getInputLatency() const {
    return latencyInput;
}

void audio_stage_t::pluginsChanged() {
    if (routingState != audiostagerouting_state_t::CUSTOM) {
        configureDefaultRoutings();
    }
    DAW::validateEffectRoutings(this->host, this);

    this->processingGraph.reset();
}

void audio_stage_t::getStageTargets(std::vector<automatable_t*>& targets) {
    // already added mixer in track_impl to have it before arp
    if (std::find(targets.begin(), targets.end(), &mixer) == targets.end()) {
        targets.push_back(&mixer);
    }
    // avoid vector construction
    if (effects.empty())
        return;
    std::vector<audio_stage_t*> childStages;
    for (effectbase* child : effects) {
        targets.push_back(child);
        childStages.clear();
        child->getChildAudioStages(childStages);
        for (audio_stage_t* childStage : childStages) {
            childStage->getStageTargets(targets);
        }
    }
    // Instead we could get the child targets by:
    /* for (audio_stage_t* childStage : this->children) {
        childStage->getStageTargets(targets);
    } */
    // But then we would have to sort the list
}

void audio_stage_t::onStopPlayback() {
}

void audio_stage_t::sendNotesOff() {
    for (effectbase* effect : effects) {
        effect->sendNotesOff();
    }
}

void audio_stage_t::notifyPluginContainers() {
    audio_stage_t* audioStage = this;
    while (audioStage != nullptr) {
        guictr_plugins* pluginCtr = audioStage->m_pluginCtr;
        if (pluginCtr) {
            dbgassert(MainCtrl::get());
            plugin_selection& sel = MainCtrl::get()->getPluginSel();
            if (sel.pluginCtr == pluginCtr) {
                sel.clear();
            }
            pluginCtr->showTrack(audioStage);
        }
        audioStage = audioStage->parent;
    }
    processingGraph.reset();
}

void track_impl_t::getAutomatableTrackTargets(std::vector<automatable_t*>& targets, bool includeEffects) {
    targets.push_back(&mixer);
    if (arp) {
        targets.push_back(arp);
    }
    if (includeEffects) {
        getStageTargets(targets);
    }
}

void track_impl_t::updateAutomatableTargets(DAW::Host::Host* const host, tick_t processingPos, playback_state state) {
    mixer.updateAutomatedParameters(host, processingPos, state);
    if (arp) {
        arp->updateAutomatedParameters(host, processingPos, state);
    }
}

void project_t::copyTo(project_snapshot_t& project) {
    trackList.copyTo(project);
}

void project_t::copyFrom(project_snapshot_t& project) {
    trackList.copyFrom(project);
}
namespace DAW {
    effectbase* loadEffectModule(Host::PluginManager* host, const plugin_snapshot_t& pluginSnapshot, bool forceLoad) {
        effectbase* effect      = nullptr;
        if (pluginSnapshot.pluginType == PLUGIN_TYPE_VST || pluginSnapshot.pluginType == PLUGIN_TYPE_CLAP) {
            log_printf("Loading Plugin '%s'\n", StringAsCStr(pluginSnapshot.name));
            plugindatabase_t* db = plugindatabase_t::getInstance();
            pluginentry_t resolvedPlugin;
            if (db->resolvePlugin(pluginSnapshot, resolvedPlugin, forceLoad ? 1 : 0)) {
                auto res = host->loadPlugin({resolvedPlugin.path, pluginSnapshot.uId, pluginSnapshot.projectGlobalId, resolvedPlugin.bugfixFlags, resolvedPlugin.moduleFormat});
                if (res.library.isSuccess()) {
                    res.plugin->localDbId = resolvedPlugin.localDbId;
                    effect = res.plugin;
                } else {
                    log_printf("Failed loading: Error loading plugin %s, uId %d. Res: %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId, static_cast<int32_t>(res.library.state));
                }
            } else {
                log_printf("Failed loading: Unknown plugin %s, uId %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId);
            }
        } else {
            effect = host->makeModuleInstance(pluginSnapshot.pluginType, pluginSnapshot.uId, pluginSnapshot.projectGlobalId);
        }
        return effect;
    }
    void removePlugin(DawInstance* daw, effectbase* module) {
        ThreadLock lock           = daw->getPlayThread()->lockThread();
        audio_stage_t* audioStage = module->getTrackLink();
        dbgassert(audioStage);
        module->closeWindow();
        audioStage->removePlugin(module, true);
        std::vector<effectbase*> effects;
        effects.push_back(module);
        auto* actionRemove = new action_remove_modules("Remove plugin", std::move(effects), audioStage->toRef(), module->getSlot());
        daw->pushHist(actionRemove);
        audioStage->pluginsChanged();
        daw->onPluginsChanged();
    }
    void loadEffectParamsFromSnapshot(const plugin_snapshot_t& pluginSnapshot, effectbase* effect) {
        const std::vector<param_snapshot_t>& pluginSnapshotParams = pluginSnapshot.params;
        uint32_t missingParams = 0;
        for (const param_snapshot_t& param : pluginSnapshotParams) {
            automatable_param_t* atParam = effect->getParam(param.idx);
            if (atParam) {
                auto paramVal = math::clamp(param.val, 0.0f, 1.0f);
                dbgassert(paramVal >= 0.0f && paramVal <= 1.0f);
                int flags = FLG_PAR_UPDATE_INIT;
                if (!param.flags) {
                    flags |= FLG_PAR_UPDATE_NOSTORE;
                }
                effect->setParamValue(atParam->idx, paramVal, flags);
            } else {
                missingParams++;
            }
        }
        if (missingParams) {
            //TODO: notify users thru UI
            log_printf("Some parameters could not be mapped: %s has %d missing parameters\n", StringAsCStr(effect->getName()), missingParams);
        } else {
            log_printf("%s: Loaded %zu params\n", StringAsCStr(effect->getName()), pluginSnapshotParams.size());
        }
        //const std::vector<param_snapshot_t>& pluginHostSideParams = pluginSnapshot.hostParams;
        //for (const param_snapshot_t& param : pluginHostSideParams) {
        //    automatable_param_t* atParam = effect->getParam(param.idx);
        //    if (atParam) {
        //        effect->setParamValue(atParam->idx, param.val, FLG_PAR_UPDATE_INIT);
        //    }
        //}
    }
    void createDawChannelRefSnapshot(const channel_ref_t& channel, io_configuration_snapshot_t& cfg) {
        cfg.type              = static_cast<int32_t>(channel.type);
        cfg.stageId           = static_cast<int32_t>(channel.stage.stageRef.stageId);
        cfg.stageEndPointType = static_cast<int32_t>(channel.stage.buffer);
        cfg.externalInputType = static_cast<int32_t>(channel.externalInputType);
        cfg.projectGlobalId   = channel.projectGlobalId;
        cfg.externalInputIdx  = channel.externalInputIdx;
        cfg.srcChannelOffset  = channel.srcChannelOffset;
        cfg.dstChannelOffset  = channel.dstChannelOffset;
    }
    void loadDawChannelRefSnapshot(const io_configuration_snapshot_t& cfg, channel_ref_t& channel) {
        channel.type                   = static_cast<stage_type>(cfg.type);
        channel.stage.stageRef.stageId = static_cast<audiostageid_i32>(cfg.stageId);
        channel.stage.buffer           = static_cast<stage_bufferpoint>(cfg.stageEndPointType);
        channel.externalInputType      = static_cast<channel_pairing>(cfg.externalInputType);
        channel.projectGlobalId        = cfg.projectGlobalId;
        channel.externalInputIdx       = cfg.externalInputIdx;
        channel.srcChannelOffset       = cfg.srcChannelOffset;
        channel.dstChannelOffset       = cfg.dstChannelOffset;
    }
    void createMidiChannelRefSnapshot(const midichannel_ref_t& channel, io_midi_snapshot_t& cfg) {
        cfg.type              = static_cast<int32_t>(channel.type);
        cfg.stageId           = static_cast<int32_t>(channel.stage.stageRef.stageId);
        cfg.stageEndPointType = static_cast<int32_t>(channel.stage.buffer);
        cfg.externalInputIdx  = channel.externalInputIdx;
    }
    void loadMidiChannelRefSnapshot(const io_midi_snapshot_t& cfg, midichannel_ref_t& channel) {
        channel.type                   = static_cast<midistage_type>(cfg.type);
        channel.stage.stageRef.stageId = static_cast<audiostageid_i32>(cfg.stageId);
        channel.stage.buffer           = static_cast<stage_bufferpoint>(cfg.stageEndPointType);
        channel.externalInputIdx       = cfg.externalInputIdx;
    }
}

void audio_stage_t::createRoutingSnapshot(track_effect_routing_snapshot_t& snapshot) {
    for (auto & channel : this->postEffectRouting) {
        io_configuration_snapshot_t cfg;
        createDawChannelRefSnapshot(channel, cfg);
        snapshot.inputRoutingOutputStage.push_back(cfg);
    }
    for (effectbase* effect : effects) {
        auto& vec = snapshot.inputRoutingEffects[static_cast<int32_t>(effect->projectGlobalId)];
        for (auto& channel : effect->inputChannels) {
            io_configuration_snapshot_t cfg;
            createDawChannelRefSnapshot(channel, cfg);
            vec.push_back(cfg);
        }
    }
    snapshot.routingState = static_cast<int32_t>(this->routingState);
}

void audio_stage_t::configureDefaultRoutings() {
    this->postEffectRouting.clear();
    this->postEffectRouting.push_back(DAW::ChannelDefaultNone());
    for (effectbase* effect : effects) {
        effect->inputChannels.clear();
        if (!effect->inputChannelsDesc.empty()) {
            effect->inputChannels.push_back(DAW::ChannelDefaultNone());
        }
    }
    routingState = audiostagerouting_state_t::DEFAULT;
}

void audio_stage_t::loadRoutingSnapshot(const track_effect_routing_snapshot_t& snapshot) {
    this->postEffectRouting.clear();
    this->routingState = audiostagerouting_state_t::INVALID;
    for (const auto & cfg : snapshot.inputRoutingOutputStage) {
         DAW::channel_ref_t channel;
        loadDawChannelRefSnapshot(cfg, channel);
        this->postEffectRouting.push_back(channel);
    }
    auto snapshotRoutingState = static_cast<audiostagerouting_state_t>(snapshot.routingState);
    //for (plugin in effectsVec)
    //    plugin->inputChannels.clear();
    //if (snapshotRoutingState != audiostagerouting_state_t::INVALID)
    {
        for (const auto& mapEntry : snapshot.inputRoutingEffects) {
            auto* plugin = getPluginById(mapEntry.first);
            //dbgassert(plugin);
            if (!plugin) {
                String log = "stage.effects = [";
                for (auto p : effects) {
                    if (p->isDeferred()) {
                        auto* def = dynamic_cast<effect_deferred*>(p);
                        const plugin_snapshot_t& plugSnapshot = def->getSnapshotConst();
                        log += StringFormat("%d(%d), ", static_cast<int32_t>(p->projectGlobalId), plugSnapshot.projectGlobalId);
                    } else {
                        log += StringFormat("%d, ", static_cast<int32_t>(p->projectGlobalId));
                    }
                }
                log += "]";
                log_lf(Log::L_DEBUG, "%s\n", StringAsCStr(log));
                String log2 = "snapshot.inputRoutingEffects = [";
                for (auto& p : snapshot.inputRoutingEffects) {
                    log2 += StringFormat("%d, ", static_cast<int32_t>(p.first));
                }
                log2 += "]";
                log_lf(Log::L_DEBUG, "%s\n", StringAsCStr(log2));
                log_lf(Log::L_DEBUG, "Plugin with id %d not found\n", static_cast<int32_t>(mapEntry.first));
                snapshotRoutingState = audiostagerouting_state_t::INVALID;
            } else {
                plugin->inputChannels.clear();
                for (const io_configuration_snapshot_t& effInputSnapshot : mapEntry.second) {
                    DAW::channel_ref_t channel;
                    loadDawChannelRefSnapshot(effInputSnapshot, channel);
                    plugin->inputChannels.push_back(channel);
                }
            }
        }
    }
    //dbgassert(snapshot.routingState == 0);
    this->routingState = snapshotRoutingState;
}

void audio_stage_t::createModulationRoutingSnapshot(track_modulation_routing_snapshot_t& snapshot) {
    for (effectbase* effect : effects) {
        auto& modulations = effect->getModulations();
        if (!modulations.empty()) {
            snapshot.effectMods[static_cast<int32_t>(effect->projectGlobalId)] = modulations;
        }
    }
}
void audio_stage_t::loadModulationRoutingSnapshot(const track_modulation_routing_snapshot_t& snapshot) {
    for (effectbase* effect : effects) {
        effect->setModulations({});
    }
    for (const auto& mapEntry : snapshot.effectMods) {
        auto* plugin = getPluginById(mapEntry.first);
        if (!plugin) {
            log_lf(Log::L_DEBUG, "Plugin with id %d not found\n", static_cast<int32_t>(mapEntry.first));
        } else {
            plugin->setModulations(mapEntry.second);
        }
    }
}

void track_impl_t::createIOSnapshot(track_io_configuration_snapshot_t& snapshot) {
    for (int i = 0; i < 2; i++) {
        DAW::channel_ref_t channel       = i == 0 ? inputChannel : outputChannel;
        io_configuration_snapshot_t& cfg = i == 0 ? snapshot.input : snapshot.output;
        createDawChannelRefSnapshot(channel, cfg);
    }
    createMidiChannelRefSnapshot(midiChannel, snapshot.midiInput);
}

void track_impl_t::loadIOConfiguration(const track_io_configuration_snapshot_t& snapshot) {
    for (int i = 0; i < 2; i++) {
        DAW::channel_ref_t& channel     = i == 0 ? inputChannel : outputChannel;
        io_configuration_snapshot_t cfg = i == 0 ? snapshot.input : snapshot.output;
        loadDawChannelRefSnapshot(cfg, channel);
    }
    loadMidiChannelRefSnapshot(snapshot.midiInput, midiChannel);
}

void audio_stage_t::loadPlugins(const std::vector<plugin_snapshot_t>& trPluginList) {
    for (const plugin_snapshot_t& pluginSnapshot : trPluginList) {
        auto effect = host->loadPluginDeferred(pluginSnapshot);
        if (effect) {
            //this->deferredEffects.push_back(effect);
            if (!host->addDeferredEffect(effect)) {
                log_printf("Failed loading effect\n");
                delete effect;
                continue;
            }
            effect->getSnapshot().projectGlobalId = effect->projectGlobalId;
            effect->load(host);
            host->insertNewPlugin(this, effect, pluginSnapshot.slot);
            //host->postPluginLoaded(this, effect);
            dbgassert(effect->trackImpl == this);
            dbgassert(!effects.empty());
            if (effect->getModuleStoredType() == PLUGIN_TYPE_GROUP) {
                host->activateDeferred(effect, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
            }
        }
    }
}

namespace DAW::Host {

void PluginManager::activateDeferred(effectbase* const eff, int flags, effectbase** out_effectLoaded) {
    dbgassert(eff->trackImpl);
    dbgassert(eff->trackImpl->effects.size());
    dbgassert(eff->getSlot() >= 0);

    auto defEffect = dynamic_cast<effect_deferred*>(eff);
    plugin_snapshot_t pluginSnapshot = defEffect->getSnapshotConst();
    effectbase* effect = loadEffectModule(this, pluginSnapshot, flags & FLAG_HOST_FORCELOAD_DISABLED_PLUGINS);
    if (out_effectLoaded) {
        *out_effectLoaded = effect;
    }
    if (!effect) {
        log_printf("Failed loading %s\n", StringAsCStr(pluginSnapshot.name));
        //dbgassert(0);
        return;
    }

    /* Begin of loading plugin state (parameter values, binary preset, automation lanes) */

    effectbase* prevPlugin = nullptr;
    always_assert(removeEntry(eff->trackImpl->deferredEffects, eff));
    replacePlugin(eff->trackImpl, effect, defEffect->getSlot(), &prevPlugin);

    /* Load plugins snapshot */
    effect->loadSnapshot(pluginSnapshot);

    effect->setModulations(prevPlugin->getModulations());
    effect->inputChannels = prevPlugin->inputChannels;
    effect->sName         = pluginSnapshot.name;
    effect->setProductName(pluginSnapshot.name);
    effect->setParamValue(PARAM_ENABLE, pluginSnapshot.enabled ? 1.0f : 0.0f, FLG_PAR_UPDATE_INIT | FLG_PAR_UPDATE_NOSTORE);

    /* Load plugin parameter automation lanes */
    loadAutomation(pluginSnapshot.automatedParams, effect);

    if (pluginSnapshot.enabled) {
        effect->onEnable();
    }

    /* Unload the (previous) deferred placeholder plugin */
    unloadPlugin(prevPlugin, flags);

    bool notifyUp = !(flags & FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
    if (notifyUp) {
        // When loading multiple plugins only fire it for the last one, or run postPluginLoaded externally

        //TODO: this shouldn't be here!
        postPluginLoaded(effect->getTrackLink(), effect);
    }
    //    if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
}

void midi_input_events_t::addMidiEvent(tick_t tick, uint32_t message, int32_t midiTime) {
    if (!m_list.empty() && tick >= m_list.back().tick) {
        m_list.push_back({ tick, message, midiTime });
    } else {
        for (auto it = m_list.begin(); it != m_list.end(); ++it) {
            if (it->tick > tick) {
                m_list.insert(it, { tick, message, midiTime });
                return;
            }
        }
        m_list.push_back({ tick, message, midiTime });
    }
}

}// namespace DAW::Host

void loadSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, const track_layout_snapshot_t& snapshot) {
    const auto& subtrackSnapshots = snapshot.subtracks;

    auto* const track = entry->track;
    for (const auto& stSnapshot : subtrackSnapshots) {
        gui_track_subtrack* al = nullptr;
        if (stSnapshot.settings.subtrackType == gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
            auto& atlRef = stSnapshot.atlRef;
            if (atlRef.type == AUTOMATABLE_EFFECT) {
                effectbase* plugin = track->getStage()->getPluginById(atlRef.refId);
                if (!plugin || plugin->getModuleType() == PLUGIN_TYPE_DEFERRED) {
                    continue;
                }
                if (!assert_expr(plugin->getParam(atlRef.paramIdx))) {
                    continue;
                }
                al = new gui_track_automationlane(entry, guiTracks->grid, plugin, atlRef.paramIdx);
            }
            if (atlRef.type == AUTOMATABLE_MIXER) {
                auto& mixer = track->getStage()->mixer;
                if (!assert_expr(mixer.getParam(atlRef.paramIdx))) {
                    continue;
                }
                al = new gui_track_automationlane(entry, guiTracks->grid, &track->getStage()->mixer, atlRef.paramIdx);
            }
            if (atlRef.type == AUTOMATABLE_ARP) {
                auto arp = track->getStage()->arp;
                if (!arp) {
                    continue;
                }
                if (!assert_expr(arp->getParam(atlRef.paramIdx))) {
                    continue;
                }
                al = new gui_track_automationlane(entry, guiTracks->grid, arp, atlRef.paramIdx);
            }
        } else if (stSnapshot.settings.subtrackType == gui_track_subtrack::SUBTRACK_TYPE_WAVE) {
            al = makeGuiSubtrack(entry, guiTracks->dawCtrl, stSnapshot.settings.subtrackType);
            if (entry->track && entry->track->audio)
                entry->track->audio->flags |= audiostageflags_t::CONVERT_OUTPUT | audiostageflags_t::RECORD_OUTPUT;
        }
        if (al) {
            al->height = stSnapshot.layoutSettings.height;
            guiTracks->addSubTrack(entry, al, false);
        }
    }
}

void saveTrackLayoutSettings(guictr_tracks* guiTracks, track_gui_entry_t* entry, tracklayout_settings_t& settings) {
    settings = entry->layout;
}

void loadTrackLayoutSettings(guictr_tracks* guiTracks, track_gui_entry_t* entry, const tracklayout_settings_t& settings) {
    entry->layout = settings;
}

void saveSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, track_layout_snapshot_t& snapshot) {
    snapshot.subtracks.reserve(entry->subtracks.size());
    for (auto* subtrack : entry->subtracks) {
        automatable_param_ref_t atlRef{};
        if (subtrack->subtrackType() == gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
            dbgassert(subtrack->at);
            atlRef = subtrack->at->toRef();
            atlRef.paramIdx = subtrack->param;
        }
        subtrack_snapshot_t subtrackSnapshot;
        subtrackSnapshot.settings.subtrackType = subtrack->subtrackType();
        subtrackSnapshot.layoutSettings.height = subtrack->height;
        subtrackSnapshot.atlRef = atlRef;
        snapshot.subtracks.push_back(std::move(subtrackSnapshot));
    }
}

void updateStoreLoadSubtracks(guictr_tracks* guiTracks, track_gui_entry_t* entry) {
    bool hide = entry->layout.hideSubtracks || entry->layout.hideTrack;
    if (entry->state.wasInHide == hide)
        return;
    entry->state.wasInHide = hide;
    if (hide) {
        entry->state.layoutSaved = track_layout_snapshot_t();
        saveSubtrackLayout(guiTracks, entry, entry->state.layoutSaved);
        guiTracks->removeAllSubtracks(entry);
        DAW::Cursor& cursor = entry->parentCtrl->getCursor();
        if (cursor.inSubTrackAny(entry->track->projectIdx)) {
            fixCursorSubRange(cursor, 0);
        }
    } else {
        loadSubtrackLayout(guiTracks, entry, entry->state.layoutSaved);
    }
}

audio_stage_t::~audio_stage_t() {
}

void audio_stage_t::onTick(double since) {
    meter.onTick(since);
    meterInput.onTick(since);
    for (auto effect : effects) {
        effect->onTick(since);
    }
}

void audio_stage_t::addAudioStage(audio_stage_t* _child) {
    auto it = std::find(children.begin(), children.end(), _child);
    if (it != children.end()) {
        throw applogicexception("attempt to add audio_stage_t twice");
    }
    _child->parent = this;
    this->children.push_back(_child);
}

void audio_stage_t::removeAudioStage(audio_stage_t* _child) {
    auto it = std::find(children.begin(), children.end(), _child);
    if (it == children.end()) {
        if (_child->parent == nullptr)
            return;
        throw applogicexception("attempt to remove non-present audio_stage_t");
    }
    _child->parent = nullptr;
    this->children.erase(it);
}

track_t* audio_stage_t::getTrack() const {
    const audio_stage_t* stage = this;
    while (stage) {
        if (stage->type == 0) {
            return static_cast<const track_impl_t*>(stage)->track;
        }
        stage = stage->parent;
    }
    //    dbgassert(0); // to be expected when deleting effectgroups
    return nullptr;
}

void track_impl_t::addAudio(const AudioBlock& src, float fGain) {
    const auto numChannels = math::min(src.channels, input.channels);
    for (auto channel = 0U; channel < numChannels; channel++) {
        float* pChSrc = src.buf[channel];
        float* pChDst = input.buf[channel];
        dbgassert(src.samples == input.samples);
        const int32_t nSamples = math::min(src.samples, input.samples);
        for (int sample = 0; sample < nSamples; sample++) {
            *pChDst++ += (*pChSrc++) * fGain;
        }
    }
}

void track_impl_t::fillAudio(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, const project_globals_t& prjGlobals, samplecount_t samplePosBegin, samplecount_t numSamplesDst, AudioBlock& outBuffer) {
    //TODO: make audiocache a track_impl_t constructor parameter
    auto cache = audiocache::getInstance();
    tick_t audioBegin = math::max(start, loopStart);
    tick_t audioEnd   = loopEnd < 0 ? end : math::min(end, loopEnd);
    static thread_local std::vector<clip_t*> clips;
    if (clips.capacity() == 0) {
        clips.reserve(16);
    } else {
        clips.clear();
    }
    track->getMidi().getClipsInRange(audioBegin, audioEnd, clips);
    DAW::Host::FillAudioBlockFromClips(cache, prjGlobals, clips, sampleFormat, samplePosBegin, outBuffer);
}

track_impl_t::track_impl_t(DAW::Host::PluginManager* const _host, audio_stage_id_t _id, track_t* _track, const sampleformat_t _sampleFormat, const channelnum_t _numChannels)
    : audio_stage_t(_host, _id, _sampleFormat, _numChannels, 0),
      arp(new DAW::midiarp(this)), track(_track),
      inputChannel(DAW::ChannelDefaultNone()),
      outputChannel(DAW::ChannelDefaultNone()), 
      midiValidation(new clip_notes_t())
{
}

const std::vector<DAW::arp_note_t>& track_impl_t::getArpHeldNotes() {
    dbgassert(this->arp);
    return this->arp->getHeldNotes();
}

std::vector<marker_t>& track_impl_t::getArpMarkers(int n) {
    dbgassert(this->arp);
    dbgassert(midiMutex.isLocked());
    if (n) return this->arp->markers2;
    return this->arp->markers;
}

void audio_stage_t::onStartPlayback() {
    notesPre.reset();
    notesPost.reset();
}

void track_impl_t::onStartPlayback() {
    ThreadLock lock = midiMutex.lockThread();
    audio_stage_t::onStartPlayback();
    if (arp)
        arp->onStartPlayback();
    notesPre.reset();
    notesPost.reset();
}

void track_impl_t::onStopPlayback() {
    // midiProcessed->clear();
}

void audio_stage_t::onPlaybackJumpFromTo(int32_t fromSamplePos, double fromTickPos, int32_t toSamplePos, double toTickPos) {
    notesPre.reset();
    notesPost.reset();
}

void track_impl_t::onPlaybackJumpFromTo(int32_t fromSamplePos, double fromTickPos, int32_t toSamplePos, double toTickPos) {
    audio_stage_t::onPlaybackJumpFromTo(fromSamplePos, fromTickPos, toSamplePos, toTickPos);
    // midiProcessed->clear();
}

void track_impl_t::sendNotesOff() {
    audio_stage_t::sendNotesOff();
    std::vector<midievent_note_t> noteEvents;
    {
        ThreadLock lock = midiMutex.lockThread();
        if (arp) {
            arp->allNotesOff(noteEvents);
        }
    }
}

namespace DAW::Host {
void sortNoteEvents(std::vector<midievent_note_t>& noteEvents) {
    std::sort(noteEvents.begin(), noteEvents.end(), [](const midievent_note_t& a, const midievent_note_t& b) {
        // sort by tick, pitch, note off, note on
        if (a.globalTick == b.globalTick) {
            if (a.pitch == b.pitch) {
                return !a.isNoteOn && b.isNoteOn;
            }
            return a.pitch < b.pitch;
        }
        return a.globalTick < b.globalTick;
    });
}
void sortControlEvents(std::vector<midievent_ctrl_t>& ctrlEvents) {
    std::sort(ctrlEvents.begin(), ctrlEvents.end(), [](const midievent_ctrl_t& a, const midievent_ctrl_t& b) {
        if (a.tick == b.tick) {
            if (a.midiTime == b.midiTime) {
                return a.message < b.message;
            }
            return a.midiTime < b.midiTime;
        }
        return a.tick < b.tick;
    });
}

void CopyMidiEventsInRange(tick_t absStart, tick_t absEnd, const DAW::Host::midi_data_t& data, std::vector<note_t>& list, std::vector<DAW::Host::midievent_ctrl_t>& ctrlEvts) {
    tick_t lastNoteStart = -1;
    for (auto& note : data.notes.m_list) {
        dbgassert(lastNoteStart == -1 || note.start() >= lastNoteStart);
        lastNoteStart = note.start();
        if (note.isIntersectTimeIncludeEnds(absStart, absEnd)) {
            auto noteCopy = note;
            int32_t ret = cutIntersectingNotesFindDupe(list, noteCopy);
            if (ret == -1) {
                continue;
            }
            if (ret != 0) {
                log_lf(Log::L_DEBUG, "note cut: %d\n", ret);
                continue;
            }
            if (!list.capacity()) {
                list.reserve(4);
            }
            auto it = std::find_if(list.begin(), list.end(), [&note](const auto& n) {
                return n.time > note.time;
            });
            list.insert(it, note);
        }
    }
    // for (auto& evt : data.events.m_list) {
    //     if (evt.tick >= absStart && evt.tick < absEnd) {
    //         if (!ctrlEvts.capacity()) {
    //             ctrlEvts.reserve(4);
    //         }
    //         auto it = std::find_if(ctrlEvts.begin(), ctrlEvts.end(), [&evt](const auto& n) {
    //             return n.tick > evt.tick;
    //         });
    //         ctrlEvts.insert(it, evt);
    //     }
    // }
    for (auto& ctrlEvt : data.events.m_list) {
        auto it = std::find_if(ctrlEvts.begin(), ctrlEvts.end(), [&ctrlEvt](const auto& n) {
            return n.tick > ctrlEvt.tick;
        });
        ctrlEvts.insert(it, ctrlEvt);
    }
    constexpr bool logProcessedNotes = false;
    if (logProcessedNotes && !data.notes.isEmpty()) {
        // print absStart to absEnd range we looked at
        // and print the min, max and number of events in data.notes
        log_lf(Log::L_DEBUG, "Notes in input: %zd. Notes copied: %zd\n", data.notes.m_list.size(), list.size());
        log_lf(Log::L_DEBUG, "absStart: %d, absEnd: %d\n", absStart, absEnd);
        log_lf(Log::L_DEBUG, "min note : %d, max note: %d\n", data.notes.minNote.start(), data.notes.maxNote.end());
    } 
}
} // namespace DAW::Host

/**
 * track_impl_t::sendNotes
 * Right now there is no latency compensation applied.
 * TODO: First apply latency compensation per-track. Then implement per-plugin latency compensation
 * TODO: OPTIMIZE this function. I saw up to 400x speed up in release mode
 */
void track_impl_t::processMidiInput(playback_state state, int32_t flags,
                             tick_t cursorPos,
                             tick_t blockStart, tick_t blockEnd,
                             tick_t loopStart, tick_t loopEnd,
                             project_globals_t& prjGlobals,
                             samplecount_t inputLatency,
                             const DAW::Host::midi_data_t& midiRealtimeInput) {
    using namespace DAW;
    using namespace DAW::Host;

    tmr.reset();
    static thread_local std::vector<note_t> notes;
    static thread_local std::vector<midievent_ctrl_t> ctrlEvents;
    static thread_local std::vector<midievent_note_t> noteEvents;
    if (notes.capacity() == 0) {
        notes.reserve(128);
    } else {
        notes.clear();
    }
    if (noteEvents.capacity() == 0) {
        noteEvents.reserve(128);
    } else {
        noteEvents.clear();
    }
    if (ctrlEvents.capacity() == 0) {
        ctrlEvents.reserve(128);
    } else {
        ctrlEvents.clear();
    }

    constexpr bool logProcessedNotes = false;
    const double ticksPerBlock = sampleToTickConvert<double, roundmode::none>(sampleFormat.blockSize, prjGlobals.tempo100, sampleFormat.sampleRate);

    if (flags & MidiFlags::PROCESS_CLIPS) {
        tick_t heldBegin = blockStart - 1; // -1 to include the note that ends at blockStart
        tick_t heldEnd   = blockEnd;
        track->getMidi().getEventsInRange(heldBegin, heldEnd, -1, loopEnd, notes, ctrlEvents);
        //TODO: make feeding parent tracks notes into this one an option
        //auto getParent = track->parent;
        //while (getParent) {
        //    getParent->getMidi().getNotesInRange(heldBegin, heldEnd, -1, loopEnd, notes);
        //    getParent = getParent->parent;
        //}

#ifdef DAW_DEBUG_MIDI_PROCESSING
        tick_t lastNoteStart = -1;
        for (note_t& note : notes) {
            if (!(lastNoteStart == -1 || note.start() >= lastNoteStart)) {
                log_lf(Log::L_WARN, "Track clip notes are note sorted by time\n");
            }
            lastNoteStart = note.start();
        }
#endif
    }

    updateProfilingTime(procMidiStats.tm0InputClips, tmr.getTimeReset());

    if ((flags & MidiFlags::PROCESS_REALTIME) 
                                && (midiChannel.getType() == midistage_type::INPUT_EXTERNAL_MIDI
                                    || (this->midiChannel.getType() == midistage_type::INPUT_DEFAULT 
                                        && isSet(this->flags, audiostageflags_t::RECORD_ARMED)))) {
        CopyMidiEventsInRange(blockStart, blockEnd, midiRealtimeInput, notes, ctrlEvents);
    }

    updateProfilingTime(procMidiStats.tm1InputRT, tmr.getTimeReset());
    bool bRequiresProcessing = !notes.empty() || !ctrlEvents.empty() || !m_heldNotes.empty() || arp;
    bRequiresProcessing = true;
    if (bRequiresProcessing) {
        ThreadLock lock = midiMutex.lockThread();

        tick_t blockLoopStart = loopStart > -1 && blockStart < loopStart ? loopStart : blockStart;
        tick_t blockLoopEnd   = loopEnd > -1 && blockEnd > loopEnd ? loopEnd : blockEnd;

        constexpr bool chaseNotes = true;
        for (note_t& note : notes) {
            // Find beginning notes
            if (note.start() >= blockLoopStart && note.start() < blockLoopEnd) {
                auto it = std::find_if( m_heldNotes.begin(), m_heldNotes.end(), [&note](const note_t& held) {
                    return held.pitch == note.pitch;
                });
                if (it != m_heldNotes.end()) {
                    log_lf(Log::L_WARN, "Block %d-%d: %s ALREADY HELD at %d (abs time: %d len: %d)\n", blockStart, blockEnd, noteName(note.pitch), note.start() - blockStart, note.time, note.len);
                } else {
                    if (logProcessedNotes) {
                        log_lf(Log::L_DEBUG, "Block %d-%d: %s ON at %d (abs time: %d len: %d)\n", blockStart, blockEnd, noteName(note.pitch), note.start() - blockStart, note.time, note.len);
                    }
                    InsertMidiEventSorted(noteEvents, {note.pitch, note.velocity, note.start() - blockStart, note.start(), true, false});
                    m_heldNotes.push_back(note);
                }
            } else if (chaseNotes && note.start() < blockLoopStart && note.end() > blockLoopStart) {
                auto it = std::find_if( m_heldNotes.begin(), m_heldNotes.end(), [&note](const note_t& held) {
                    return held.pitch == note.pitch;
                });
                // check in in heldNotes
                if (it == m_heldNotes.end()) {
                    tick_t minStartTime = blockStart;
                    for (const auto& evt : noteEvents) {
                        if (!evt.isNoteOn && evt.pitch  == note.pitch) {
                            minStartTime = evt.globalTick;
                        }
                    }
                    if (logProcessedNotes)
                        log_lf(Log::L_DEBUG, "Block %d-%d: %s CHASE ON at %d (abs time: %d len: %d)\n", blockStart, blockEnd, noteName(note.pitch), minStartTime-blockStart, note.time, note.len);
                    InsertMidiEventSorted(noteEvents, {note.pitch, note.velocity, minStartTime-blockStart, minStartTime, true, false});
                    m_heldNotes.push_back(note);
                }
            }
            // Find ending notes
            if (note.end() >= blockLoopStart && note.end() < blockLoopEnd) {
                if (removeEntry(m_heldNotes, note)) {
                    if (logProcessedNotes)
                        log_lf(Log::L_DEBUG, "Block %d-%d: %s OFF at %d/%f (abs time: %d len: %d)\n", blockStart, blockEnd, noteName(note.pitch), note.end() - blockStart, ticksPerBlock, note.time, note.len);
                    auto tickOffsetInBlock = note.end() - blockStart;
                    dbgassert(tickOffsetInBlock >= 0);
                    InsertMidiEventSorted(noteEvents, {note.pitch, note.velocity, tickOffsetInBlock, note.end(), false, false});
                } else if (note.end() > blockLoopStart) {
                    log_lf(Log::L_WARN, "Block %d-%d: %s OFF WAS NOT HELD at %d/%f (abs time: %d len: %d) \n", blockStart, blockEnd, noteName(note.pitch), note.end() - blockStart, ticksPerBlock, note.time, note.len);
                }
            }
        }

        updateProfilingTime(procMidiStats.tm2ProcNotes, tmr.getTimeReset());

        // force end notes at loop end boundary
        if (loopEnd > 0 && blockStart < loopEnd && blockEnd >= loopEnd) {
            for (auto it = m_heldNotes.begin(); it != m_heldNotes.end();) {
                const note_t& noteHeld = *it;

                auto tickOffsetInBlockEnd = math::min(blockEnd - blockStart - 1, loopEnd - blockStart - 1);
                if (logProcessedNotes)
                    log_lf(Log::L_INFO, "Block %d-%d: %s Force OFF (LOOP END @%d) at %d/%f = %d\n", blockStart, blockEnd, noteName(noteHeld.pitch), loopEnd, tickOffsetInBlockEnd, ticksPerBlock, blockStart + tickOffsetInBlockEnd);
                InsertMidiEventSorted(noteEvents, {noteHeld.pitch, noteHeld.velocity, tickOffsetInBlockEnd, blockStart + tickOffsetInBlockEnd, false, true});
                it = m_heldNotes.erase(it);
            }
        }

        // revalidate held notes ends so we end notes that were modified by the user (loop or clip modifactions)
        // this also cuts of notes on clip looparounds
        for (auto it = m_heldNotes.begin(); it != m_heldNotes.end();) {
            const note_t& noteHeld = *it;
            bool found             = false;
            for (note_t& note : notes) {
                if (note.pitch != noteHeld.pitch) {
                    continue;
                }
                if (note.start() < blockLoopEnd && note.end() >= blockEnd) {
                    found |= true;
                    break;
                }
            }
            if (!found) {
                if (logProcessedNotes)
                    log_lf(Log::L_INFO, "Block %d-%d: %s Force OFF at %d\n", blockStart, blockEnd, noteName(noteHeld.pitch), blockEnd - 1);
                InsertMidiEventSorted(noteEvents, {noteHeld.pitch, noteHeld.velocity, math::floordS32(ticksPerBlock) - 1, blockEnd - 1, false, false});
                it = m_heldNotes.erase(it);
                continue;
            }
            ++it;
        }

#ifdef DAW_DEBUG_MIDI_PROCESSING
        noteEventValidator.validate(noteEvents);

        tick_t lastNoteStart = -1;
        tick_t tickOffsetInBlock = -1;
        for (auto& event : noteEvents) {
            dbgassert(lastNoteStart == -1 || event.globalTick >= lastNoteStart);
            lastNoteStart = event.globalTick;
            dbgassert(tickOffsetInBlock == -1 || event.tickOffsetInBlock >= tickOffsetInBlock);
            tickOffsetInBlock = event.tickOffsetInBlock;
        }
#endif
        updateProfilingTime(procMidiStats.tm3RevalidateEnds, tmr.getTimeReset());

        notesPre.update(blockStart, noteEvents, ctrlEvents);
        updateProfilingTime(procMidiStats.tm4SortEvents, tmr.getTimeReset());
        
        if (midiChannel.type == midistage_type::INPUT_AUDIOSTAGE) {
            auto* stageMidiInput = host->getAudioStage(midiChannel.stage.stageRef);
            if (stageMidiInput) {
                noteEvents.clear();
                ctrlEvents.clear();
                stageMidiInput->getNotesDelayed(blockStart, ticksPerBlock, noteEvents, ctrlEvents, midiChannel.stage.buffer != stage_bufferpoint::INPUT);
            }
        }
        if (isSet(this->flags, audiostageflags_t::RECORD_ARMED)) {
            recorder.recordNoteEvents(state, blockStart, blockEnd, noteEvents);
        }

        tmr.reset();
        this->noteEventsProcessed.clear();
        if (arp && (flags & MidiFlags::PROCESS_ARP)) {
            arp->process(host, state, cursorPos, noteEvents, blockStart, blockEnd, loopStart, loopEnd, noteEventsProcessed);
        } else {
            noteEventsProcessed = /* std::move */(noteEvents);
        }
        updateProfilingTime(procMidiStats.tm5ProcArp, tmr.getTimeReset());

        notesPost.update(blockStart, noteEventsProcessed, ctrlEvents);
        updateProfilingTime(procMidiStats.tm6UpdateOutputPost, tmr.getTimeReset());

#ifdef DAW_DEBUG_MIDI_PROCESSING
        std::vector<midievent_note_t> noteEventsPostValidate;
        std::vector<midievent_ctrl_t> ctrlEventsPostValidate;
        notesPost.getNotesDelayed(blockStart, ticksPerBlock, noteEventsPostValidate, ctrlEventsPostValidate);
        noteEventValidatorPost.validate(noteEventsPostValidate);
        if (logProcessedNotes && !midiRealtimeInput.notes.isEmpty()) {
            log_lf(Log::L_DEBUG, "Realtime i: %zd. Notes: %zd. Events Processed: %zd. Post Event Buffered: %zd\n",
                                midiRealtimeInput.notes.m_list.size(),
                                notes.size(),
                                noteEventsProcessed.size(),
                                notesPost.noteEvts.size());
        }
#endif
    }

#ifdef DAW_DEBUG_MIDI_PROCESSING
    validateProcessedMidi(state, flags, blockStart, blockEnd, loopStart, loopEnd, prjGlobals, inputLatency);
    updateProfilingTime(procMidiStats.tm7ValidateMidi, tmr.getTimeReset());
#endif

    this->noteEventsProcessed.clear();
}

void audio_stage_t::getNotesDelayed(tick_t tickLatencyCompensated, const double ticksPerBlock, std::vector<midievent_note_t>& evtsOut, std::vector<DAW::Host::midievent_ctrl_t>& ctrlEvts, bool isPost) {
    auto& notesStage = isPost ? notesPost : notesPre;
    notesStage.getNotesDelayed(tickLatencyCompensated, ticksPerBlock, evtsOut, ctrlEvts);
}
void audio_stage_t::sendMidiToEffect(const std::vector<midievent_note_t>& evtsOut, const std::vector<DAW::Host::midievent_ctrl_t>& ctrlEvts, tick_t tickLatencyCompensated, int32_t bpm100, effectbase* effect) {
    midi_data_processing_t events{ &evtsOut, &ctrlEvts, tickLatencyCompensated, bpm100 };
    effect->processMidi(events);
}

void track_impl_t::validateProcessedMidi(playback_state state, int32_t flags, tick_t blockStart, tick_t blockEnd, tick_t loopStart, tick_t loopEnd, project_globals_t& prjGlobals, samplecount_t inputLatency) {
    constexpr bool logProcessedNotes = false;
    bool notesProcessed              = false;
    if (!noteEventsProcessed.empty()) {

        std::vector<note_t> newNotes;
        for (midievent_note_t& msg : noteEventsProcessed) {
            if (msg.isNoteOn) {
                note_t note;
                //                note.setRealtime(false);
                note.setIsHeld(true);
                note.time     = blockStart + msg.tickOffsetInBlock;
                note.len      = lenTicksInfinite;
                note.pitch    = msg.pitch;
                note.velocity = msg.velocity;
                newNotes.push_back(note);
            }
        }
        if (!newNotes.empty()) {
            if (logProcessedNotes)
                for (auto& note : newNotes) {
                    log_lf(Log::L_DEBUG, "Block %d, note open %d (%s)\n", blockStart, note.start(), noteName(note.pitch));
                }
            midiValidation->addAll(newNotes);
            notesProcessed = true;
        }
        for (midievent_note_t& msg : noteEventsProcessed) {
            if (!msg.isNoteOn) {
                int32_t pitch   = msg.pitch;
                int32_t tickEnd = blockStart + msg.tickOffsetInBlock;
                if (logProcessedNotes)
                    log_lf(Log::L_DEBUG, "%s@%d Looking for NOTE_ON evt\n", noteName(pitch), tickEnd);
                bool fnd = false;
                for (note_t& noteHeld : midiValidation->m_list) {
                    if (noteHeld.pitch == pitch) {
                        if (!noteHeld.isHeld()) {
                            //log_lf(Log::L_WARN, "%s@%d note was released before (@%d), looking for next one\n", noteName(noteHeld.pitch), noteHeld.start(), noteHeld.end());
                            continue;
                        }
                        if (noteHeld.start() > tickEnd) {
                            //log_lf(Log::L_WARN, "%s@%d note starts after this release\n", noteName(noteHeld.pitch), noteHeld.start());
                            continue;
                        }
                        if (noteHeld.start() == tickEnd) {
                            //log_lf(Log::L_WARN, "%s noteHeld.start() == tickEnd %d, adding TICKS_16TH/4\n", noteName(noteHeld.pitch), tickEnd);
                            tickEnd += TICKS_16TH / 4;
                        }
                        noteHeld.len = tickEnd - noteHeld.start();
                        noteHeld.setIsHeld(false);
                        dbgassert(noteHeld.len >= 0);
                        fnd            = true;
                        notesProcessed = true;
                        if (logProcessedNotes)
                            log_lf(Log::L_DEBUG, "Block %d, note complete %d END %d (%s)\n", blockStart, noteHeld.start(), noteHeld.end(), noteName(noteHeld.pitch));
                        break;
                    }
                }
                if (!fnd) {
                    log_lf(Log::L_WARN, "MIDI_OFF_NOTE note not found %s tickEnd %d. midiProcessed size %zd\n", noteName(pitch), tickEnd, midiValidation->m_list.size());
                }
            }
        }
        //if (newNotes.size() || notesProcessed) {
        //    midiProcessed->removeDuplicates();
        //    notesProcessed = true;
        //}
    }
    if (!midiValidation->m_list.empty()) {

        // TODO: make this debug only
        {
            for (note_t& n : midiValidation->m_list) {
                if (n.isHeld())
                    continue;
                int exactDupes = 0;
                for (note_t& c : midiValidation->m_list) {
                    if (c.isHeld())
                        continue;
                    if (c.pitch == n.pitch) {
                        if (c == n && exactDupes == 0) {
                            exactDupes++;
                            continue;
                        }
                        if (c.start() >= n.end() || c.end() <= n.start()) {
                            continue;
                        }
                        log_lf(Log::L_WARN, "Found notes overlapping (%s@%d-%d and @%d-%d)\n", noteName(n.pitch), n.start(), n.end(), c.start(), c.end());
                    }
                }
            }
        }

        auto it = midiValidation->m_list.begin();
        while (it != midiValidation->m_list.end()) {
            note_t& note = *it;
            if (!note.isHeld() && note.end() < blockStart) {
                if (logProcessedNotes) {
                    String strTmStart = tickAsBeatString(note.start(), false);
                    String strTmEnd   = tickAsBeatString(note.end(), false);
                    log_lf(Log::L_DEBUG, "Note %s recorded from %s to %s\n", noteName(note.pitch), StringAsCStr(strTmStart), StringAsCStr(strTmEnd));
                    log_lf(Log::L_DEBUG, "Note %s recorded from %d to %d\n", noteName(note.pitch), note.start(), note.end());
                }
                notesProcessed = true;
                it = midiValidation->m_list.erase(it);
            } else {
                it++;
            }
        }
    }
    if (notesProcessed) {
        midiValidation->updateBounds();
    }
}

void track_params_t::createSnapshot(track_params_snapshot_t& snapshot, const tracksnapshot_store_opts_t& opts) {
    if (opts.storePluginPreset) {
        snapshot.params.reserve(getNumParameters());
        visitParams([&snapshot](auto& mapEntry) {
            automatable_param_t& param = mapEntry.second;
            snapshot.params.push_back(param_snapshot_t{ param.idx, param.getValue(), param.inUse ? 1 : 0 });
        });
    }
    if (opts.storeAutomation) {
        storeAutomation(snapshot.automatedParams, this);
    }
}

void track_params_t::loadSnapshot(const track_params_snapshot_t& snapshot) {
    for (const auto& param : snapshot.params) {
        dbgassert(getParam(param.idx));
        int flags = FLG_PAR_UPDATE_INIT;
        if (!param.flags) {
            flags |= FLG_PAR_UPDATE_NOSTORE;
        }
        setParamValue(param.idx, param.val, flags);
    }
    loadAutomation(snapshot.automatedParams, this);
}

void track_params_t::postSetParameter(int32_t idx, float preVal, float val, int flags) {
    if (flags & FLG_PAR_UPDATE_FINISH) {
        dbgassert(this->audiostage->getTrack());
        automatable_param_ref_t ref = toRef();

        track_t* track    = this->audiostage->getTrack();
        parameter_ref_t p = { track->projectIdx, ref.type, 0, idx };
        DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
    }
}

track_params_t::track_params_t(audio_stage_t* _audiostage) : automatable_t(), audiostage(_audiostage) {
    const std::array<track_param_entry_t, 3> parameterTypes{ {
            { PARAM_ENABLE, "Enabled", "", 1.0f },
            { PARAM_TRACK_GAIN, "Gain", "dB", dsp_util::gainToLinScale(1.0f) },
            { PARAM_TRACK_PAN, "Pan", "", 0.5f },
    } };
    for (const auto& paramEntry : parameterTypes) {
        registerParam(paramEntry.id)->initValue(paramEntry);
    }
    getParam(PARAM_TRACK_PAN)->isBiPolar = true;
    for (int i = 0; i < MAX_SEND_CHANNELS; i++) {
        automatable_param_t* regparam = registerParam(PARAM_OFFSET_SEND_GAIN + i);
        regparam->setInitial(0.0f);
        regparam->name  = StringFormat("Send %d Gain", (i + 1));
        regparam->shortLabel  = "Gain";
        regparam->unit  = "dB";
        automatable_param_t* regparamPan = registerParam(PARAM_OFFSET_SEND_PAN + i);
        regparamPan->setInitial(0.5f);
        regparamPan->name  = StringFormat("Send %d Pan", (i + 1));
        regparamPan->shortLabel  = "Pan";
        regparamPan->unit  = "dB";
        regparamPan->isBiPolar = true;
    }
    getParam(PARAM_ENABLE)->quantizationSteps = 1;
}

track_t* track_params_t::getTrack() {
    return audiostage->getTrack();
}

automatable_param_ref_t track_params_t::toRef() const {
    automatable_param_ref_t ref;
    ref.type  = AUTOMATABLE_MIXER;
    ref.refId = static_cast<int32_t>(audiostage->stageId.stageId);
    return ref;
}

namespace DAW {
    void assignFreeStageIds(Host::PluginManager* host, plugin_snapshot_t& snapshot) {
        std::map<int32_t,int32_t> idMap;
        std::map<int32_t,int32_t> pluginIdMap;
        std::vector<plugin_snapshot_t*> all;
        std::vector<plugin_snapshot_t*> q;
        q.push_back(&snapshot);
        while (!q.empty()) {
            plugin_snapshot_t* s = q.back();
            q.pop_back();
            all.push_back(s);
            if (s->projectGlobalId) {
                auto pluginId = host->getNextGlobalModuleId(0);
                log_lf(Log::L_DEBUG, "projectGlobalId %d is in use, assigning new id %d\n", s->projectGlobalId, pluginId);
                pluginIdMap[s->projectGlobalId] = pluginId;
                s->projectGlobalId = pluginId;
            }
            if (host->isStageIdInUse(s->stageIds)) {
                auto stageId = host->getNextGlobalAudioStageId(0);
                log_lf(Log::L_DEBUG, "stageId %d is in use, assigning new id %d\n", s->stageIds.stageId, static_cast<int32_t>(stageId.stageId));
                idMap[s->stageIds.stageId] = static_cast<int32_t>(stageId.stageId);
                idMap[s->stageIds.inputStageId] = static_cast<int32_t>(stageId.inputStageId);
                idMap[s->stageIds.outputStageId] = static_cast<int32_t>(stageId.outputStageId);
                idMap[s->stageIds.outputPostStageId] = static_cast<int32_t>(stageId.outputPostStageId);
                s->stageIds = saveTrackIdSnapshot(stageId);
            }
            for (auto& child : s->pluginSnapshots) {
                q.push_back(&child);
            }
        }

        const auto getNewStageId = [&idMap](int32_t stageId) -> int32_t {
            if (idMap.find(stageId) != idMap.end()) {
                log_lf(Log::L_DEBUG, "updating referenced to stage %d: now points at %d\n", stageId, idMap[stageId]);
                return idMap[stageId];
            }
            return stageId;
        };
        const auto getNewPluginId = [&pluginIdMap](int32_t pluginId) -> int32_t {
            if (pluginIdMap.find(pluginId) != pluginIdMap.end()) {
                log_lf(Log::L_DEBUG, "updating referenced to plugin %d: now points at %d\n", pluginId, pluginIdMap[pluginId]);
                return pluginIdMap[pluginId];
            }
            return pluginId;
        };
        for (auto* s : all) {
            for (auto& r : s->effectRouting.inputRoutingOutputStage) {
                r.stageId = getNewStageId(r.stageId);
                r.projectGlobalId = getNewPluginId(r.projectGlobalId);
            }
            std::map<int32_t, std::vector<io_configuration_snapshot_t>> inputRoutingEffects;
            for (auto& reff : s->effectRouting.inputRoutingEffects) {
                auto key = getNewPluginId(reff.first);
                auto copyVals = reff.second;
                for (auto& r : copyVals) {
                    r.stageId = getNewStageId(r.stageId);
                    r.projectGlobalId = getNewPluginId(r.projectGlobalId);
                }
                inputRoutingEffects[key] = copyVals;
            }
            s->effectRouting.inputRoutingEffects = inputRoutingEffects;
        }
    }

    void assignFreeStageIdsTrackSnapshot(Host::PluginManager* host, track_snapshot_t& snapshot) {
        std::map<int32_t,int32_t> idMap;
        std::map<int32_t,int32_t> pluginIdMap;
        std::vector<track_effect_routing_snapshot_t*> all;
        std::vector<track_modulation_routing_snapshot_t*> allModulation;
        std::vector<plugin_snapshot_t*> q;

        // if (host->isStageIdInUse(snapshot.stageIds))
        {
            auto stageId = host->getNextGlobalAudioStageId(0);
            log_lf(Log::L_DEBUG, "stageId %d is in use, assigning new id %d\n", snapshot.stageIds.stageId, static_cast<int32_t>(stageId.stageId));
            idMap[snapshot.stageIds.stageId] = static_cast<int32_t>(stageId.stageId);
            idMap[snapshot.stageIds.inputStageId] = static_cast<int32_t>(stageId.inputStageId);
            idMap[snapshot.stageIds.outputStageId] = static_cast<int32_t>(stageId.outputStageId);
            idMap[snapshot.stageIds.outputPostStageId] = static_cast<int32_t>(stageId.outputPostStageId);
            snapshot.stageIds = saveTrackIdSnapshot(stageId);
            all.push_back(&snapshot.data.effectRouting);
            allModulation.push_back(&snapshot.data.modulationRouting);
        }

        q.reserve(snapshot.data.pluginSnapshots.size());
        for (auto& plugin : snapshot.data.pluginSnapshots) {
            q.push_back(&plugin);
        }

        while (!q.empty()) {
            plugin_snapshot_t* s = q.back();
            q.pop_back();
            all.push_back(&s->effectRouting);
            allModulation.push_back(&s->modulationRouting);
            if (s->projectGlobalId) {
                auto pluginId = host->getNextGlobalModuleId(0);
                log_lf(Log::L_DEBUG, "projectGlobalId %d is in use, assigning new id %d\n", s->projectGlobalId, pluginId);
                pluginIdMap[s->projectGlobalId] = pluginId;
                s->projectGlobalId = pluginId;
            }
            if (host->isStageIdInUse(s->stageIds)) {
                auto stageId = host->getNextGlobalAudioStageId(0);
                log_lf(Log::L_DEBUG, "stageId %d is in use, assigning new id %d\n", s->stageIds.stageId, static_cast<int32_t>(stageId.stageId));
                idMap[s->stageIds.stageId] = static_cast<int32_t>(stageId.stageId);
                idMap[s->stageIds.inputStageId] = static_cast<int32_t>(stageId.inputStageId);
                idMap[s->stageIds.outputStageId] = static_cast<int32_t>(stageId.outputStageId);
                idMap[s->stageIds.outputPostStageId] = static_cast<int32_t>(stageId.outputPostStageId);
                s->stageIds = saveTrackIdSnapshot(stageId);
            }
            for (auto& child : s->pluginSnapshots) {
                q.push_back(&child);
            }
        }

        const auto getNewStageId = [&idMap](int32_t stageId) -> int32_t {
            if (idMap.find(stageId) != idMap.end()) {
                log_lf(Log::L_DEBUG, "updating referenced to stage %d: now points at %d\n", stageId, idMap[stageId]);
                return idMap[stageId];
            }
            return stageId;
        };
        const auto getNewPluginId = [&pluginIdMap](int32_t pluginId) -> int32_t {
            if (pluginIdMap.find(pluginId) != pluginIdMap.end()) {
                log_lf(Log::L_DEBUG, "updating referenced to plugin %d: now points at %d\n", pluginId, pluginIdMap[pluginId]);
                return pluginIdMap[pluginId];
            }
            return pluginId;
        };
        for (auto* effectRouting : all) {
            for (auto& r : effectRouting->inputRoutingOutputStage) {
                r.stageId = getNewStageId(r.stageId);
                r.projectGlobalId = getNewPluginId(r.projectGlobalId);
            }
            std::map<int32_t, std::vector<io_configuration_snapshot_t>> inputRoutingEffects;
            for (auto& reff : effectRouting->inputRoutingEffects) {
                auto key = getNewPluginId(reff.first);
                auto copyVals = reff.second;
                for (auto& r : copyVals) {
                    r.stageId = getNewStageId(r.stageId);
                    r.projectGlobalId = getNewPluginId(r.projectGlobalId);
                }
                inputRoutingEffects[key] = copyVals;
            }
            effectRouting->inputRoutingEffects = std::move(inputRoutingEffects);
        }
        for (auto* modulationRouting : allModulation) {
            for (modulation_channel_ref& r : modulationRouting->arp) {
                r.refSrc.refId = getNewPluginId(r.refSrc.refId);
            }
            for (modulation_channel_ref& r : modulationRouting->mixer) {
                r.refSrc.refId = getNewPluginId(r.refSrc.refId);
            }
            std::map<int32_t, std::vector<DAW::modulation_channel_ref>> effectMods;
            for (auto& reff : modulationRouting->effectMods) {
                auto key = getNewPluginId(reff.first);
                auto copyVals = reff.second;
                for (auto& r : copyVals) {
                    r.refSrc.refId = getNewPluginId(r.refSrc.refId);
                }
                effectMods[key] = copyVals;
            }
            modulationRouting->effectMods = std::move(effectMods);
        }
    }

    automatable_t* resolveAutomatableRefDevice(const Host::PluginManager* const host, const automatable_param_ref_t& ref) {
        dbgassert(ref.type != AUTOMATABLE_MODULATOR_OUTPUT);
        if (ref.type == AUTOMATABLE_EFFECT) {
            return host->getPluginById(ref.refId);
        }
        if (ref.type == AUTOMATABLE_MIXER) {
            auto stage = host->getAudioStage(AudioStageRefFromId(ref.refId));
            if (stage) {
                return &stage->mixer;
            }
        }
        if (ref.type == AUTOMATABLE_ARP) {
            auto stage = host->getAudioStage(AudioStageRefFromId(ref.refId));
            if (stage->getTrack() && stage->getTrack()->getStage()) {
                auto trImpl = stage->getTrack()->getStage();
                if (trImpl && trImpl->arp) {
                    return trImpl->arp;
                }
            }
        }
        return nullptr;
    }

    const automated_param_t* ResolveModulationChannel(const Host::PluginManager* const host, const DAW::modulation_channel_ref& modChannel) {
        if (modChannel.refSrc.type == AUTOMATABLE_MODULATOR_OUTPUT) {
            auto effBase = host->getPluginById(modChannel.refSrc.refId);
            if (!effBase || !effBase->hasAutomationModulationOutput()) {
                return nullptr;
            }
            if (!effBase->hasAutomationModulationOutput()) {
                return nullptr;
            }
    #ifndef NDEBUG
            auto effMod = dynamic_cast<internal_modulator*>(effBase);
            if (!assert_expr(effMod))
                return nullptr;
    #else
            auto effMod = static_cast<internal_modulator*>(effBase);
    #endif
            auto p = effMod->getModulationOutputData(modChannel);
            if (!assert_expr(effMod))
                return nullptr;
            return p;
        }
        dbgassert(0);
        return nullptr;
    }
    void ConnectModulationInputChannel(automatable_t* dev, int32_t paramIdx, modulation_channel_ref modChannel, const modulation_scaling_t& scale) {
        // int32_t numModulations = 0;
        if (dev->isParamModulated(paramIdx)) {
            auto* pModulations = dev->getModulations(paramIdx);
            for (auto mod : *pModulations) {
                if (mod->refSrc == modChannel.refSrc) {
                    return;
                }
            }
            // numModulations = CtrSize(inputs);
        }
        auto inputRef = modChannel;
        inputRef.paramIdxDst = paramIdx;
        inputRef.refSrc = modChannel.refSrc;
        inputRef.scale = scale;

        if (inputRef.scale.mode == ModulationMode::BYPASS) {
            inputRef.scale.mode = ModulationMode::MUL;
        }

        dev->getModulations().push_back(inputRef);
        dev->updateModulationMap();
    }
    void DisonnectModulationForParam(automatable_t* dev, int32_t paramIdx) {
        auto& inputs = dev->getModulations();
        for (int i = 0; i < CtrSize(inputs); i++) {
            if (inputs[i].paramIdxDst == paramIdx) {
                inputs.erase(inputs.begin() + i);
            }
        }
        dev->updateModulationMap();
    }
    void DisonnectModulationInputChannel(automatable_t* dev, DAW::modulation_channel_ref modChannel) {
        auto& inputs = dev->getModulations();
        for (int i = 0; i < CtrSize(inputs); i++) {
            if (inputs[i].refSrc == modChannel.refSrc) {
                inputs.erase(inputs.begin() + i);
            }
        }
        dev->updateModulationMap();
    }
}


const char* trackTypeNames[5] = {
    "Master", "Return", "Midi", "Audio", NULL
};
const char* TrackTypeToName(int type) {
    return trackTypeNames[type];
}

//vFILE_TYPES_TRACKSNAPSHOT
//const SupportedFileType FILE_TYPE_TRACKSNAPSHOT;

const SupportedFileType FILE_TYPE_TRACKSNAPSHOT{ "Track File", "tracks" };
const std::vector<SupportedFileType> vFILE_TYPES_TRACKSNAPSHOT = { FILE_TYPE_TRACKSNAPSHOT };
const SupportedFileType FILE_TYPE_PLUGINSNAPSHOT{ "Plugin Preset File", "preset" };
std::vector<SupportedFileType> vFILE_TYPE_PLUGINSNAPSHOT = { FILE_TYPE_PLUGINSNAPSHOT };

bool storePluginPresetWithSnapshot = true;
bool loadPluginPresetWithSnapshot  = false;
bool clip_recorder::writeRecordedData(project_controller_t* projCtrl, track_impl_t* trImpl, audiocache* cache, DawInstance* daw) {
    if (this->hasNewRecordedData) {
        ThreadLock lock = daw && daw->getMainControl() ? daw->getMainControl()->lockPlayThread() : ThreadLock::MakeVoidLock();
        this->hasNewRecordedData = false;
        if (recordDataProcessed && recordDataProcessed->getLen() > 0) {
            clip_t* pClip = nullptr;
            std::swap(recordDataProcessed, pClip);
            auto tr = trImpl->getTrack();
            if (tr) {
                bool update = true;
                if (pClip->clipType == CLIP_AUDIO) {
                    update = false;
                    if (audioSampleId < 0) {
                        create_sample_req_t ssr;
                        ssr.format      = trImpl->sampleFormat;
                        ssr.id          = -1;
                        ssr.numChannels = trImpl->input.channels;
                        ssr.preAllocate = 1024L*64;
                        auto [fPath, fName] = daw->createUniqueNonExistingFilename("recorded", trImpl->track ? trImpl->track->name : "", "Recorded", "wav");
                        ssr.path = fPath;
                        // createSample is not thread safe, we might be doing a lookup from waveformrenderer
                        auto file = cache->createSample(ssr);
                        audioSampleId = file->id;
                        pClip->name = fName;
                        pClip->audio.id = audioSampleId;
                        pClip->rgb = tr->rgb;
                    }
                    if (samplesRecorded >= trImpl->sampleFormat.sampleRate>>2 && audioSampleId >= 0) {
                        auto* file = cache->get(audioSampleId);
                        if (file) {
                            update = true;
                            ssr.format = trImpl->sampleFormat;
                            ssr.id = audioSampleId;
                            ssr.length = samplesRecorded;
                            ssr.offset = samplesWritten;
                            auto nSamplesRead = trImpl->audioInput.readSamples(firstRecordedSample+ssr.offset,
                                                            ssr.length,
                                                            trImpl->input.channels,
                                                            ssr.channels);
                            dbgassert(nSamplesRead <= ssr.length);
                            if (nSamplesRead > 0) {
                                ssr.length = nSamplesRead;
                                ssr.preAllocate = 1024L*64;
                                cache->updateSample(ssr);
                                samplesWritten += samplesRecorded;
                                samplesRecorded = 0;
                                pClip->name = file->name;
                                pClip->audio.id = audioSampleId;
                                pClip->rgb = tr->rgb;
                            }
                        } else {
                            // sample wen't offline
                            audioSampleId = -1;
                            isRecording = false;
                        }
                    }
                }
                if (!this->isRecording) {
                    samplesWritten = 0;
                    samplesRecorded = 0;
                    audioSampleId = -1;
                }
                if (!update) {
                    delete pClip;
                    return false;
                }
                // log_lf(Log::L_DEBUG, "Processing recorded clip. Recorded %zu notes\n", pClip->notes.m_list.size());
                // log_lf(Log::L_DEBUG, "Processing recorded clip. Last note time %d\n", pClip->notes.lastNote.time);
                tick_t tickBegin = pClip->time;
                tick_t tickEnd   = pClip->end();
                daw->cutIntersecting(tr, tickBegin, tickEnd);
                pClip->setDirty();
                pClip->notes.updateBounds();
                tr->getMidi().addClip(pClip);
                tr->getMidi().sortClips();
                return true;
            } else {
                delete pClip;
            }
        }
    }
    return false;
}
void clip_recorder::finishRecordingClip(samplecount_t samplePosBlockStart, samplecount_t samplePosBlockEnd, tick_t tickPosBlockStart, tick_t tickBlockEnd, const std::vector<note_t>& m_list) {
    for (auto& note : m_list) {
        note_t noteCopy = note;
        if (noteCopy.time < tickPosBlockStart && noteCopy.isHeld()) {
            noteCopy.len = tickPosBlockStart - noteCopy.time;
            noteCopy.setIsHeld(false);
        }
        if (noteCopy.len > 0 && !noteCopy.isHeld()) {
            noteCopy.time -= recordingClip->start();
            noteCopy.setEnabled(true);
            noteCopy.setRealtime(false);
            recordingClip->notes.addSingle(noteCopy);
        }
    }
    clip_t* cloned = recordingClip->clone();
    cloned->loopEnabled = false;
    cloned->loopLen = ((math::max(1, cloned->getLen() / (TICKS_BAR * 4))) * (TICKS_BAR * 4));
    cloned->notes.updateBounds();
    cloned->setDirty();
    std::swap(recordDataProcessed, cloned);
    delete cloned;
    isRecording = false;
    hasNewRecordedData = true;
    delete recordingClip;
    recordingClip = nullptr;
}
void clip_recorder::updateRecordingClip(samplecount_t samplePosBlockStart, samplecount_t samplePosBlockEnd, tick_t tickPosBlockStart, tick_t tickBlockEnd, int trackType, const std::vector<note_t>& m_list) {
    if (recordingClip == nullptr) {
        isRecording = true;
        firstRecordedSample = samplePosBlockStart;
        samplesRecorded = 0;
        recordingClip       = new clip_t;
        recordingClip->name = "Recorded";
        recordingClip->time = tickPosBlockStart;
        recordingClip->setLen(5);
        recordingClip->loopStart = 0;
        recordingClip->loopLen   = TICKS_BAR * 4;
        recordingClip->clipType = trackType == TRACK_TYPE_AUDIO ? CLIP_AUDIO : CLIP_MIDI;
    }

    if (recordingClip) {
        if (recordingClip->start() > tickPosBlockStart) {
            recordingClip->time = tickPosBlockStart;
        }
        if (recordingClip->end() < tickBlockEnd) {
            recordingClip->setLen((tickBlockEnd) -recordingClip->start());
        }
        if (recordingClip->lenSamples < samplePosBlockEnd - firstRecordedSample) {
            recordingClip->lenSamples = samplePosBlockEnd - firstRecordedSample;
        }
    }
    if (recordingClip && tickBlockEnd - recordingClip->time > TICKS_QUARTER) {
        for (auto& note : m_list) {
            if (!note.isHeld()) {
                auto noteCopy = note;
                noteCopy.time -= recordingClip->start();
                noteCopy.setEnabled(true);
                noteCopy.setRealtime(false);
                recordingClip->notes.addSingle(noteCopy);
            }
        }
        clip_t* cloned = recordingClip->clone();
        cloned->lenSamples = samplePosBlockEnd - firstRecordedSample;
        cloned->setLen(tickBlockEnd - recordingClip->time);
        cloned->loopEnabled = false;
        cloned->loopLen     = ((math::max(1, cloned->getLen() / (TICKS_BAR * 4))) * (TICKS_BAR * 4));
        cloned->notes.updateBounds();
        cloned->setDirty();
        samplesRecorded += samplePosBlockEnd - samplePosBlockStart;
        std::swap(recordDataProcessed, cloned);
        delete cloned;
        hasNewRecordedData = true;
    }
}
void clip_recorder::update(playback_state state, samplecount_t samplePosBlockStart, samplecount_t samplePosBlockEnd, tick_t tickBlockStart, tick_t tickBlockEnd, int trackType, bool bRecordArmed) {

    bool bIsPlayingAndRecording = state == playback_state::status_playback && bRecordArmed;
    if (notesProcessed || bIsPlayingAndRecording) {
        midiProcessedInput.updateBounds();
        if (bIsPlayingAndRecording) {
            updateRecordingClip(samplePosBlockStart, samplePosBlockEnd, tickBlockStart, tickBlockEnd, trackType, midiProcessedInput.m_list);
        }
    }

    if (recordingClip && !bIsPlayingAndRecording) {
        finishRecordingClip(samplePosBlockStart, samplePosBlockEnd, tickBlockStart, tickBlockEnd, midiProcessedInput.m_list);
    }
    notesProcessed = false;
}
void clip_recorder::recordNoteEvents(playback_state state, tick_t tickBlockStart, tick_t tickBlockEnd, const std::vector<midievent_note_t>& noteEventsProcessed) {
    bool notesProcessed = false;
    if (!midiProcessedInput.m_list.empty()) {
        auto it = midiProcessedInput.m_list.begin();
        while (it != midiProcessedInput.m_list.end()) {
            note_t& note = *it;
            if (!note.isHeld() && note.end() < tickBlockEnd) {
                notesProcessed = true;
                it             = midiProcessedInput.m_list.erase(it);
            } else {
                it++;
            }
        }
    }
    if (!noteEventsProcessed.empty()) {
        std::vector<note_t> newNotes;
        for (auto& msg : noteEventsProcessed) {
            if (msg.isNoteOn) {
                note_t note;
                note.setRealtime(true);
                note.setIsHeld(true);
                note.time     = tickBlockStart + msg.tickOffsetInBlock;
                note.len      = lenTicksInfinite;
                note.pitch    = msg.pitch;
                note.velocity = msg.velocity;
                newNotes.push_back(note);
            }
        }
        if (!newNotes.empty()) {
            //for (auto& note : newNotes) {
            //    log_printf("Block %d, note open %d (%s)\n", procPos, note.start(), noteName(note.pitch));
            //}
            midiProcessedInput.addAll(newNotes);
        }
        for (auto& msg : noteEventsProcessed) {
            if (!msg.isNoteOn) {
                int32_t pitch   = msg.pitch;
                int32_t tickEnd = tickBlockStart + msg.tickOffsetInBlock;
                //log_printf("%s@%d Looking for NOTE_ON evt\n", noteName(pitch), tickEnd);
                bool fnd = false;
                for (note_t& noteHeld : midiProcessedInput.m_list) {
                    if (noteHeld.pitch == pitch) {
                        if (!noteHeld.isHeld()) {
                            //log_printf("%s@%d note was released before (@%d), looking for next one\n",
                            //              noteName(noteHeld.pitch), noteHeld.start(), noteHeld.end());
                            continue;
                        }
                        if (noteHeld.start() > tickEnd) {
                            //log_printf("%s@%d note starts after this release\n",
                            //        noteName(noteHeld.pitch), noteHeld.start());
                            continue;
                        }
                        if (noteHeld.start() == tickEnd) {
                            //log_printf("%s noteHeld.start() == tickEnd %d, adding TICKS_16TH/4\n", noteName(noteHeld.pitch), tickEnd);
                            tickEnd += TICKS_16TH / 4;
                        }
                        noteHeld.len = tickEnd - noteHeld.start();
                        noteHeld.setIsHeld(false);
                        dbgassert(noteHeld.len >= 0);
                        fnd            = true;
                        notesProcessed = true;
                        //log_printf("Block %d, note complete %d END %d (%s)\n", procPos, noteHeld.start(), noteHeld.end(), noteName(noteHeld.pitch));
                        break;
                    }
                }
                if (!fnd) {
                    log_printf("MIDI_OFF_NOTE note not found %s tickEnd %d\n", noteName(pitch), tickEnd);
                }
            }
        }

        if (!newNotes.empty() || notesProcessed) {
            midiProcessedInput.removeDuplicates();
            notesProcessed = true;
        }
    }
    this->notesProcessed |= notesProcessed;

}
automatable_t* track_impl_t::getAutomatableByType(const automatable_param_ref_t& ref) {
    if (ref.type == AUTOMATABLE_MODULATOR_OUTPUT) {
        return nullptr;
    }
    if (ref.type == AUTOMATABLE_EFFECT) {
        return getPluginById(ref.refId);
    }
    if (ref.type == AUTOMATABLE_MIXER) {
        return &mixer;
    }
    if (ref.type == AUTOMATABLE_ARP) {
        return arp;
    }
    return nullptr;
}
namespace DAW {
    automated_param_connection_t GetParameterModulationFromRouting(const Host::PluginManager* const host, const automation_routing_t routing) {
        if (routing.type == automation_routing_type::ROUTING_NONE)
            return {routing.type, nullptr, 0};
        auto atl = resolveAutomatableRefDevice(host, routing.destinationRef);
        if (!atl)
            return {automation_routing_type::ROUTING_NONE, nullptr, 0};
        return automated_param_connection_t{routing.type, atl, routing.destinationRef.paramIdx};
    }
    automation_routing_t GetRoutingFromDestinationParam(const automatable_t* dev, int32_t paramIdx) {
        if (dev->isParamModulated(paramIdx)) {
            return ModulationRef(dev, paramIdx);
        }
        auto at = dev->getRegisteredConstAutomation(paramIdx);
        if (at && at->isAutomated()) {
            return AutomationRef(dev, paramIdx);
        }
        return AutomationConstant(dev, paramIdx);
    }
}// namespace DAW
void track_impl_t::createModulationRoutingSnapshot(track_modulation_routing_snapshot_t& snapshot) {
    audio_stage_t::createModulationRoutingSnapshot(snapshot);
    if (arp) snapshot.arp = arp->getModulations();
    snapshot.mixer = mixer.getModulations();
}
void track_impl_t::loadModulationRoutingSnapshot(const track_modulation_routing_snapshot_t& snapshot) {
    audio_stage_t::loadModulationRoutingSnapshot(snapshot);
    if (arp) arp->setModulations(snapshot.arp);
    mixer.setModulations(snapshot.mixer);
}
String midievent_note_t::ToString() {
    IMidiMsg msg;
    if(isNoteOn) {
        msg.MakeNoteOnMsg(pitch, globalTick, 0);
    } else {
        msg.MakeNoteOffMsg(pitch, globalTick, 0);
    }
    return msg.ToString();
}

namespace DAW::Host {

void noteevent_buffer::update(tick_t blockStart, const std::vector<midievent_note_t>& _noteEvts, const std::vector<midievent_ctrl_t>& _ctrlEvts) {
    this->currentTick = blockStart;
    addAll(noteEvts, _noteEvts);
    const tick_t eventTimeout = 10000;// TODO: calculate this depending on the total latency
    auto it                   = noteEvts.begin();
    while (it != noteEvts.end()) {
        auto& evt = *it;
        if (evt.globalTick < currentTick - (eventTimeout)) {
            it = noteEvts.erase(it);
        } else {
            it++;
        }
    }
    // addAll(ctrlEvts, _ctrlEvts);
    auto itSrc = _ctrlEvts.begin();
    auto itEvt = ctrlEvts.begin();
    while (itEvt != ctrlEvts.end() && itSrc != _ctrlEvts.end()) {
        auto& src = *itSrc;
        if (itEvt->tick < currentTick - (eventTimeout)) {
            itEvt = ctrlEvts.erase(itEvt);
            continue;
        }
        if (itEvt->tick > src.tick) {
            itEvt = ctrlEvts.insert(itEvt, src);
            itSrc++;
            continue;
        }
        if (itEvt->tick == src.tick && itEvt->message == src.message) {
            break;
        }
        itEvt++;
    }
    while (itSrc != _ctrlEvts.end()) {
        ctrlEvts.push_back(*itSrc++);
    }
}

void noteevent_buffer::getNotesDelayed(tick_t tickLatencyCompensated, const double ticksPerBlock, std::vector<midievent_note_t>& noteEvtsOuts, std::vector<midievent_ctrl_t>& ctrlEvtsOut) {
    if (tickLatencyCompensated > currentTick) {
        log_lf(Log::L_WARN, "tickLatencyCompensated=%d, ticksPerBlock=%f, currentTick=%d\n", tickLatencyCompensated, ticksPerBlock, currentTick);
        return;
    }
    if (!noteEvts.empty()) {
        for (auto& evt : noteEvts) {
            if (evt.globalTick >= tickLatencyCompensated && evt.globalTick < tickLatencyCompensated + ticksPerBlock) {
                noteEvtsOuts.emplace_back(evt);
                auto& evtCompensated = noteEvtsOuts.back();
                evtCompensated.tickOffsetInBlock = (evtCompensated.globalTick - tickLatencyCompensated);
                dbgassert(evt.tickOffsetInBlock >= 0 && evt.tickOffsetInBlock < ticksPerBlock);
            }
        }
    }
    if (!ctrlEvts.empty()) {
        for (auto& evt : ctrlEvts) {
            if (evt.tick >= tickLatencyCompensated && evt.tick < tickLatencyCompensated + ticksPerBlock) {
                ctrlEvtsOut.emplace_back(evt);
            }
        }
        /* if (!ctrlEvtsOut.empty()) {
            log_lf(Log::L_DEBUG, "%zd events in range %d %f. %zd events total\n", ctrlEvtsOut.size(), tickLatencyCompensated, tickLatencyCompensated+ticksPerBlock, ctrlEvts.size());
            for (auto& evt : ctrlEvtsOut) {
                auto msg = IMidiMsg::FromU32AndTick(evt.message, evt.tick);
                log_lf(Log::L_DEBUG, "in range: %s\n", msg.ToString().c_str());
            }
        } */
    }
}

} // namespace DAW::Host
