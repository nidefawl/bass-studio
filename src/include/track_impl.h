#pragma once
#include <memory>
#include <utility>
#include <vector>
#include <array>
#include <memory>
#include <type_traits>
#include "host/mainctrl.h"
#include "logging.h"
#include "seq_util.h"
#include "types.h"
#include "config.h"
#include "samplerate.h"
#include "math/seq_math.h"
#include "audioblock.h"
#include "automation.h"
#include "meter.h"
#include "note.h"
#include "dsp_util.h"
#include "str_util.h"
#include "seq_time.h"
#include "audiosample.h"
#include "audiotrack.h"
#include "snapshot.h"
#include "track.h"
#include "fileio.h"
#include "host/host_pluginmanager.h"
#include "host/plugin/base_plugin.h"
#include "host/audio_config.h"
#include "host/daw_channel.h"
#include "util/profiling.h"
#include "threads/threadlock.h"

#define PARAM_TRACK_GAIN 1
#define PARAM_TRACK_PAN 2

struct VstEvent_t;

class vstplugin;
class effectbase;
class guictr_plugins;
struct track_params_snapshot_t;
struct track_io_configuration_snapshot_t;
struct audio_stage_t;
extern const std::vector<SupportedFileType> vFILE_TYPES_TRACKSNAPSHOT;

struct track_audio_src {
    std::vector<float*> channels;
    samplecount_t samples = 0;
    sampleformat_t sampleFormat;
    AudioBlock toAudioBlock() const {
        return AudioBlock(channels, static_cast<samplecount_t>(samples));
    }
};
struct track_params_t : public automatable_t {
private:
    audio_stage_t* const audiostage;
    struct track_param_entry_t {
        int32_t id;
        String name;
        String unit;
        float val;
    };

public:
    explicit track_params_t(audio_stage_t* _audiostage);
    String getAutomatableName() override {
        return "Mixer";
    }
    float getParamValue(int32_t idx) override;
    /**
     * setParamValue
     * @param idx
     * @param val
     * @param flags valid flags are
     * #define FLG_PAR_UPDATE_INIT 1
     * #define FLG_PAR_UPDATE_USER 2
     * #define FLG_PAR_UPDATE_UNDO 4
     * #define FLG_PAR_UPDATE_AUTOMATED 8
     *
     */
    void setParamValue(int32_t idx, float val, int flags) override;
    automationlane_snapshot_t toRef() const override;
    track_t* getTrack() override;
    bool isEnabled() {
        return getParamUnchecked(PARAM_ENABLE)->value >= 0.5f;
    }
    void createSnapshot(track_params_snapshot_t& snapshot, const tracksnapshot_store_opts_t& opts);
    void loadSnapshot(const track_params_snapshot_t& snapshot);
    void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
};
class noteevent_buffer {
    tick_t currentTick = 0;
    std::vector<noteevent_t> events;
    public:
    void update(tick_t blockStart, const std::vector<noteevent_t>& noteEvents) {
        addAll(events, noteEvents);
        sortNoteEvents(events);
        auto it = events.begin();
        while (it != events.end()) {
            auto& evt = *it;
            if (evt.globalTick < blockStart-(100000)) {
                it = events.erase(it);
            } else {
                it++;
            }
        }
        this->currentTick = blockStart;
    }
    void reset() {
        events.clear();
    }
    void getNotesDelayed(tick_t tickLatencyCompensated, const double ticksPerBlock, std::vector<noteevent_t>& evtsOut) {
        if (tickLatencyCompensated > currentTick) {
            log_lf(Log::L_WARN, "tickLatencyCompensated=%d, ticksPerBlock=%f, currentTick=%d\n", tickLatencyCompensated, ticksPerBlock, currentTick);
            return;
        }
        if (!events.empty()) {
            for (auto& evt : events) {
                if (evt.globalTick >= tickLatencyCompensated && evt.globalTick < tickLatencyCompensated + ticksPerBlock) {
                    evtsOut.emplace_back(evt);
                    auto& evtCompensated = evtsOut.back();
                    evtCompensated.tickOffsetInBlock = (evtCompensated.globalTick - tickLatencyCompensated);
                    dbgassert(evt.tickOffsetInBlock >= 0 && evt.tickOffsetInBlock < ticksPerBlock);
                }
            }
            sortNoteEvents(evtsOut);
        }
    }

};
struct clip_recorder {
    clip_t* recordingClip = nullptr;
    std::atomic<bool> isRecording{};
    std::atomic<bool> hasNewRecordedData{};
    clip_t* recordDataProcessed = nullptr;
    clip_notes_t midiProcessedInput;
    bool notesProcessed = false;
    samplecount_t firstRecordedSample = 0;
    samplecount_t samplesWritten = 0;
    samplecount_t samplesRecorded = 0;
    int32_t audioSampleId = -1;
    store_sample_req_t ssr;
    void updateRecordingClip(samplecount_t samplePosBlockStart, samplecount_t samplePosBlockEnd, tick_t tickPosBlockStart, tick_t tickBlockEnd, int 
    trackType, const std::vector<note_t>& m_list);
    void finishRecordingClip(samplecount_t samplePosBlockStart, samplecount_t samplePosBlockEnd, tick_t tickPosBlockStart, tick_t tickBlockEnd, const std::vector<note_t>& m_list);
    public:
    clip_t* getRecordingClip() {
        return recordingClip;
    }
    void update(playback_state state, samplecount_t samplePosBlockStart, samplecount_t samplePosBlockEnd, tick_t tickBlockStart, tick_t tickBlockEnd, int trackType, bool bRecordArmed);
    void recordNoteEvents(playback_state state, tick_t tickBlockStart, tick_t tickBlockEnd, const std::vector<noteevent_t>& noteEventsProcessed);
    bool writeRecordedData(project_controller_t* project, track_impl_t* trImpl, audiocache* cache, DawInstance* daw);
};
struct audio_stage_t : public IDelayLineStorage {
    /**
     * Internal pre-process per-block input buffer
     * guaranteed to have at least 2 channels
     */
    AudioBlock input;
    /**
     * Internal post-process per-block output buffer
     * guaranteed to have at least 2 channels
     */
    AudioBlock output;
    AudioBlock outputPost;
    AudioBlock clipBuffer;

