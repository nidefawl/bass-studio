#pragma once
#include <vector>
#include <array>
#include <memory>
#include <type_traits>
#include "config.h"
#include "math/seq_math.h"
#include "automation.h"
#include "meter.h"
#include "audioblock.h"
#include "samplerate.h"
#include "note.h"
#include "dsp_util.h"
#include "str_util.h"
#include "seq_time.h"
#include "audiosample.h"
#include "audiotrack.h"
#include "snapshot.h"
#include "track.h"
#include "fileio.h"
#include "host/vst_host.h"
#include "host/plugin/base_plugin.h"
#include "host/audio_config.h"
#include "host/daw_channel.h"
#include "profiling.h"
#include "threads/threadlock.h"

#define PARAM_TRACK_GAIN 1

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
	uint32_t samples = 0;
	int32_t latency = 0;
	sampleformat_t sampleFormat;
	AudioBlock toAudioBlock() const {

		const bool bFitsNumChannels = FitsTypeRange<uint32_t, decltype(channels.size())>(channels.size());
		const bool bFitsNumSamples = FitsTypeRange<uint32_t, decltype(samples)>(samples);
		dbgassert(bFitsNumChannels && bFitsNumSamples);
		if (!(bFitsNumChannels && bFitsNumSamples)) {
			return AudioBlock(0, 0);
		}
		return AudioBlock(channels, static_cast<uint32_t>(samples));
	}
};
struct track_params_t : public automatable_t {
private:
	audio_stage_t* const audiostage;
	struct track_param_entry_t {
		int32_t id;
		String name;
		float val;
	};

public:
	track_params_t(audio_stage_t* _audiostage);
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
	void createSnapshot(track_params_snapshot_t& snapshot);
	void loadSnapshot(const track_params_snapshot_t& snapshot);
	void postSetParameter(int32_t idx, float preVal, float val, int flags);
};
struct audio_stage_t {
	vsthost* host;
//	audiostageid_i32 stageId = TRACKID_INVALID_I32;
	audio_stage_id_t stageId = {TRACKID_INVALID_I32, TRACKID_INVALID_I32, TRACKID_INVALID_I32};
	audiostageflags_t flags = audiostageflags_t::NONE;
	audiostagerouting_state_t routingState = audiostagerouting_state_t::INVALID;
	audio_stage_t* parent;
	effectbase* owner;
	/**
	 * backward pointer to gui containing this effect stage.
	 * Used in drag/move handling
     */
	guictr_plugins* pluginCtr;
	rmsmeterimpl<16000> meter;
	rmsmeterimpl<16000> meterInput;
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
	track_params_t mixer;
	audiotrack_t audioOutput;
	samplerate_t latencyInternal = 0;
	samplerate_t latencyInput = 0;
	samplerate_t latencyOuput = 0;
	int type;
	sampleformat_t sampleFormat;

	std::vector<effectbase*> effects;
	std::vector<effectbase*> deferredEffects;
	std::vector<DAW::channel_ref_t> postEffectRouting;
	std::vector<audio_stage_t*> children;
	stats_processing_timings_t procStats;
	struct latency_info_t {
		int32_t delayToPreReturn = 0;
		int32_t delayToPostReturn = 0;
	} latencyInfo;
    std::map<uint32_t, std::shared_ptr<DelayLine>> effDelayLines;

	audio_stage_t(vsthost* const _host, const audio_stage_id_t _id,/*track_t* _track, */const samplerate_t _sampleRate, const uint16_t _blockSize, int32_t nChannels, int _type = 1)
	: host(_host), stageId(_id), parent(nullptr), owner(nullptr),/*track(_track),*/
	  pluginCtr(nullptr),
	  input(nChannels, _blockSize),
	  output(nChannels, _blockSize),
	  outputPost(nChannels, _blockSize),
	  mixer(this),
	  type(_type)
	{
		  sampleFormat.blockSize = _blockSize;
		  sampleFormat.sampleRate = _sampleRate;
		  sampleFormat.sampleformat = sampleformat_bits_t::FLOAT_32;
		  configureDefaultRoutings();
	}
	virtual ~audio_stage_t();
	void getDeferredEffects(std::vector<effectbase*>& out_effects) {
		for (auto effect : effects) {
			effect->getDeferredEffects(out_effects);
		}
		if (deferredEffects.size()) {
			out_effects.reserve(out_effects.size()+deferredEffects.size());
			addAll(out_effects, deferredEffects);
		}
	}
	DelayLine* getEffectDelayLine(uint32_t id, uint32_t numChannels) {
		using namespace std;
//	    lock_guard<mutex> hold(mtx);
	    if (!effDelayLines.count(id) || effDelayLines[id]->block.channels != numChannels) {
	    	effDelayLines[id] = std::shared_ptr<DelayLine>(new DelayLine(numChannels, 16));
	    }
	    return effDelayLines[id].get();
	}
	virtual void removePlugin(effectbase* _vst, bool notifyUp);
	void loadPlugins(const std::vector<plugin_snapshot_t>& trPluginList);
	samplerate_t getInternalLatency() const;
	samplerate_t getOutputLatency() const;
	samplerate_t getInputLatency() const;
	void insertEffect(int32_t idx, effectbase* _instrument);
	bool replaceEffect(int32_t idx, effectbase* _effect, effectbase** _prevEffect);
	void pluginsChanged();
	void updateLatency();
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
	virtual void sendNotesOff(int32_t bpm100);
	void notifyPluginContainers();
	virtual void onStopPlayback();
};
inline bool isAudioStageChildOf(audio_stage_t* parent, audio_stage_t* child) {
	std::vector<audio_stage_t*>& children = parent->children;
	for (audio_stage_t* t : children) {
		if (t == child) {
			return true;
		}
		if (t->children.size() && isAudioStageChildOf(t, child)) {
			return true;
		}
	}
	return false;
}

