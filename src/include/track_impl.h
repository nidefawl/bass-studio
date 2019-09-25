#pragma once
#include <vector>
#include <array>
#include <memory>
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
#include "host/audio_config.h"

#define PARAM_TRACK_GAIN 1

struct VstEvent_t;

class vstplugin;
class effectbase;
class guictr_plugins;
struct track_params_snapshot_t;
struct audio_stage_t;
extern const std::vector<SupportedFileType> vFILE_TYPES_TRACKSNAPSHOT;
/* Calculate mixer gain level from parameter
 * returns: false if gain == -inf db */
inline bool getGainLvl(float fLinGain, float& fGainOut) {
	float fGainRaw = dsp_util::linScaleToGain(fLinGain);
	if (fGainRaw  < dsp_util::GAIN_DBFLOOR) {
		fGainOut = 0.0f;
		return false;
	}
	fGainOut = dsp_util::clampReadGain(fGainRaw);
	return true;
}
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
	void setParamValue(int32_t idx, float val, int flags) override;
	automationlane_snapshot_t toRef() override {
		automationlane_snapshot_t ref;
		ref.type = AUTOMATABLE_MIXER;
		ref.refId = 0;
		return ref;
	}
	track_t* getTrack() override;
	bool isEnabled() {
		dbgassert(getParam(PARAM_ENABLE));
		return getParam(PARAM_ENABLE)->value >= 0.5f;
	}
	void createSnapshot(track_params_snapshot_t& snapshot);
	void loadSnapshot(const track_params_snapshot_t& snapshot);
	void postSetParameter(int32_t idx, float preVal, float val, int flags);
};
struct audio_stage_t;

struct audio_stage_t {
	int32_t id;
	audio_stage_t* parent;
	effectbase* owner;
	guictr_plugins* pluginCtr;
	rmsmeterimpl<16000> meter;
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
	track_params_t mixer;
	audiotrack_t audioOutput;
	int32_t latency = 0;
	int type;
	const samplerate_t& sampleRate;
	const uint16_t& blockSize;
	std::array<DelayLine, 2> delayLines;
	std::vector<effectbase*> effects;
	std::vector<effectbase*> deferredEffects;
	std::vector<audio_stage_t*> children;
	struct latency_info_t {
		int32_t delayToPreReturn = 0;
		int32_t delayToPostReturn = 0;
	} latencyInfo;
	audio_stage_t(int32_t _id,/*track_t* _track, */const samplerate_t& _sampleRate, const uint16_t& _blockSize, int32_t nChannels, int _type = 1)
	: id(_id), parent(nullptr), owner(nullptr),/*track(_track),*/
	  pluginCtr(nullptr),
	  input(nChannels, _blockSize),
	  output(nChannels, _blockSize),
	  mixer(this),
	  type(_type),
	  sampleRate(_sampleRate),
	  blockSize(_blockSize),
	  delayLines{{DelayLine(nChannels, 0), DelayLine(nChannels, 0)}} {
	}
	virtual ~audio_stage_t() {

	}
	virtual void removePlugin(effectbase* _vst, bool notifyUp);
	void loadPlugins(const std::vector<plugin_snapshot_t>& trPluginList);
	int32_t getLatency();
	DelayLine* getDelayLine(int32_t idx) {
		dbgassert(idx >= 0 && idx < delayLines.size());
		return &delayLines[idx];
	}
	void insertEffect(int32_t idx, effectbase* _instrument);
	bool replaceEffect(int32_t idx, effectbase* _effect, effectbase** _prevEffect);
	void pluginsChanged();
	void onTick(double since);
	track_t* getTrack();
	void addAudioStage(audio_stage_t* stage);
	void removeAudioStage(audio_stage_t* stage);
	effectbase* getPluginById(int32_t projectGlobalId);
	audio_stage_ref_t toRef();
	void getStageTargets(std::vector<automatable_t*>& targets);
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
namespace MidiFlags {
static constexpr int PROCESS_REALTIME = 1;
static constexpr int PROCESS_CLIPS = 2;
static constexpr int PROCESS_ARP = 4;
}
struct channel_ref_t {
	String name = "None";
	audio_stage_ref_t stage{-1};
	int32_t inputTrackIdx = -1;
	int32_t inputChannelOffset = 0;
	AudioIO::tracktype type;
};
inline bool isChannelConnected(channel_ref_t& ch) {
	return ch.stage.id > -1 || ch.inputTrackIdx > -1;
}
inline channel_ref_t ChannelNone() {
	return channel_ref_t{};
}
inline channel_ref_t ChannelAudioInput(int32_t idx, int32_t channelOffset, String name, AudioIO::tracktype type) {
	return channel_ref_t{name, {-1}, idx, channelOffset, type};
}
inline channel_ref_t ChannelStage(audio_stage_t* stage) {
	String str = "";
	auto track = stage->getTrack();
	if (track) {
		str = track->name;
	}
	return channel_ref_t{str, stage->toRef(), -1, 0, AudioIO::getTrackTypeNumChannels(stage->input.channels)};
}
struct track_impl_t : public audio_stage_t {
	midiarp* arp = nullptr;
	track_t* track;
	std::vector<note_t> heldNotes;
	VstEvent_t* midiEventsBuf = nullptr;
	automatable_t* selectedAutomationCtr = nullptr;
	int32_t selectedAutomationParam = -1;
	std::vector<automationlane_snapshot_t> atl;
	bool wasInHide = false;
	channel_ref_t inputChannel;
	channel_ref_t outputChannel;
	track_impl_t(int32_t _id, track_t* _track, const samplerate_t& _sampleRate, const uint16_t& _blockSize, int32_t nChannels);
	~track_impl_t();
	void sendNotesOff(int32_t bpm100);
	void sendNotes(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos, clip_notes_t& midiRealtimeInput, int32_t flags);
	void fillAudio(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos, float** buffer, int32_t samples);
	void addAudio(const AudioBlock* const output, float fGain);
	int32_t mapInput(int32_t nInputChannels, int32_t nChannel);
	VstEvent_t* reallocEvts(size_t size);
	int loadSubtrackLayout(const std::vector<automationlane_snapshot_t>& atl);
	void saveSubtrackLayout(std::vector<automationlane_snapshot_t>& atl);
	void updateStoreLoadSubtracks();
	void removePlugin(effectbase* _vst, bool notifyUp) override;
	std::vector<note_t>& getArpHeldNotes();
	std::vector<note_t>& getArpInputNotes();
	std::vector<marker_t>& getArpMarkers();
	void getAutomatableTrackTargets(std::vector<automatable_t*>& targets);
};