    track_params_t mixer;

    const int type;
    audio_stage_id_t stageId = {TRACKID_INVALID_I32, TRACKID_INVALID_I32, TRACKID_INVALID_I32};
    audiostageflags_t flags = audiostageflags_t::NONE;
    audiostagerouting_state_t routingState = audiostagerouting_state_t::INVALID;
    sampleformat_t sampleFormat;

    samplerate_t latencyInput = 0;
    samplerate_t latencyOuput = 0;


    audiotrack_t audioInput;
    audiotrack_t audioOutput;
    std::shared_ptr<DAW::meter_runningsum[]> meterDataInput;
    std::shared_ptr<DAW::meter_runningsum[]> meterDataOutput;
    DAW::rmsmeter meter;
    DAW::rmsmeter meterInput;
    std::map<uint32_t, std::shared_ptr<DelayLine>> effDelayLines;

    std::vector<effectbase*> effects;
    std::vector<effectbase*> deferredEffects;
    std::vector<DAW::channel_ref_t> postEffectRouting;
    std::vector<audio_stage_t*> children;

    DAW::Host::PluginManager* const host = nullptr;
    audio_stage_t* parent = nullptr;
    effectbase* owner = nullptr;
    /**
     * backward pointer to gui containing this effect stage.
     * Used in drag/move handling
     */
    guictr_plugins* m_pluginCtr = nullptr;

    stats_processing_timings_t procStats;
    noteevent_buffer notesPre;
    noteevent_buffer notesPost;
    clip_recorder recorder;
    std::shared_ptr<DAW::effect_processing_graph_t> processingGraph;