class clip_notes_t;
class midiarp;
struct arp_note_t;
namespace MidiFlags {
static constexpr int PROCESS_REALTIME = 1;
static constexpr int PROCESS_CLIPS = 2;
static constexpr int PROCESS_ARP = 4;
}
namespace DAW {
inline bool isChannelConnected(const DAW::channel_ref_t& ch) {
	return ch.stage.stageRef.stageId != TRACKID_INVALID_I32 || ch.externalInputIdx > -1;
}
inline channel_ref_t ChannelNone() {
	return channel_ref_t{};
}
inline channel_ref_t ChannelDefaultNone() {
	channel_ref_t ref = ChannelNone();
	ref.type = channel_input_type::INPUT_DEFAULT;
	return ref;
}
inline channel_ref_t ChannelAudioInput(int32_t idx, int32_t channelOffset, String name, AudioIO::tracktype type) {
	return channel_ref_t{channel_input_type::INPUT_EXTERNAL_AUDIO, type, idx, channelOffset, {{TRACKID_INVALID_I32}, stagebuffer_point::OUTPUT}, 0, name};
}
inline channel_ref_t ChannelStage(const audio_stage_t* stage, stagebuffer_point isInput) {
	dbgassert(stage);
	String str = "";
	auto track = stage->getTrack();
	if (track) {
		str = track->name;
	}
	if (isInput == stagebuffer_point::INPUT) {
		str += " IN";
	} else {
		str += " OUT";
	}
	return channel_ref_t{channel_input_type::INPUT_AUDIOSTAGE, AudioIO::getTrackTypeFromNumChannels(stage->input.channels), -1, 0, {stage->toRef(), isInput}, 0, str};
}
inline channel_ref_t ChannelAudioEffect(effectbase* effect, stagebuffer_point isInput) {
	dbgassert(effect);
	String str = "";
	auto stage = effect->getTrackLink();
	dbgassert(stage);
	auto track = stage->getTrack();
	if (track) {
		str = track->name;
	}
	str += effect->getName();
	int32_t numChannels;
	if (isInput == stagebuffer_point::INPUT) {
		str += " IN";
		numChannels = effect->blockInputs ? effect->blockInputs->channels : 2;
	} else {
		str += " OUT";
		numChannels = effect->blockOutputs ? effect->blockOutputs->channels : 2;
	}
	return channel_ref_t{channel_input_type::INPUT_AUDIOSTAGE_EFFECT, AudioIO::getTrackTypeFromNumChannels(numChannels), -1, 0, {stage->toRef(), isInput}, effect->projectGlobalId, str};
}
}
struct track_gui_entry_t;
inline void updateProfilingTime(int64_t& field, int64_t tm, uint8_t weighting = 20) {
	field = (field * (weighting-1) + tm) / weighting;
}
struct track_impl_t : public audio_stage_t {
	midiarp* arp = nullptr;
	track_t* track;
	std::vector<note_t> heldNotes;
	VstEvent_t* midiEventsBuf = nullptr;
	DAW::channel_ref_t inputChannel;
	DAW::channel_ref_t outputChannel;
	std::vector<track_gui_entry_t*> guiInstances;
	std::vector<noteevent_t> noteEventsProcessed;
	clip_notes_t* midiProcessed = nullptr;
	ThreadMutex midiMutex;
	track_midiprocess_profiling_t procMidiStats;
	track_impl_t(vsthost* const _host, audio_stage_id_t _id, track_t* _track, const samplerate_t _sampleRate, const uint16_t _blockSize, int32_t nChannels);
	~track_impl_t();
	void sendNotesOff(int32_t bpm100) override;
	void onStartPlayback();
	void onStopPlayback() override;
	void onPlaybackJumpFromTo(int32_t fromSamplePos, double fromTickPos, int32_t toSamplePos, double toTickPos);
	void sendNotes(playback_state state, int32_t flags, tick_t cursorPos, tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos, const clip_notes_t& midiRealtimeInput);
	void processMidiOutput(playback_state state, int32_t flags, tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos);
	void fillAudio(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos, float** buffer, int32_t samples);
	void addAudio(const AudioBlock& src, float fGain);
	int32_t mapInput(int32_t nInputChannels, int32_t nChannel);
	VstEvent_t* reallocEvts(size_t size);
	void removePlugin(effectbase* _vst, bool notifyUp) override;
	const std::vector<arp_note_t>& getArpHeldNotes();
	std::vector<marker_t>& getArpMarkers(int n);
	void getAutomatableTrackTargets(std::vector<automatable_t*>& targets);
	void createIOSnapshot(track_io_configuration_snapshot_t& snapshot);
	void loadIOConfiguration(const track_io_configuration_snapshot_t& trPluginList);
};
