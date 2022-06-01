#include <algorithm>

#include "math/seq_math.h"
#include "exceptions.h"
#include "logging.h"
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
#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"
#include "types.h"
#include "vst_host.h"
#include "track_impl.h"

#include "modules.h"
#include "project.h"
#include "projectcontroller.h"
#include "snapshot.h"
#include "mainctrl.h"
#include "history.h"
#include "plugindatabase.h"
#include "wave/waveform_render_impl.h"
#include "gui/track/subtrack.h"
#include "midi-msg.h"
#include "fileio.h"
#include "clip.h"
#include "host/vst_midi_event.h"
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
    log_lf(Log::L_DEBUG, "release track %016zX\n", reinterpret_cast<uint64_t>(tr));
    dbgassert(tr && tr->audio);
    if (cb)
        cb->preTrackDelete(tr);
    vsthost* host = vsthost::getInstance();
    host->unloadTrack(tr);
    tr->getMidi().deleteClips(cb);
    dbgassert(tr->audio->guiInstances.empty());
    host->releaseAudio(tr);
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
void track_t::updateAudioClipLengths() {
    auto ctrl = project_controller_t::get();
    if (!ctrl)
        return;
    for (clip_t* clip : midi.getClips()) {
        if (clip->clipType == CLIP_AUDIO) {
            dbgassert(clip->lenSamples > 0 && clip->len > 0);
            clip->len = ctrl->samplesToTicks(clip->lenSamples);
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
    for (auto trackentry : _effect->getTrackLink()->getTrack()->getStage()->guiInstances) {
        trackentry->state.selectedAutomationCtr = nullptr;
        trackentry->parent->removeAllAutomationLanes(trackentry, _effect);
    }
    removeEntry(deferredEffects, _effect);
    if (!removeEntry(effects, _effect)) {
        return;
    }
    int slot = 0;
    for (effectbase* effect : effects) {
        effect->setSlot(slot++);
    }
    auto stage = _effect->getTrackLink();
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
    delete m_midiEventsBuf;
    delete arp;
    delete midiProcessed;
}

VstEvent_t* track_impl_t::reallocEvts(size_t size) {
    size = math::max((size_t) 128, size);
    if (m_midiEventsBuf == nullptr || m_midiEventsBuf->maxEvents < (int32_t) size) {
        if (m_midiEventsBuf) delete m_midiEventsBuf;
        m_midiEventsBuf = new VstEvent_t(size);
    }
    m_midiEventsBuf->reset();
    return m_midiEventsBuf;
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

void audio_stage_t::sendNotesOff(int32_t bpm100) {
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
    this->processingGraph.reset();
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

void track_impl_t::updateAutomatableTargets(tick_t processingPos) {
    mixer.updateAutomatedParameters(processingPos);
    if (arp) {
        arp->updateAutomatedParameters(processingPos);
    }
}

void project_t::copyTo(project_snapshot_t& project) {
    trackList.copyTo(project);
}

void project_t::copyFrom(project_snapshot_t& project) {
    trackList.copyFrom(project);
}

effectbase* loadEffectModule(vsthost* host, const plugin_snapshot_t& pluginSnapshot, bool forceLoad) {
    effectbase* effect      = nullptr;
    if (pluginSnapshot.pluginType == PLUGIN_TYPE_VST) {
        log_printf("Next loading plugin %s, uId %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId);
        plugindatabase_t* db = plugindatabase_t::getInstance();
        pluginentry_t resolvedPlugin;
        if (db->resolve(pluginSnapshot, resolvedPlugin, forceLoad ? 1 : 0)) {
            log_lf(Log::L_DEBUG, "Plugin is registered... loading %s, uId %d, forceLoad %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId, forceLoad);
            vstpluginloadres res = host->loadPlugin(resolvedPlugin.path, pluginSnapshot.uId, pluginSnapshot.projectGlobalId, resolvedPlugin.bugfixFlags);
            if (res.result == 0 && res.plugin) {
                res.plugin->localDbId = resolvedPlugin.localDbId;
                effect = res.plugin;
            } else {
                log_printf("Failed loading: Error loading plugin %s, uId %d. Res: %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId, res.result);
            }
        } else {
            log_printf("Failed loading: Unknown plugin %s, uId %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId);
        }
    } else {
        effect = host->makeModuleInstance(pluginSnapshot.pluginType, pluginSnapshot.uId, pluginSnapshot.projectGlobalId);
    }
    return effect;
}
void loadEffectParamsFromSnapshot(const plugin_snapshot_t& pluginSnapshot, effectbase* effect) {
    const std::vector<param_snapshot_t>& pluginSnapshotParams = pluginSnapshot.params;
    uint32_t missingParams = 0;
    for (const param_snapshot_t& param : pluginSnapshotParams) {
        automatable_param_t* atParam = effect->getParam(param.idx);
        if (atParam) {
            dbgassert(param.val >= 0.0f && param.val <= 1.0f);
            int flags = FLG_PAR_UPDATE_INIT;
            if (!param.flags) {
                flags |= FLG_PAR_UPDATE_NOSTORE;
            }
            effect->setParamValue(atParam->idx, param.val, flags);
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
namespace DAW {
    void createDawChannelRefSnapshot(const DAW::channel_ref_t& channel, io_configuration_snapshot_t& cfg) {
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
        channel.type                   = static_cast<DAW::stage_type>(cfg.type);
        channel.stage.stageRef.stageId = static_cast<audiostageid_i32>(cfg.stageId);
        channel.stage.buffer           = static_cast<stage_bufferpoint>(cfg.stageEndPointType);
        channel.externalInputType      = static_cast<DAW::channel_pairing>(cfg.externalInputType);
        channel.projectGlobalId        = cfg.projectGlobalId;
        channel.externalInputIdx       = cfg.externalInputIdx;
        channel.srcChannelOffset       = cfg.srcChannelOffset;
        channel.dstChannelOffset       = cfg.dstChannelOffset;
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
        for (DAW::channel_ref_t& channel : effect->inputChannels) {
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

void track_impl_t::createIOSnapshot(track_io_configuration_snapshot_t& snapshot) {
    for (int i = 0; i < 2; i++) {
        DAW::channel_ref_t channel       = i == 0 ? inputChannel : outputChannel;
        io_configuration_snapshot_t& cfg = i == 0 ? snapshot.input : snapshot.output;
        createDawChannelRefSnapshot(channel, cfg);
    }
}

void track_impl_t::loadIOConfiguration(const track_io_configuration_snapshot_t& snapshot) {
    for (int i = 0; i < 2; i++) {
        DAW::channel_ref_t& channel     = i == 0 ? inputChannel : outputChannel;
        io_configuration_snapshot_t cfg = i == 0 ? snapshot.input : snapshot.output;
        loadDawChannelRefSnapshot(cfg, channel);
    }
}

void audio_stage_t::loadPlugins(const std::vector<plugin_snapshot_t>& trPluginList) {
    for (const plugin_snapshot_t& pluginSnapshot : trPluginList) {
        auto effect = loadPluginDeferred(pluginSnapshot);
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
                host->activateDeferred(effect, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
            }
        }
    }
}

void vsthost::activateDeferred(effectbase* const eff, int flags, effectbase** out_effectLoaded) {
    dbgassert(eff->trackImpl);
    dbgassert(eff->trackImpl->effects.size());
    dbgassert(eff->getSlot() >= 0);

    auto defEffect = dynamic_cast<effect_deferred*>(eff);
    plugin_snapshot_t pluginSnapshot = defEffect->getSnapshotConst();
    log_printf("activating deferred plugin loadEffectModule %s\n", StringAsCStr(pluginSnapshot.name));
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

    log_printf("Activate Plugin %s: %zu Parameters, %zu Automated parameters\n",
               StringAsCStr(pluginSnapshot.name),
               pluginSnapshot.params.size(),
               pluginSnapshot.automatedParams.size());

    effectbase* prevPlugin = nullptr;
    always_assert(removeEntry(eff->trackImpl->deferredEffects, eff));
    replacePlugin(eff->trackImpl, effect, defEffect->getSlot(), &prevPlugin);

    /* Load plugins snapshot */
    effect->loadSnapshot(pluginSnapshot);

    effect->inputChannels = prevPlugin->inputChannels;
    effect->sName         = pluginSnapshot.name;
    effect->setProductName(pluginSnapshot.name);
    effect->setParamValue(PARAM_ENABLE, pluginSnapshot.enabled ? 1.0f : 0.0f, FLG_PAR_UPDATE_INIT | FLG_PAR_UPDATE_NOSTORE);

    /* Load plugin parameter automation lanes */
    loadAutomation(pluginSnapshot.automatedParams, effect);

    log_lf(Log::L_DEBUG, "done activating deferred plugin %s: isenabled %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.enabled);
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

void loadSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, const track_layout_snapshot_t& snapshot) {
    const std::vector<automationlane_snapshot_t>& atls = snapshot.automationLanes;

    track_t* const track = entry->track;
    for (const automationlane_snapshot_t& ref : atls) {
        gui_track_subtrack* al = NULL;
        if (ref.subtrackType == gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
            if (ref.type == AUTOMATABLE_EFFECT) {
                effectbase* plugin = track->getStage()->getPluginById(ref.refId);
                if (!plugin || plugin->getModuleType() == PLUGIN_TYPE_DEFERRED) {
                    log_printf("skipping layout for automation on deferred plugin ref.type %d, ref.refId %d, ref.paramIdx %d\n", ref.type, ref.refId, ref.paramIdx);
                    continue;
                }
                al = new gui_track_automationlane(entry, guiTracks->grid, plugin, ref.paramIdx);
            }
            if (ref.type == AUTOMATABLE_MIXER) {
                al = new gui_track_automationlane(entry, guiTracks->grid, &track->getStage()->mixer, ref.paramIdx);
            }
            if (ref.type == AUTOMATABLE_ARP) {
                auto arp = track->getStage()->arp;
                if (!arp) {
                    log_printf("skipping layout for automation on missing arp ref.type %d, ref.refId %d, ref.paramIdx %d\n", ref.type, ref.refId, ref.paramIdx);
                    continue;
                }
                al = new gui_track_automationlane(entry, guiTracks->grid, arp, ref.paramIdx);
            }
        } else if (ref.subtrackType == gui_track_subtrack::SUBTRACK_TYPE_WAVE) {
            al = makeGuiSubtrack(entry, guiTracks->dawCtrl, ref.subtrackType);
        }
        if (al) {
            al->height = ref.height;
            guiTracks->addSubTrack(entry, al, false);
        }
    }
}

void saveSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, track_layout_snapshot_t& snapshot) {
    snapshot.automationLanes.reserve(entry->subtracks.size());
    for (gui_track_subtrack* atl : entry->subtracks) {
        automationlane_snapshot_t subtrackSnapshot;
        if (atl->subtrackType() == gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
            dbgassert(atl->at);
            subtrackSnapshot = atl->at->toRef();
        } else {
        }
        subtrackSnapshot.paramIdx     = atl->param;
        subtrackSnapshot.height       = atl->height;
        subtrackSnapshot.subtrackType = atl->subtrackType();
        snapshot.automationLanes.push_back(std::move(subtrackSnapshot));
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
    log_lf(Log::L_DEBUG, "delete audio_stage_t %08zX\n", reinterpret_cast<uint64_t>(this));
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
    while (stage->parent) {
        stage = stage->parent;
    }
    if (stage->type == 0) {
        return dynamic_cast<const track_impl_t*>(stage)->track;
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

void track_impl_t::fillAudio(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos, float** buffer, int32_t blockSize) {

    int32_t blockEnd  = blockSamplePos + blockSize;
    tick_t audioBegin = math::max(start, loopStart);
    tick_t audioEnd   = loopEnd < 0 ? end : math::min(end, loopEnd);
    std::vector<clip_t*> clips;
    track->getMidi().getClipsInRange(audioBegin, audioEnd, clips);
    for (clip_t* clip : clips) {
        tick_t clipStartTick    = clip->getOffsetStart();
        tick_t clipEndTick      = clip->end();
        int32_t clipStartSample = tickToSampleConvert<int32_t, roundmode::floor>(clipStartTick, bpm100, sampleFormat.sampleRate);
        int32_t clipEndSample   = tickToSampleConvert<int32_t, roundmode::floor>(clipEndTick, bpm100, sampleFormat.sampleRate);
        if (clipStartSample > blockEnd)
            continue;
        if (clipEndSample <= blockSamplePos)
            continue;
        int32_t clipEndSampleLen   = math::min((int32_t) blockSize, clipEndSample - blockSamplePos);
        int32_t clipStartSampleLen = blockSize - math::max((int32_t) 0, clipStartSample - blockSamplePos);
        int32_t srcStartOffset     = blockSamplePos - clipStartSample /* + clip->offsetSamples */;
        int32_t dstStartOffset     = math::max(0, clipStartSample - blockSamplePos);
        if (srcStartOffset + blockSize <= 0)
            continue;

        audiofile_t* audio = audiocache::getInstance()->get(clip->audio.id);
        if (audio) {
            audiosample_t* sample = audio->sample.get();
            if (srcStartOffset >= (int32_t) sample->nSamples)
                continue;
            dbgassert(!sample->samples.empty());
            for (uint32_t i = 0; i < this->input.channels; i++) {
                float* dst      = buffer[i];
                auto& srcVector = i >= sample->samples.size() ? sample->samples[sample->samples.size() - 1] : sample->samples[i];
                int32_t len     = math::min((int32_t) blockSize - math::max(0, -srcStartOffset),
                                            math::min(clipEndSampleLen, math::min(clipStartSampleLen, (int32_t) srcVector.size() - srcStartOffset)));
                dbgassert(len >= 0);
                if (len <= 0) {//TODO: could figure this out outside the loop
                    continue;
                }
                dbgassert(dstStartOffset + len <= (int32_t) blockSize);
                dbgassert(srcStartOffset + len <= (int32_t) srcVector.size());
                dbgassert(dstStartOffset >= 0);
                memcpy(dst + dstStartOffset, srcVector.data() + math::max(0, srcStartOffset), len * sizeof(float));
            }
        }
    }
}

void sortNoteEvents(std::vector<noteevent_t>& noteEvents) {
    std::sort(noteEvents.begin(), noteEvents.end(), [](const noteevent_t& a, const noteevent_t& b) {
        // sort by tick, pitch, note off, note on
        if (a.tickOffsetInBlock == b.tickOffsetInBlock) {
            if (a.pitch == b.pitch) {
                return !a.isNoteOn && b.isNoteOn;
            }
            return a.pitch < b.pitch;
        }
        return a.tickOffsetInBlock < b.tickOffsetInBlock;
    });
}

track_impl_t::track_impl_t(vsthost* const _host, audio_stage_id_t _id, track_t* _track, const sampleformat_t _sampleFormat, const channelnum_t _numChannels)
    : audio_stage_t(_host, _id, _sampleFormat, _numChannels, 0),
      arp(new DAW::midiarp(this)), track(_track),
      inputChannel(DAW::ChannelDefaultNone()),
      outputChannel(DAW::ChannelDefaultNone()), 
      midiProcessed(new clip_notes_t())
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

void track_impl_t::onStartPlayback() {
    ThreadLock lock = midiMutex.lockThread();
    if (arp)
        arp->onStartPlayback();
}

void track_impl_t::onStopPlayback() {
    midiProcessed->clear();
}

void track_impl_t::onPlaybackJumpFromTo(int32_t fromSamplePos, double fromTickPos, int32_t toSamplePos, double toTickPos) {
    midiProcessed->clear();
}

void track_impl_t::sendNotesOff(int32_t bpm100) {
    std::vector<noteevent_t> noteEvents;
    {
        ThreadLock lock = midiMutex.lockThread();
        if (arp) {
            arp->allNotesOff(noteEvents);
        }
        if (!arp || !arp->isProcessingEnabled()) {
            const std::vector<note_t>& heldNotes = track->audio->m_heldNotes;
            noteEvents.reserve(heldNotes.size());
            for (const note_t& noteHeld : heldNotes) {
                noteEvents.emplace_back(noteHeld.pitch, noteHeld.velocity, 0, noteHeld.start(), false, false);
            }
        }
        track->audio->m_heldNotes.clear();
    }
    sortNoteEvents(noteEvents);

    const double ticksPerBlock = sampleToTickConvert<double, roundmode::none>(sampleFormat.blockSize, bpm100, sampleFormat.sampleRate);
    const double tickToSamples = tickToSampleConvert<double, roundmode::none>(1.0, bpm100, sampleFormat.sampleRate);

    VstEvent_t* midiEventsBuf  = reallocEvts(noteEvents.size() + 1);
    for (noteevent_t& evt : noteEvents) {
        dbgassert(evt.tickOffsetInBlock >= 0 && evt.tickOffsetInBlock < ticksPerBlock);
        midiEventsBuf->writeVstMidiEvt(evt, tickToSamples, sampleFormat.blockSize);
    }
    dbgassert(midiEventsBuf->vstEvents->numEvents == (int32_t) noteEvents.size());
    midiEventsBuf->writeInstantOff();
    midi_events_t events{ &noteEvents, midiEventsBuf };
    for (effectbase* effect : effects) {
        effect->processMidi(events);
    }
}

/**
 * track_impl_t::sendNotes
 * Right now there is no latency compensation applied.
 * TODO: First apply latency compensation per-track. Then implement per-plugin latency compensation
 * TODO: OPTIMIZE this function. I saw up to 400x speed up in release mode
 */
void track_impl_t::sendNotes(playback_state state, int32_t flags,
                             tick_t cursorPos,
                             tick_t blockStart, tick_t blockEnd,
                             tick_t loopStart, tick_t loopEnd,
                             int32_t bpm100,
                             int32_t blockSamplePos,
                             const clip_notes_t& midiRealtimeInput) {
    constexpr bool logProcessedNotes = false;
    if (arp || std::any_of(effects.begin(), effects.end(), [](const effectbase* ref) {
            return ref->bCanReceiveMidi;
        })) {
        const double ticksPerBlock = sampleToTickConvert<double, roundmode::none>(sampleFormat.blockSize, bpm100, sampleFormat.sampleRate);
        const double tickToSamples = tickToSampleConvert<double, roundmode::none>(1.0, bpm100, sampleFormat.sampleRate);

        tmr.reset();

        std::vector<note_t> notes;
        if (flags & MidiFlags::PROCESS_CLIPS) {
            tick_t heldBegin = blockStart;
            tick_t heldEnd   = blockEnd;
            track->getMidi().getNotesInRange(heldBegin, heldEnd, -1, loopEnd, notes);
            //auto getParent = track->parent;
            //while (getParent) {
            //    getParent->getMidi().getNotesInRange(heldBegin, heldEnd, -1, loopEnd, notes);
            //    getParent = getParent->parent;
            //}
        }

        updateProfilingTime(procMidiStats.tm0InputClips, tmr.getTimeReset());

        if (flags & MidiFlags::PROCESS_REALTIME) {
            tick_t heldBegin = blockStart;
            tick_t heldEnd   = blockEnd;
            getClipNotesInTimeRange(heldBegin, heldEnd, -1, loopEnd, midiRealtimeInput, notes);
        }

        updateProfilingTime(procMidiStats.tm1InputRT, tmr.getTimeReset());

        if (!notes.empty() || !m_heldNotes.empty() || arp) {
            ThreadLock lock = midiMutex.lockThread();


            tick_t blockLoopStart = loopStart > -1 && blockStart < loopStart ? loopStart : blockStart;
            tick_t blockLoopEnd   = loopEnd > -1 && blockEnd > loopEnd ? loopEnd : blockEnd;


            std::vector<noteevent_t> noteEvents;

            tmr.reset();

            for (note_t& note : notes) {
                // Find beginning notes
                if (note.start() >= blockLoopStart && note.start() < blockLoopEnd) {
                    if (logProcessedNotes)
                        log_lf(Log::L_DEBUG, "Block %d-%d: %s ON at %d (abs time: %d len: %d)\n", blockStart, blockEnd, noteName(note.pitch), note.start() - blockStart, note.time, note.len);

                    noteEvents.emplace_back(note.pitch, note.velocity, note.start() - blockStart, note.start(), true, false);
                    m_heldNotes.push_back(note);
                }
                // Find ending notes
                if (note.end() > blockLoopStart && note.end() <= blockLoopEnd) {
                    if (removeEntry(m_heldNotes, note)) {
                        if (logProcessedNotes)
                            log_lf(Log::L_DEBUG, "Block %d-%d: %s OFF at %d/%f\n", blockStart, blockEnd, noteName(note.pitch), note.end() - blockStart - 1, ticksPerBlock);
                        noteEvents.emplace_back(note.pitch, note.velocity, note.end() - blockStart - 1, note.end() - 1, false, false);
                    }
                }
            }

            updateProfilingTime(procMidiStats.tm2ProcNotes, tmr.getTimeReset());

            // revalidate held notes ends so we end notes that were modified by the user (loop or clip modifactions)
            for (auto it = m_heldNotes.begin(); it != m_heldNotes.end();) {
                const note_t& noteHeld = *it;
                bool found             = false;
                for (note_t& note : notes) {
                    if (note.pitch != noteHeld.pitch) {
                        continue;
                    }
                    if (note.start() < blockLoopEnd && note.end() > blockEnd) {
                        found |= true;
                        break;
                    }
                }
                if (!found) {
                    if (logProcessedNotes)
                        log_lf(Log::L_DEBUG, "Block %d-%d: %s Force OFF at %d\n", blockStart, blockEnd, noteName(noteHeld.pitch), 0);
                    noteEvents.emplace_back(noteHeld.pitch, noteHeld.velocity, 0, blockStart, false, false);
                    it = m_heldNotes.erase(it);
                    continue;
                }
                ++it;
            }
            // force end notes at loop end boundary
            if (loopEnd > 0 && blockStart < loopEnd && blockEnd >= loopEnd) {
                for (auto it = m_heldNotes.begin(); it != m_heldNotes.end();) {
                    const note_t& noteHeld = *it;

                    auto tickOffsetInBlockEnd = math::min(blockEnd - blockStart - 1, loopEnd - blockStart - 1);
                    if (logProcessedNotes)
                        log_lf(Log::L_DEBUG, "Block %d-%d: %s Force OFF (LOOP END @%d) at %d/%f = %d\n", blockStart, blockEnd, noteName(noteHeld.pitch), loopEnd, tickOffsetInBlockEnd, ticksPerBlock, blockStart + tickOffsetInBlockEnd);
                    noteEvents.emplace_back(noteHeld.pitch, noteHeld.velocity, tickOffsetInBlockEnd, blockStart + tickOffsetInBlockEnd, false, true);
                    it = m_heldNotes.erase(it);
                }
            }

            updateProfilingTime(procMidiStats.tm3RevalidateEnds, tmr.getTimeReset());

            sortNoteEvents(noteEvents);

            updateProfilingTime(procMidiStats.tm4SortEvents, tmr.getTimeReset());

            tmr.reset();
            this->noteEventsProcessed.clear();
            if (arp && (flags & MidiFlags::PROCESS_ARP)) {
                arp->process(state, cursorPos, noteEvents, blockStart, blockEnd, loopStart, loopEnd, noteEventsProcessed);
            } else {
                noteEventsProcessed = std::move(noteEvents);
            }

            updateProfilingTime(procMidiStats.tm5ProcArp, tmr.getTimeReset());

            size_t numEvents = noteEventsProcessed.size();
            if (numEvents > 0) {
                VstEvent_t* midiEventsBuf = reallocEvts(numEvents);
                for (noteevent_t& evt : noteEventsProcessed) {
                    dbgassert(evt.tickOffsetInBlock >= 0 && evt.tickOffsetInBlock < ticksPerBlock);
                    midiEventsBuf->writeVstMidiEvt(evt, tickToSamples, sampleFormat.blockSize);
                }
                dbgassert(midiEventsBuf->vstEvents->numEvents == (int32_t) numEvents);
                midi_events_t events{ &noteEventsProcessed, midiEventsBuf };
                for (effectbase* effect : effects) {
                    effect->processMidi(events);
                }
            }

            updateProfilingTime(procMidiStats.tm6WriteVstEvents, tmr.getTimeReset());
        }

        processMidiOutput(state, flags, blockStart, blockEnd, loopStart, loopEnd, bpm100, blockSamplePos);

        this->noteEventsProcessed.clear();

        updateProfilingTime(procMidiStats.tm7ProcessOutput, tmr.getTimeReset());
    }
}
void track_impl_t::processMidiOutput(playback_state state, int32_t flags, tick_t blockStart, tick_t blockEnd, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos) {
    constexpr bool logProcessedNotes = false;
    bool notesProcessed              = false;
    const int32_t lenTicksInfinite   = TICKS_BAR * 16;
    if (!noteEventsProcessed.empty()) {

        std::vector<note_t> newNotes;
        for (noteevent_t& msg : noteEventsProcessed) {
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
            midiProcessed->addAll(newNotes);
            notesProcessed = true;
        }
        for (noteevent_t& msg : noteEventsProcessed) {
            if (!msg.isNoteOn) {
                int32_t pitch   = msg.pitch;
                int32_t tickEnd = blockStart + msg.tickOffsetInBlock;
                if (logProcessedNotes)
                    log_lf(Log::L_DEBUG, "%s@%d Looking for NOTE_ON evt\n", noteName(pitch), tickEnd);
                bool fnd = false;
                for (note_t& noteHeld : midiProcessed->m_list) {
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
                        assert(noteHeld.len >= 0);
                        fnd            = true;
                        notesProcessed = true;
                        if (logProcessedNotes)
                            log_lf(Log::L_DEBUG, "Block %d, note complete %d END %d (%s)\n", blockStart, noteHeld.start(), noteHeld.end(), noteName(noteHeld.pitch));
                        break;
                    }
                }
                if (!fnd) {
                    log_lf(Log::L_WARN, "MIDI_OFF_NOTE note not found %s tickEnd %d\n", noteName(pitch), tickEnd);
                }
            }
        }
        //if (newNotes.size() || notesProcessed) {
        //    midiProcessed->removeDuplicates();
        //    notesProcessed = true;
        //}
    }
    if (!midiProcessed->m_list.empty()) {

        // TODO: make this debug only
        {
            for (note_t& n : midiProcessed->m_list) {
                if (n.isHeld())
                    continue;
                int exactDupes = 0;
                for (note_t& c : midiProcessed->m_list) {
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

        auto it = midiProcessed->m_list.begin();
        while (it != midiProcessed->m_list.end()) {
            note_t& note = *it;
            if (!note.isHeld() && note.end() < blockStart) {
                String strTmStart = tickAsBeatString(note.start());
                String strTmEnd   = tickAsBeatString(note.end());
                if (logProcessedNotes) {
                    log_lf(Log::L_DEBUG, "Note %s recorded from %s to %s\n", noteName(note.pitch), StringAsCStr(strTmStart), StringAsCStr(strTmEnd));
                    log_lf(Log::L_DEBUG, "Note %s recorded from %d to %d\n", noteName(note.pitch), note.start(), note.end());
                }
                notesProcessed = true;
                it             = midiProcessed->m_list.erase(it);
            } else {
                it++;
            }
        }
    }
    if (notesProcessed) {
        midiProcessed->updateBounds();
    }
}

void track_params_t::createSnapshot(track_params_snapshot_t& snapshot, const tracksnapshot_store_opts_t& opts) {
    if (opts.storePluginPreset) {
        snapshot.params.reserve(getNumParameters());
        visitParams([&snapshot](auto& mapEntry) {
            automatable_param_t& param = mapEntry.second;
            snapshot.params.push_back(param_snapshot_t{ param.idx, param.value, param.inUse ? 1 : 0 });
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
    if (flags != FLG_PAR_UPDATE_USER) {
        return;
    }
    dbgassert(this->audiostage->getTrack());
    automationlane_snapshot_t ref = toRef();

    track_t* track    = this->audiostage->getTrack();
    parameter_ref_t p = { track->projectIdx, ref.type, 0, idx };
    DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
}

track_params_t::track_params_t(audio_stage_t* _audiostage) : automatable_t(), audiostage(_audiostage) {
    const std::array<track_param_entry_t, 2> parameterTypes{ {
            { PARAM_ENABLE, "Enabled", "", 1.0f },
            { PARAM_TRACK_GAIN, "Gain", "dB", dsp_util::gainToLinScale(1.0f) },
    } };
    for (const track_param_entry_t& paramEntry : parameterTypes) {
        automatable_param_t* regparam = registerParam(paramEntry.id);

        regparam->defaultValue = paramEntry.val;
        regparam->value = paramEntry.val;
        regparam->name  = paramEntry.name;
        regparam->unit  = paramEntry.unit;
    }
    for (int i = 0; i < MAX_SEND_CHANNELS; i++) {
        automatable_param_t* regparam = registerParam(PARAM_OFFSET_SEND + i);

        regparam->defaultValue = 0.0f;
        regparam->value = 0.0f;
        regparam->name  = StringFormat("Send %d", (i + 1));
        regparam->unit  = "dB";
    }
    getOrCreateAutomation(PARAM_ENABLE)->quantizationSteps = 1;
}

float track_params_t::getParamValue(int32_t idx) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    //        return convertValFrom(idx, param->value);
    return param->value;
}

void track_params_t::setParamValue(int32_t idx, float val, int flags) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    param->value = val;//convertValTo(idx, val);
}

track_t* track_params_t::getTrack() {
    return audiostage->getTrack();
}

automationlane_snapshot_t track_params_t::toRef() const {
    automationlane_snapshot_t ref;
    ref.type  = AUTOMATABLE_MIXER;
    ref.refId = static_cast<int32_t>(audiostage->stageId.stageId);
    return ref;
}


void assignFreeStageIds(vsthost* host, plugin_snapshot_t& snapshot) {
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

void assignFreeStageIdsTrackSnapshot(vsthost* host, track_snapshot_t& snapshot) {
    std::map<int32_t,int32_t> idMap;
    std::map<int32_t,int32_t> pluginIdMap;
    std::vector<track_effect_routing_snapshot_t*> all;
    std::vector<plugin_snapshot_t*> q;

    // if (host->isStageIdInUse(snapshot.stageIds)) {
        auto stageId = host->getNextGlobalAudioStageId(0);
        log_lf(Log::L_DEBUG, "stageId %d is in use, assigning new id %d\n", snapshot.stageIds.stageId, static_cast<int32_t>(stageId.stageId));
        idMap[snapshot.stageIds.stageId] = static_cast<int32_t>(stageId.stageId);
        idMap[snapshot.stageIds.inputStageId] = static_cast<int32_t>(stageId.inputStageId);
        idMap[snapshot.stageIds.outputStageId] = static_cast<int32_t>(stageId.outputStageId);
        idMap[snapshot.stageIds.outputPostStageId] = static_cast<int32_t>(stageId.outputPostStageId);
        snapshot.stageIds = saveTrackIdSnapshot(stageId);
        all.push_back(&snapshot.data.effectRouting);
    // }

    q.reserve(snapshot.data.pluginSnapshots.size());
    for (auto& plugin : snapshot.data.pluginSnapshots) {
        q.push_back(&plugin);
    }

    while (!q.empty()) {
        plugin_snapshot_t* s = q.back();
        q.pop_back();
        all.push_back(&s->effectRouting);
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
        effectRouting->inputRoutingEffects = inputRoutingEffects;
    }
}

namespace DAW {
    bool resolveAutomatableRef(const vsthost* const host, const automationlane_snapshot_t& ref, automatable_t** out) {
        if (ref.type == AUTOMATABLE_EFFECT) {
            effectbase* plugin = host->getPluginById(ref.refId);
            if (plugin) {
                *out = plugin;
                return true;
            }
            return false;
        }
        if (ref.type == AUTOMATABLE_MIXER) {
            auto stage = host->getAudioStage(AudioStageRefFromId(ref.refId));
            if (stage) {
                *out = &stage->mixer;
                return true;
            }
            return false;
        }
        if (ref.type == AUTOMATABLE_ARP) {
            auto stage = host->getAudioStage(AudioStageRefFromId(ref.refId));
            if (stage->getTrack() && stage->getTrack()->getStage()) {
                auto trImpl = stage->getTrack()->getStage();
                if (trImpl && trImpl->arp) {
                    *out = trImpl->arp;
                    return true;
                }
            }
            return false;
        }

        return false;
    }
    bool resolveAutomationAtTime(const vsthost* const host, const automation_ref_t& ref, tick_t atTime, float* fOut) {
        dbgassert(fOut);
        switch (ref.type) {
            case 0:
                *fOut = ref.val;
                return true;
            case 1:
                automatable_t* at = nullptr;
                if (resolveAutomatableRef(host, ref.snapshot, &at)) {
                    auto* atData = at->getRegisteredAutomation(ref.snapshot.paramIdx);
                    if (atData) {
                        *fOut = atData->getValueAt(atTime);
                        return true;
                    }
                }
                break;
        }
        return false;
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