    audio_stage_t(DAW::Host::PluginManager* const _host, const audio_stage_id_t _id, const sampleformat_t _sampleFormat, const channelnum_t _numChannels, int _type = 1)
    : input(_numChannels, _sampleFormat.blockSize),
      output(_numChannels, _sampleFormat.blockSize),
      outputPost(_numChannels, _sampleFormat.blockSize),
      mixer(this),
      type(_type),
      stageId(_id),
      host(_host)
    {
        initMeters();
        setSampleFormat(_sampleFormat);
        configureDefaultRoutings();
    }
    virtual ~audio_stage_t();
    void setSampleFormat(const sampleformat_t _sampleFormat) {
        sampleFormat = _sampleFormat;
        input.realloc(_sampleFormat.blockSize);
        output.realloc(_sampleFormat.blockSize);
        outputPost.realloc(_sampleFormat.blockSize);
    }
    void initMeters() {
        meterDataOutput = std::shared_ptr<DAW::meter_runningsum[]>(new DAW::meter_runningsum[output.channels]);
        meterDataInput = std::shared_ptr<DAW::meter_runningsum[]>(new DAW::meter_runningsum[input.channels]);
        meter = DAW::rmsmeter(meterDataOutput.get(), output.channels);
        meterInput = DAW::rmsmeter(meterDataInput.get(), input.channels);
    }
    void getDeferredEffects(std::vector<effectbase*>& out_effects) {
        for (auto effect : effects) {
            effect->getDeferredEffects(out_effects);
        }
        if (!deferredEffects.empty()) {
            out_effects.reserve(out_effects.size()+deferredEffects.size());
            addAll(out_effects, deferredEffects);
        }
    }
    DelayLine* getProcessingDelayLine(uint32_t id) override {
        // no lock required here
        // std::lock_guard<mutex> hold(mtx);
        if (!effDelayLines.count(id)) {
            effDelayLines[id] = std::make_shared<DelayLine>();
        }
        return effDelayLines[id].get();
    }
    virtual void removePlugin(effectbase* _vst, bool notifyUp);
    void loadPlugins(const std::vector<plugin_snapshot_t>& trPluginList);
    samplecount_t getInternalLatency() const;
    samplecount_t getOutputLatency() const;
    samplecount_t getInputLatency() const;
    void insertEffect(int32_t idx, effectbase* _instrument);
    bool replaceEffect(int32_t idx, effectbase* _effect, effectbase** _prevEffect);
    void pluginsChanged();
    void onTick(double since);
    track_t* getTrack() const;
    void addAudioStage(audio_stage_t* stage);
    void removeAudioStage(audio_stage_t* stage);
    effectbase* getPluginById(int32_t projectGlobalId) const;
    audio_stage_ref_t toRef() const;
    void getStageTargets(std::vector<automatable_t*>& targets);
    void createRoutingSnapshot(track_effect_routing_snapshot_t& snapshot);
    void loadRoutingSnapshot(const track_effect_routing_snapshot_t& snapshot);
    void configureDefaultRoutings();
    virtual void sendNotesOff();
    virtual void onStartPlayback();
    virtual void sendNotesToEffect(const std::vector<noteevent_t>& evtsOut, tick_t tickLatencyCompensated, int32_t bpm100, effectbase* effect);
    virtual void getNotesDelayed(tick_t tickLatencyCompensated, const double ticksPerBlock, std::vector<noteevent_t>& evtsOut, bool isPost);
    virtual void onPlaybackJumpFromTo(int32_t fromSamplePos, double fromTickPos, int32_t toSamplePos, double toTickPos);
    void notifyPluginContainers();
    virtual void onStopPlayback();
};
inline bool isAudioStageChildOf(audio_stage_t* parent, audio_stage_t* child) {
    std::vector<audio_stage_t*>& children = parent->children;
    for (audio_stage_t* t : children) {
        if (t == child) {
            return true;
        }
        if (!t->children.empty() && isAudioStageChildOf(t, child)) {
            return true;
        }
    }
    return false;
}
track_id_snapshot_t saveTrackIdSnapshot(const audio_stage_id_t& stageId);
audio_stage_id_t loadTrackIdSnapshot(const track_id_snapshot_t& stageId);

