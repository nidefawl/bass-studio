#include <algorithm>

#include "math/seq_math.h"
#include "exceptions.h"
#include "logging.h"
#include "samplerate.h"
#include "seq_util.h"
#include "seq_time.h"

#include "../gui/pluginctr.h"
#include "../gui/trackctr.h"
#include "../gui/trackcontrols.h"
#include "../gui/trackcontent.h"

#include "note.h"
#include "clip.h"
#include "midiarp.h"
#include "track.h"
#include "cursor.h"
#include "audiocache.h"
#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"
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
#include "gui/subtrack.h"
#include "midi-msg.h"
#include "fileio.h"
#include "clip.h"
#include "assert_dbg.h"


const tick_t INVALID_TICK = 1 << 31;

void releaseClipResources(clip_t* cl, delete_cb* cb) {
    if (cb)
        cb->preClipDelete(cl);
    //    if (waveformrender::getInstance()) {
    //        waveformrender::getInstance()->assertWaveformRefIsUnbound(&cl->audio.waveformRef);
    //    }
    std::vector<track_gui_entry_t*> copyEntries = cl->trackEntries;
    for (track_gui_entry_t* entry : copyEntries) {
        track_t* track = entry->track;
        if (track) {
            if (entry->clipsGuis.count(cl)) {
                auto* pGui = entry->clipsGuis[cl];
                dbgassert(pGui);
                entry->content->remove(pGui);
                DELETE_PTR(pGui);
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
    vsthost* host = vsthost::getInstance();
    host->unloadTrack(tr);
    tr->getMidi().deleteClips(cb);
    //    if (tr->mixer) {
    //        delete (tr->mixer);
    //    }
    //    if (tr->content) {
    //        delete (tr->content);
    //    }
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
    dbgassert(midi.getConstClips().empty());
    for (const clip_t& clip : obj.clips) {
        midi.addClip(new clip_t(clip));
    }
    midi.sortClips();
    tracksettings_t& dst       = *static_cast<tracksettings_t*>(this);
    const tracksettings_t& src = *static_cast<const tracksettings_t*>(&obj);
    dst                        = src;
    scrolloffset               = 0;
    return *this;
}
void track_t::fixClipLengths() {
    for (clip_t* clip : midi.getClips()) {
        if (clip->clipType == CLIP_AUDIO && project_controller_t::get()) {
            dbgassert(clip->lenSamples > 0 && clip->len > 0);
            auto convertetLenSamples = project_controller_t::get()->tickToSamples(clip->len);
            auto convertetLenTicks   = project_controller_t::get()->samplesToTicks(clip->lenSamples);
            clip->len                = convertetLenTicks;
        }

    }
}
track_t::track_t(const track_snapshot_t& a)
    : tracksettings_t(a), localIdxFlat(a.localIdx) {
    dbgassert(midi.getConstClips().empty());
    for (const clip_t& clip : a.clips) {
        midi.addClip(new clip_t(clip));
    }
}

track_impl_snapshot_t::track_impl_snapshot_t(track_impl_t* p, bool storePluginChunks) {
    if (p) {
        p->arp->createSnapshot(trackArp);
        p->mixer.createSnapshot(trackParams);
        p->createIOSnapshot(trackIO);
        p->createRoutingSnapshot(effectRouting);
        std::vector<effectbase*> effects = p->effects;
        pluginSnapshots.reserve(p->effects.size());
        for (effectbase* effect : p->effects) {
            plugin_snapshot_t ps;
            effect->makeSnapshot(ps, storePluginChunks);
            pluginSnapshots.push_back(std::move(ps));
        }
    }
}

void saveSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, track_layout_snapshot_t& snapshot);

track_id_snapshot_t getTrackIdSnapshot(const audio_stage_id_t& stageId) {
    return track_id_snapshot_t{
        static_cast<int32_t>(stageId.stageId),
        static_cast<int32_t>(stageId.inputStageId),
        static_cast<int32_t>(stageId.outputStageId),
        static_cast<int32_t>(stageId.outputPostStageId)
    };
}

track_snapshot_t::track_snapshot_t(const track_t* track, bool storePluginChunks)
    : tracksettings_t(*track),
      stageIds(track->audio ? getTrackIdSnapshot(track->audio->stageId) : track_id_snapshot_t{}), localIdx(track->localIdxFlat),
      plugins(track->audio, storePluginChunks) {
    auto& otherClips = track->getConstMidi().getConstClips();
    for (auto clip : otherClips) {
        clips.emplace_back(*clip);
    }
    track_impl_t* p = track->audio;
    if (p) {
        // get all trackcointainer instances
        std::vector<guictr_tracks*> trackCointainers;
        DawInstance::get()->getTrackContainers(trackCointainers);
        // should use UUID for serialization
        int32_t trackCtrIdx = 0;
        for (auto* ctr : trackCointainers) {
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
    const auto& implSnapshot = snapshot.plugins;
    // if the snapshot holds a stage id then use it, otherwise keep current stageId
    if (snapshot.stageIds.inputStageId != -1) {
        audio->stageId.stageId           = static_cast<audiostageid_i32>(snapshot.stageIds.stageId);
        audio->stageId.inputStageId      = static_cast<audiostageid_i32>(snapshot.stageIds.inputStageId);
        audio->stageId.outputStageId     = static_cast<audiostageid_i32>(snapshot.stageIds.outputStageId);
        audio->stageId.outputPostStageId = static_cast<audiostageid_i32>(snapshot.stageIds.outputPostStageId);
    }
    //TODO: test if stageId is in use. Caller is responsible for generating new stageId

    audio->mixer.loadSnapshot(implSnapshot.trackParams);
    audio->arp->loadSnapshot(implSnapshot.trackArp);
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
    for (auto gui : track->audio->guiInstances) {
        gui->state.selectedAutomationCtr = nullptr;
    }
    audio_stage_t::removePlugin(_effect, notifyUp);
}

void audio_stage_t::removePlugin(effectbase* _effect, bool notifyUp) {
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


struct VstEvent_t {
    int32_t maxEvents;
    VstEvents* vstEvents;
    VstMidiEvent* evtArr;
    int32_t numOns  = 0;
    int32_t numOffs = 0;

    explicit VstEvent_t(size_t s) : maxEvents(s) {
        /**
         * Allocates following struct equivalent to:
            struct VstEvents
            {
                VstInt32 numEvents;        ///< number of Events in array
                VstIntPtr reserved;        ///< zero (Reserved for future use)
                VstEvent* events[maxEvents];    ///< event pointer array, variable size
                VstMidiEvent midiEvents[maxEvents];
            };
         */

        size_t hdr = sizeof(VstEvents) + sizeof(VstEvent*) * (s - 2);
        size_t len = sizeof(VstMidiEvent) * (s);
        vstEvents  = static_cast<VstEvents*>(malloc(hdr));
        evtArr     = static_cast<VstMidiEvent*>(malloc(len));
        memset(vstEvents, 0, hdr);
        memset(evtArr, 0, len);
    }

    void reset() {
        numOns = numOffs = 0;
        //        vstEvents->numEvents = 0;
        //        memset(vstEvents->events, 0, sizeof(VstEvent)*maxEvents);
        memset(vstEvents, 0, sizeof(VstEvents) + sizeof(VstEvent*) * (maxEvents - 2));
        memset(evtArr, 0, sizeof(VstMidiEvent) * (maxEvents));
    }

    ~VstEvent_t() {
        free(vstEvents);
        free(evtArr);
    }

    void writeNoteOn(unsigned char* buf, int32_t pitch, int32_t velocity) {
        buf[0] = 0x90;
        buf[1] = CLAMP_I(pitch, 0, 0x7F);
        buf[2] = CLAMP_I(velocity, 0, 0x7F);
        buf[3] = 0;
    }

    void writeNoteOff(unsigned char* buf, int32_t pitch) {
        buf[0] = 0x80;
        buf[1] = CLAMP_I(pitch, 0, 0x7F);
        buf[2] = 0x40;
        buf[3] = 0;
    }

    void writeVstMidiEvt(noteevent_t& nevt, double tickToSamples, int32_t blockSize) {
        int32_t idx = vstEvents->numEvents;
        dbgassert(idx < maxEvents);
        VstMidiEvent& evt = evtArr[idx];
        evt.type          = kVstMidiType;
        evt.byteSize      = 24;//sizeof(VstMidiEvent);
        evt.flags         = 0; //kVstMidiEventIsRealtime;
        evt.deltaFrames   = math::floorS32D(nevt.tickOffsetInBlock * tickToSamples);
        dbgassert(evt.deltaFrames >= 0 && evt.deltaFrames < blockSize);
        if (nevt.isNoteOn) {
            numOns++;
            writeNoteOn((unsigned char*) evt.midiData, nevt.pitch, nevt.velocity);
        } else {
            numOffs++;
            writeNoteOff((unsigned char*) evt.midiData, nevt.pitch);
        }
        vstEvents->events[idx] = reinterpret_cast<VstEvent*>(&evt);
        vstEvents->numEvents++;
    }

    void writeMessage(unsigned char c0, unsigned char c1, unsigned char c2, unsigned char c3, int32_t delta) {
        int32_t idx = vstEvents->numEvents;
        dbgassert(idx < maxEvents);
        VstMidiEvent& evt  = evtArr[idx];
        evt.type           = kVstMidiType;
        evt.byteSize       = 24;//sizeof(VstMidiEvent);
        evt.flags          = 0; //kVstMidiEventIsRealtime
        evt.deltaFrames    = 0;

        unsigned char* buf = (unsigned char*) evt.midiData;

        buf[0] = c0;
        buf[1] = c1;
        buf[2] = c2;
        buf[3] = c3;

        vstEvents->events[idx] = reinterpret_cast<VstEvent*>(&evt);
        vstEvents->numEvents++;
    }
    void writeInstantOff() {
        /* Send all notes off midi event */
        writeMessage(0xB0, 123, 0, 0, 0);
        //for (int32_t i = 0; i < vstEvents->numEvents; i++) {
        //    evtArr[i].deltaFrames = 0;
        //}
    }
};

track_impl_t::~track_impl_t() {
    delete m_midiEventsBuf;
    delete arp;
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

samplerate_t audio_stage_t::getInternalLatency() const {
    return latencyInternal;
}

samplerate_t audio_stage_t::getOutputLatency() const {
    return latencyOuput;
}

samplerate_t audio_stage_t::getInputLatency() const {
    return latencyInput;
}

void audio_stage_t::pluginsChanged() {
    if (routingState != audiostagerouting_state_t::CUSTOM) {
        configureDefaultRoutings();
    }
    DAW::validateEffectRoutings(this->host, this);

    //host->onPluginsChanged(this);
    updateLatency();
}

void audio_stage_t::updateLatency() {
    //combined stage latency needs to be determined differently when using custom routing
    samplerate_t latency = 0;
    for (effectbase* effect : effects) {
        latency += effect->getPluginLatency();
    }
    this->latencyInternal = latency;
}

void audio_stage_t::getStageTargets(std::vector<automatable_t*>& targets) {
    if (std::find(targets.begin(), targets.end(), &mixer) == targets.end()) {
        targets.push_back(&mixer);
    }
    for (effectbase* child : effects) {
        targets.push_back(child);
        std::vector<audio_stage_t*> childStages;
        child->getChildAudioStages(childStages);
        for (audio_stage_t* childStage : childStages) {
            childStage->getStageTargets(targets);
        }
    }
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
            log_printf("Update audiostage of %s which is %s\n", StringAsCStr(pluginCtr->getClassName()),
                      pluginCtr->isDefaultPluginCtr ? "default" : "group");
            pluginCtr->showTrack(audioStage);
        }
        audioStage = audioStage->parent;
    }
}

void track_impl_t::getAutomatableTrackTargets(std::vector<automatable_t*>& targets) {
    targets.push_back(&mixer);
    targets.push_back(arp);
    getStageTargets(targets);
}

void project_t::copyTo(project_snapshot_t& project) {
    trackList.copyTo(project);
}

void project_t::copyFrom(project_snapshot_t& project) {
    trackList.copyFrom(project);
}

effectbase* loadEffectModule(const plugin_snapshot_t& pluginSnapshot, bool forceLoad) {
    vsthost* host = vsthost::getInstance();
    String path;
    effectbase* effect      = nullptr;
    vstplugin* loadedPlugin = nullptr;
    if (pluginSnapshot.pluginType == PLUGIN_TYPE_VST) {
        log_printf("Next loading plugin %s, uId %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId);
        plugindatabase_t* db = plugindatabase_t::getInstance();
        if (db->resolve(pluginSnapshot, &path, forceLoad ? 1 : 0)) {
            log_printf("Plugin is registered... loading %s, uId %d, forceLoad %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId, forceLoad);
            vstpluginloadres res = host->loadPlugin(path, pluginSnapshot.uId, pluginSnapshot.projectGlobalId);
            if (res.result == 0 && res.plugin) {
                loadedPlugin = res.plugin;
                effect       = res.plugin;
            } else {
                log_printf("Failed loading: Error loading plugin %s, uId %d. Res: %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId, res.result);
            }
        } else {
            log_printf("Failed loading: Unknown plugin %s, uId %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId);
        }
    } else {
        effect = host->makeModuleInstance(pluginSnapshot.pluginType, pluginSnapshot.uId, pluginSnapshot.projectGlobalId);
        if (effect && effect->getModuleType() == PLUGIN_TYPE_INTERNAL_EFFECT) {
            loadedPlugin = dynamic_cast<vstplugin*>(effect);
        }
    }
    //if (loadedPlugin && (loadedPlugin->getFlagsVST() & effFlagsProgramChunks) != 0) {
    //    if (pluginSnapshot.dataChunk.size() > 0) {
    //        log_printf("Plugin %s: Load data1[%d]\n", StringAsCStr(loadedPlugin->sName), pluginSnapshot.dataChunk.size());
    //        loadedPlugin->dispatch(effSetChunk, 0, pluginSnapshot.dataChunk.size(), (void*) pluginSnapshot.dataChunk.data());
    //    }
    //    if (loadPluginPresetWithSnapshot && pluginSnapshot.dataChunk2.size() > 0) {
    //        log_printf("Plugin %s: Load data2[%d]\n", StringAsCStr(loadedPlugin->sName), pluginSnapshot.dataChunk2.size());
    //        loadedPlugin->dispatch(effSetChunk, 1, pluginSnapshot.dataChunk2.size(), (void*) pluginSnapshot.dataChunk2.data());
    //    }
    //}
    return effect;
}
void loadEffectParamsFromSnapshot(const plugin_snapshot_t& pluginSnapshot, effectbase* effect) {
    const std::vector<param_snapshot_t>& pluginSnapshotParams = pluginSnapshot.params;
    uint32_t missingParams                                    = 0;
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
        log_printf("%s: Loaded %d params\n", StringAsCStr(effect->getName()), pluginSnapshotParams.size());
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
    void createDawChannelRefSnapshot(const channel_ref_t& channel, io_configuration_snapshot_t& cfg);
    void loadDawChannelRefSnapshot(const io_configuration_snapshot_t& cfg, channel_ref_t& channel);

    void createDawChannelRefSnapshot(const DAW::channel_ref_t& channel, io_configuration_snapshot_t& cfg) {
        cfg.inputType         = static_cast<int32_t>(channel.type);
        cfg.channelOffset     = channel.inputChannelOffset;
        cfg.stageId           = static_cast<int32_t>(channel.stage.stageRef.stageId);
        cfg.stageEndPointType = static_cast<int32_t>(channel.stage.buffer);
        cfg.externalInputId   = channel.externalInputIdx;
        cfg.externalInputType = static_cast<int32_t>(channel.externalInputType);
    }
    void loadDawChannelRefSnapshot(const io_configuration_snapshot_t& cfg, channel_ref_t& channel) {
        channel.type                   = static_cast<DAW::channel_input_type>(cfg.inputType);
        channel.inputChannelOffset     = cfg.channelOffset;
        channel.stage.stageRef.stageId = static_cast<audiostageid_i32>(cfg.stageId);
        channel.stage.buffer           = static_cast<stagebuffer_point>(cfg.stageEndPointType);
        channel.externalInputIdx       = cfg.externalInputId;
        channel.externalInputType      = static_cast<AudioIO::tracktype>(cfg.externalInputType);
    }
}

void audio_stage_t::createRoutingSnapshot(track_effect_routing_snapshot_t& snapshot) {
    for (auto & channel : this->postEffectRouting) {
        io_configuration_snapshot_t cfg;
        createDawChannelRefSnapshot(channel, cfg);
        snapshot.inputRoutingOutputStage.push_back(cfg);
    }
    for (effectbase* effect : effects) {
        for (DAW::channel_ref_t& channel : effect->inputChannels) {
            io_configuration_snapshot_t cfg;
            createDawChannelRefSnapshot(channel, cfg);
            snapshot.inputRoutingEffects[static_cast<int32_t>(effect->projectGlobalId)].push_back(cfg);
        }
    }
    snapshot.routingState = static_cast<int32_t>(this->routingState);
}

void audio_stage_t::configureDefaultRoutings() {
    this->postEffectRouting.clear();
    this->postEffectRouting.push_back(DAW::ChannelDefaultNone());
    for (effectbase* effect : effects) {
        effect->inputChannels.clear();
        effect->inputChannels.push_back(DAW::ChannelDefaultNone());
    }
    routingState = audiostagerouting_state_t::DEFAULT;
}

void audio_stage_t::loadRoutingSnapshot(const track_effect_routing_snapshot_t& snapshot) {
    this->postEffectRouting.clear();
    this->routingState = audiostagerouting_state_t::INVALID;
    for (int i = 0; i < snapshot.inputRoutingOutputStage.size(); i++) {
        const io_configuration_snapshot_t& cfg = snapshot.inputRoutingOutputStage[i];
        DAW::channel_ref_t channel;
        loadDawChannelRefSnapshot(cfg, channel);
        this->postEffectRouting.push_back(channel);
    }
    audiostagerouting_state_t snapshotRoutingState = static_cast<audiostagerouting_state_t>(snapshot.routingState);
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
                        effect_deferred* def = dynamic_cast<effect_deferred*>(p);
                        const plugin_snapshot_t& plugSnapshot = def->getSnapshotConst();
                        log += StringFormat("%d(%d), ", static_cast<int32_t>(p->projectGlobalId), plugSnapshot.projectGlobalId);
                    } else {
                        log += StringFormat("%d, ", static_cast<int32_t>(p->projectGlobalId));
                    }
                }
                log += "]";
                log_printf("%s\n", StringAsCStr(log));
                String log2 = "snapshot.inputRoutingEffects = [";
                for (auto& p : snapshot.inputRoutingEffects) {
                    log2 += StringFormat("%d, ", static_cast<int32_t>(p.first));
                }
                log2 += "]";
                log_printf("%s\n", StringAsCStr(log2));
                log_printf("Plugin with id %d not found\n", static_cast<int32_t>(mapEntry.first));
                snapshotRoutingState = audiostagerouting_state_t::INVALID;
            } else {
                dbgassert(plugin->inputChannels.empty());
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
                log_printf("Failed loading effect\n", 0);
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

bool vsthost::addDeferredEffect(effectbase* plugin) {
    plugin->projectGlobalId = getNextGlobalModuleId(plugin->projectGlobalId);// hell is lose
                                                                             //    plugin->projectGlobalId = getNextGlobalModuleId(0); // everything goochy
    while (getPluginById(plugin->projectGlobalId) != nullptr) {
        plugin->projectGlobalId = getNextGlobalModuleId(0);
    }
    auto it = std::find_if(pluginsDeferred.begin(), pluginsDeferred.end(), [plugin](auto* eff) { return eff->projectGlobalId == plugin->projectGlobalId; });
    if (it != pluginsDeferred.end()) {
        return false;
    }
    pluginsDeferred.push_back(plugin);
    return true;
}

void vsthost::activateDeferred(effectbase* const eff, int flags, effectbase** out_effectLoaded) {
    dbgassert(eff->trackImpl);
    dbgassert(eff->trackImpl->effects.size());
    dbgassert(eff->getSlot() >= 0);

    auto defEffect = dynamic_cast<effect_deferred*>(eff);
    plugin_snapshot_t pluginSnapshot = defEffect->getSnapshotConst();
    log_printf("activating deferred plugin loadEffectModule %s\n", StringAsCStr(pluginSnapshot.name));
    effectbase* effect = loadEffectModule(pluginSnapshot, flags & FLAG_HOST_FORCELOAD_DISABLED_PLUGINS);
    if (out_effectLoaded) {
        *out_effectLoaded = effect;
    }
    if (!effect) {
        log_printf("Failed loading %s\n", StringAsCStr(pluginSnapshot.name));
        //dbgassert(0);
        return;
    }

    /* Begin of loading plugin state (parameter values, binary preset, automation lanes) */

    log_printf("Activate Plugin %s: %d Parameters, %d Automated parameters\n",
               StringAsCStr(pluginSnapshot.name),
               pluginSnapshot.params.size(),
               pluginSnapshot.automatedParams.size());

    bool loadParamsBeforePluginSnapshot = false;
    /* check if parameter values are assigned before loadSnapshot */
    if (loadParamsBeforePluginSnapshot) {
        loadEffectParamsFromSnapshot(pluginSnapshot, effect);
    }

    effectbase* prevPlugin = nullptr;
    always_assert(removeEntry(eff->trackImpl->deferredEffects, eff));
    replacePlugin(eff->trackImpl, effect, defEffect->getSlot(), &prevPlugin);

    /* Load plugins binary snapshot */
    effect->loadSnapshot(pluginSnapshot);

    /* check if parameter values are assigned after loadSnapshot */
    if (!loadParamsBeforePluginSnapshot) {
        loadEffectParamsFromSnapshot(pluginSnapshot, effect);
    }

    effect->inputChannels = prevPlugin->inputChannels;
    effect->sName         = pluginSnapshot.name;
    effect->setParamValue(PARAM_ENABLE, pluginSnapshot.enabled ? 1.0f : 0.0f, FLG_PAR_UPDATE_INIT | FLG_PAR_UPDATE_NOSTORE);

    /* Load plugin parameter automation lanes */
    loadAutomation(pluginSnapshot.automatedParams, effect);

    log_printf("done activating deferred plugin %s: isenabled %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.enabled);
    if (pluginSnapshot.enabled) {
        effect->resume();
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

int loadSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, const track_layout_snapshot_t& snapshot) {
    const std::vector<automationlane_snapshot_t>& atls = snapshot.automationLanes;
    track_t* const track                               = entry->track;
    int n                                              = atls.size();
    n                                                  = 0;
    for (const automationlane_snapshot_t& ref : atls) {
        gui_track_subtrack* al = NULL;
        if (ref.subtrackType == gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
            if (ref.type == AUTOMATABLE_EFFECT) {
                effectbase* plugin = track->getStage()->getPluginById(ref.refId);
                if (!plugin || plugin->getModuleType() == PLUGIN_TYPE_DEFERRED) {
                    log_printf("skipping deferred ref.type %d, ref.refId %d, ref.paramIdx %d\n", ref.type, ref.refId, ref.paramIdx);
                    n++;
                    continue;
                }
                al = new gui_track_automationlane(entry, guiTracks->grid, plugin, ref.paramIdx);
            }
            if (ref.type == AUTOMATABLE_MIXER) {
                al = new gui_track_automationlane(entry, guiTracks->grid, &track->getStage()->mixer, ref.paramIdx);
            }
            if (ref.type == AUTOMATABLE_ARP) {
                al = new gui_track_automationlane(entry, guiTracks->grid, track->getStage()->arp, ref.paramIdx);
            }
        } else if (ref.subtrackType == gui_track_subtrack::SUBTRACK_TYPE_WAVE) {
            al = makeGuiSubtrack(entry, MainCtrl::get(), ref.subtrackType);
        }
        if (al) {
            al->height = ref.height;
            guiTracks->addSubTrack(entry, al, false);
        }
    }

    return n;
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
        log_printf("save ref.type %d, ref.refId %d, ref.paramIdx %d\n", subtrackSnapshot.type, subtrackSnapshot.refId, subtrackSnapshot.paramIdx);
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
    log_printf("delete track %08X\n", reinterpret_cast<uint64_t>(this));
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
        int32_t srcStartOffset     = blockSamplePos - clipStartSample + clip->offsetSamples;
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
                auto& srcVector = i >= (int) sample->samples.size() ? sample->samples[sample->samples.size() - 1] : sample->samples[i];
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
                if (!a.isNoteOn && b.isNoteOn) {
                    return true;
                }
                return false;
            }
            return a.pitch < b.pitch;
        }
        return a.tickOffsetInBlock < b.tickOffsetInBlock;
    });
}

track_impl_t::track_impl_t(vsthost* const _host, audio_stage_id_t _id, track_t* _track, const samplerate_t _sampleRate, const uint16_t _blockSize, int32_t nChannels)
    : audio_stage_t(_host, _id, /*_track, */ _sampleRate, _blockSize, nChannels, 0),
      track(_track),
      inputChannel(DAW::ChannelDefaultNone()),
      outputChannel(DAW::ChannelDefaultNone()) {
    arp = new midiarp(this);
    midiProcessed = new clip_notes_t();
}

const std::vector<arp_note_t>& track_impl_t::getArpHeldNotes() {
    return this->arp->getHeldNotes();
}

std::vector<marker_t>& track_impl_t::getArpMarkers(int n) {
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
    for (effectbase* effect : effects) {
        vstplugin* vst = dynamic_cast<vstplugin*>(effect);
        if (vst && vst->bCanReceiveMidi) {
            //TODO: decide if we should make a copy, plugin may manipulate data
            //VstEvent_t midiEventsBufTemp = *midiEventsBuf;
            //log_printf("send %d midi events to %s\n", midiEventsBuf->vstEvents->numEvents, StringAsCStr(vst->getName()));
            vst->dispatch(effProcessEvents, 0, 0, midiEventsBuf->vstEvents);
        }
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

        std::vector<note_t> notes;
        hires_timer_t tmr;

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

        if (!notes.empty() || !m_heldNotes.empty() || arp != nullptr) {
            ThreadLock lock = midiMutex.lockThread();


            tick_t blockLoopStart = loopStart > -1 && blockStart < loopStart ? loopStart : blockStart;
            tick_t blockLoopEnd   = loopEnd > -1 && blockEnd > loopEnd ? loopEnd : blockEnd;


            std::vector<noteevent_t> noteEvents;

            tmr.reset();

            for (note_t& note : notes) {
                // Find beginning notes
                if (note.start() >= blockLoopStart && note.start() < blockLoopEnd) {
                    if (logProcessedNotes)
                        log_printf("Block %d-%d: %s ON at %d (abs time: %d len: %d)\n", blockStart, blockEnd, noteName(note.pitch), note.start() - blockStart, note.time, note.len);

                    noteEvents.emplace_back(note.pitch, note.velocity, note.start() - blockStart, note.start(), true, false);
                    m_heldNotes.push_back(note);
                }
                // Find ending notes
                if (note.end() > blockLoopStart && note.end() <= blockLoopEnd) {
                    if (removeEntry(m_heldNotes, note)) {
                        if (logProcessedNotes)
                            log_printf("Block %d-%d: %s OFF at %d/%f\n", blockStart, blockEnd, noteName(note.pitch), note.end() - blockStart - 1, ticksPerBlock);
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
                        log_printf("Block %d-%d: %s Force OFF at %d\n", blockStart, blockEnd, noteName(noteHeld.pitch), 0);
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
                        log_printf("Block %d-%d: %s Force OFF (LOOP END @%d) at %d/%f = %d\n", blockStart, blockEnd, noteName(noteHeld.pitch), loopEnd, tickOffsetInBlockEnd, ticksPerBlock, blockStart + tickOffsetInBlockEnd);
                    noteEvents.emplace_back(noteHeld.pitch, noteHeld.velocity, tickOffsetInBlockEnd, blockStart + tickOffsetInBlockEnd, false, true);
                    it = m_heldNotes.erase(it);
                }
            }

            updateProfilingTime(procMidiStats.tm3RevalidateEnds, tmr.getTimeReset());

            sortNoteEvents(noteEvents);

            updateProfilingTime(procMidiStats.tm4SortEvents, tmr.getTimeReset());

            tmr.reset();
            this->noteEventsProcessed.clear();
            if (flags & MidiFlags::PROCESS_ARP) {
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
                for (effectbase* effect : effects) {
                    auto* vst = dynamic_cast<vstplugin*>(effect);
                    if (vst && vst->isSynth) {
                        //TODO: decide if we should make a copy, plugin may manipulate data
                        //VstEvent_t midiEventsBufTemp = *midiEventsBuf;
                        vst->midiEventsDispatched += midiEventsBuf->vstEvents->numEvents;
                        vst->dispatch(effProcessEvents, 0, 0, midiEventsBuf->vstEvents);
                    }
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
                    log_printf("Block %d, note open %d (%s)\n", blockStart, note.start(), noteName(note.pitch));
                }
            midiProcessed->addAll(newNotes);
            notesProcessed = true;
        }
        for (noteevent_t& msg : noteEventsProcessed) {
            if (!msg.isNoteOn) {
                int32_t pitch   = msg.pitch;
                int32_t tickEnd = blockStart + msg.tickOffsetInBlock;
                if (logProcessedNotes)
                    log_printf("%s@%d Looking for NOTE_ON evt\n", noteName(pitch), tickEnd);
                bool fnd = false;
                for (note_t& noteHeld : midiProcessed->m_list) {
                    if (noteHeld.pitch == pitch) {
                        if (!noteHeld.isHeld()) {
                            //log_printf("%s@%d note was released before (@%d), looking for next one\n", noteName(noteHeld.pitch), noteHeld.start(), noteHeld.end());
                            continue;
                        }
                        if (noteHeld.start() > tickEnd) {
                            //log_printf("%s@%d note starts after this release\n", noteName(noteHeld.pitch), noteHeld.start());
                            continue;
                        }
                        if (noteHeld.start() == tickEnd) {
                            //log_printf("%s noteHeld.start() == tickEnd %d, adding TICKS_16TH/4\n", noteName(noteHeld.pitch), tickEnd);
                            tickEnd += TICKS_16TH / 4;
                        }
                        noteHeld.len = tickEnd - noteHeld.start();
                        noteHeld.setIsHeld(false);
                        assert(noteHeld.len >= 0);
                        fnd            = true;
                        notesProcessed = true;
                        if (logProcessedNotes)
                            log_printf("Block %d, note complete %d END %d (%s)\n", blockStart, noteHeld.start(), noteHeld.end(), noteName(noteHeld.pitch));
                        break;
                    }
                }
                if (!fnd) {
                    log_printf("MIDI_OFF_NOTE note not found %s tickEnd %d\n", noteName(pitch), tickEnd);
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
                        log_printf("Found notes overlapping (%s@%d-%d and @%d-%d)\n", noteName(n.pitch), n.start(), n.end(), c.start(), c.end());
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
                    log_printf("Note %s recorded from %s to %s\n", noteName(note.pitch), StringAsCStr(strTmStart), StringAsCStr(strTmEnd));
                    log_printf("Note %s recorded from %d to %d\n", noteName(note.pitch), note.start(), note.end());
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

void track_params_t::createSnapshot(track_params_snapshot_t& snapshot) {
    snapshot.params.reserve(getNumParameters());
    visitParams([&snapshot](auto& mapEntry) {
        automatable_param_t& param = mapEntry.second;
        snapshot.params.push_back(param_snapshot_t{ param.idx, param.value, param.inUse ? 1 : 0 });
    });
    storeAutomation(snapshot.automatedParams, this);
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
            { PARAM_ENABLE, "Enabled", 1.0f },
            { PARAM_TRACK_GAIN, "Gain", dsp_util::gainToLinScale(1.0f) },
    } };
    for (const track_param_entry_t& paramEntry : parameterTypes) {
        automatable_param_t* regparam = registerParam(paramEntry.id);

        regparam->value      = paramEntry.val;
        regparam->label      = paramEntry.name;
        regparam->shortLabel = paramEntry.name;
    }
    for (int i = 0; i < MAX_SEND_CHANNELS; i++) {
        automatable_param_t* regparam = registerParam(PARAM_OFFSET_SEND + i);

        regparam->value      = 0.0f;
        regparam->label      = StringFormat("Send %d", (i + 1));
        regparam->shortLabel = regparam->label;
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
                if (trImpl) {
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