class clip_notes_t;
namespace MidiFlags {
static constexpr int PROCESS_REALTIME = 1;
static constexpr int PROCESS_CLIPS = 2;
static constexpr int PROCESS_ARP = 4;
}
struct midi_events_t {
    const std::vector<noteevent_t>* noteEventsProcessed{};
    tick_t tickLatencyCompensated = 0;
    int32_t bpm100 = 0;
};
namespace DAW {
class midiarp;
struct arp_note_t;
void assignFreeStageIds(Host::PluginManager* host, plugin_snapshot_t& snapshot);
void assignFreeStageIdsTrackSnapshot(Host::PluginManager* host, track_snapshot_t& snapshot);
inline bool isChannelConnected(const channel_ref_t& ch) {
    return ch.type != stage_type::INPUT_EMPTY;
}
inline bool isTrackSrcSolod(const track_source_t& src) {
    return (src.flags & (audiostageflags_t::SOLO|audiostageflags_t::SOLO_PARENT)) != audiostageflags_t::NONE;
}
inline bool isMidiChannelConnected(const midichannel_ref_t& ch) {
    return ch.type != midistage_type::INPUT_EMPTY;
}
inline midichannel_ref_t MidiChannelNone() {
    return midichannel_ref_t{midistage_type::INPUT_EMPTY};
}
inline midichannel_ref_t MidiChannelDefault() {
    return midichannel_ref_t{midistage_type::INPUT_DEFAULT, {}, 0, "Default"};
}
inline midichannel_ref_t MidiChannelStage(const audio_stage_t* stage, stage_bufferpoint isInput) {
    dbgassert(stage);
    String str;
    auto track = stage->getTrack();
    if (track) {
        str = track->name;
    }
    if (isInput == stage_bufferpoint::INPUT) {
        str += " Pre";
    } else {
        str += " Post";
    }
    return midichannel_ref_t {
        midistage_type::INPUT_AUDIOSTAGE,
        { stage->toRef(), isInput },
        0,
        str
    };
}
inline midichannel_ref_t MidiChannelExternal(channelnum_t idx, String name) {
    return midichannel_ref_t {
        midistage_type::INPUT_EXTERNAL_MIDI, 
        {},
        idx,
        std::move(name)
    };
}
inline channel_ref_t ChannelNone() {
    return channel_ref_t{stage_type::INPUT_EMPTY};
}
inline channel_ref_t ChannelDefaultNone() {
    return channel_ref_t{stage_type::INPUT_DEFAULT};
}
inline channel_ref_t ChannelAudioInput(channelnum_t idx, channelnum_t channelOffset, String name, channel_pairing type) {
    return channel_ref_t {
        stage_type::INPUT_EXTERNAL_AUDIO, 
        type,
        { { TRACKID_INVALID_I32 }, stage_bufferpoint::OUTPUT }, 
        0, 
        idx,
        channelOffset,
        0,
        std::move(name)
    };
}
inline channel_ref_t ChannelStage(const audio_stage_t* stage, stage_bufferpoint isInput) {
    dbgassert(stage);
    String str;
    auto track = stage->getTrack();
    if (track) {
        str = track->name;
    }
    if (isInput == stage_bufferpoint::INPUT) {
        str += " In";
    } else {
        str += " Out";
    }
    return channel_ref_t {
        stage_type::INPUT_AUDIOSTAGE,
        AudioIO::getTrackTypeFromNumChannels(stage->input.channels),
        { stage->toRef(), isInput },
        0,
        0,
        0,
        0,
        str
    };
}
inline channel_ref_t ChannelAudioEffect(effectbase* effect, stage_bufferpoint isInput, const channel_desc& channelDescSrc, const channel_desc& channelDescDst) {
    dbgassert(effect);
    String str;
    auto stage = effect->getTrackLink();
    dbgassert(stage);
    auto track = stage->getTrack();
    if (track) {
        str = track->name;
    }
    str += effect->getName();
    str += " ";
    str += channelDescSrc.name;
    return channel_ref_t {
        stage_type::INPUT_AUDIOSTAGE_EFFECT,
        AudioIO::getTrackTypeFromNumChannels(channelDescSrc.count),
        { stage->toRef(), isInput },
        effect->projectGlobalId,
        0,
        channelDescSrc.offset,
        channelDescDst.offset,
        str
    };
}
}
struct track_gui_entry_t;
inline void updateProfilingTime(int64_t& field, int64_t tm, uint8_t weighting = 20) {
    field = (field * (weighting-1) + tm) / weighting;
}
struct track_impl_t : public audio_stage_t {
    DAW::midiarp* arp = nullptr;
    track_t* track;
    std::vector<note_t> m_heldNotes;
    DAW::channel_ref_t inputChannel;
    DAW::channel_ref_t outputChannel;
    DAW::midichannel_ref_t midiChannel;
    std::vector<track_gui_entry_t*> guiInstances;
    std::vector<noteevent_t> noteEventsProcessed;
    clip_notes_t* midiProcessed = nullptr;
    ThreadMutex midiMutex;
    track_midiprocess_profiling_t procMidiStats;
    hires_timer_t tmr;
    track_impl_t(DAW::Host::PluginManager* const _host, audio_stage_id_t _id, track_t* _track, const sampleformat_t _sampleFormat, const channelnum_t _numChannels);
    ~track_impl_t() override;
    void sendNotesOff() override;
    void onStartPlayback() override;
    void onStopPlayback() override;
    void onPlaybackJumpFromTo(int32_t fromSamplePos, double fromTickPos, int32_t toSamplePos, double toTickPos) override;
    void processMidiInput(playback_state state, int32_t flags, tick_t cursorPos, tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, project_globals_t& prjGlobals, samplecount_t inputLatency, const clip_notes_t& midiRealtimeInput);
    void postProcessMidiInput(playback_state state, int32_t flags, tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, project_globals_t& prjGlobals, samplecount_t inputLatency);
    void fillAudio(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, project_globals_t& prjGlobals, int32_t readPos, int32_t readLen, float** dstBuffer);
    void addAudio(const AudioBlock& src, float fGain);
    void removePlugin(effectbase* _vst, bool notifyUp) override;
    const std::vector<DAW::arp_note_t>& getArpHeldNotes();
    std::vector<marker_t>& getArpMarkers(int n);
    void updateAutomatableTargets(tick_t processingPos);
    void getAutomatableTrackTargets(std::vector<automatable_t*>& targets, bool includeEffects = true);
    void createIOSnapshot(track_io_configuration_snapshot_t& snapshot);
    void loadIOConfiguration(const track_io_configuration_snapshot_t& trPluginList);
};

class action_modify_track : public action_base {
protected:
    trackstate_t before;
    trackstate_t after;

public:
    action_modify_track() = default;
    action_modify_track(String description, trackstate_t&& _tracks) : action_base() {
        desc   = std::move(description);
        before = std::move(_tracks);
    }
    static void loadTrackSnapshot(DawInstance* daw, track_t* track, const track_snapshot_t* trackStored);
    void undo(DawInstance* daw) override;
    void redo(DawInstance* daw) override;
};