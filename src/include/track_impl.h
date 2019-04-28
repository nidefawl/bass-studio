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
#include "snapshot.h"

#define PARAM_TRACK_GAIN 1

struct VstEvent_t;

class vstplugin;
class effectbase;
class guictr_plugins;
struct track_params_snapshot_t;
struct audio_stage_t;

struct track_params_t : public automatable_t {
private:
	audio_stage_t* audiostage;
	struct track_param_entry_t {
		int32_t id;
		String name;
		float val;
	};

public:
	track_params_t(audio_stage_t* _audiostage)
	  : automatable_t(), audiostage(_audiostage) {
		const std::array<track_param_entry_t, 2> parameterTypes { {
			track_param_entry_t{PARAM_ENABLE, "Enabled", 1.0f},
			track_param_entry_t{PARAM_TRACK_GAIN, "Gain", 1.0f},
		} };
		for (const track_param_entry_t& paramEntry : parameterTypes) {
			automatable_param_t* regparam = registerParam(paramEntry.id);
			regparam->value = paramEntry.val;
			regparam->label = paramEntry.name;
			regparam->shortLabel = paramEntry.name;
		}
		getAutomation(PARAM_ENABLE)->quantizationSteps = 1;
	}
	const float lvlRange = dsp_util::DBFS_MUTE_POS - dsp_util::MTR_CEIL;
	const float EXP = 2.0f;
	float gainToLinScale(float f) {
		float db = dsp_util::dBFS(f);
		float f2 = ((math::max(dsp_util::DBFS_MUTE_POS, math::min(db, dsp_util::MTR_CEIL)) - dsp_util::MTR_CEIL) / lvlRange);
		return 1.0f - powf(f2, 1.0/EXP);
	}
	float linScaleToGain(float f) {
		float f1 = (1.0f-f);
		f1 = powf(f1, EXP);
		float f2 = (f1 * lvlRange)+dsp_util::MTR_CEIL;
		return dsp_util::fromdBFS(f2);
	}
	float convertValFrom(int32_t idx, float f) {
		if (idx == 1) {
			float f2 = gainToLinScale(f);
			return f2;
		}
		return f;
	}
	float convertValTo(int32_t idx, float f) {
		if (idx == 1) {
			return linScaleToGain(f);
		}
		return f;
	}
	String getAutomatableName() override {
		return "Mixer";
	}
	float getParamValue(int32_t idx) override {
		automatable_param_t* param = getParam(idx);
		assert(param);
		return convertValFrom(idx, param->value);
	}
	void setParamValue(int32_t idx, float val, int flags) override {
		automatable_param_t* param = getParam(idx);
		assert(param);
		param->value = convertValTo(idx, val);
	}
	automationlane_snapshot_t toRef() override {
		automationlane_snapshot_t ref;
		ref.type = AUTOMATABLE_MIXER;
		ref.refId = 0;
		return ref;
	}
	void createSnapshot(track_params_snapshot_t& snapshot);
	void loadSnapshot(const track_params_snapshot_t& snapshot);
	float getGain() {
		return getParamValue(PARAM_TRACK_GAIN);
	}
	void setGain(float f) {
		setParamValue(PARAM_TRACK_GAIN, f, FLG_PAR_UPDATE_USER);
	}
	bool isEnabled() {
		return getParamValue(PARAM_ENABLE) >= 0.5f;
	}
	void postSetParameter(int32_t idx, float preVal, float val, int flags);
};
struct audio_stage_ref_t {
	int id;
};
struct audio_stage_t;
//class audio_stage_holder_t {
//public:
//	std::vector<audio_stage_t*> stages;
//	virtual ~audio_stage_holder_t() { };
//	virtual void onStagesCreated();
//};
struct audio_stage_t {
	int32_t id;
	audio_stage_t* parent;
	effectbase* owner;
	guictr_plugins* pluginCtr;
	rmsmeter<16000> meter;
	AudioBlock input; //guaranteed to have at least 2 channels
	AudioBlock output; //guaranteed to have at least 2 channels
	DelayLine delayLine;
	track_params_t mixer;
	int32_t latency = 0;
	int type;
	const samplerate_t& sampleRate;
	const uint16_t& blockSize;
	std::vector<effectbase*> effects;
	std::vector<effectbase*> deferredEffects;
	std::vector<audio_stage_t*> children;
	audio_stage_t(int32_t _id,/*track_t* _track, */const samplerate_t& _sampleRate, const uint16_t& _blockSize, int32_t nChannels, int _type = 1)
	: id(_id), parent(nullptr), owner(nullptr),/*track(_track),*/
	  pluginCtr(nullptr),
	  input(nChannels, _blockSize),
	  output(nChannels, _blockSize),
	  delayLine(nChannels, _blockSize),
	  mixer(this),
	  type(_type),
	  sampleRate(_sampleRate),
	  blockSize(_blockSize) {
	}
	virtual ~audio_stage_t() {

	}
	virtual void removePlugin(effectbase* _vst, bool notifyUp);
	void loadPlugins(const std::vector<plugin_snapshot_t>& trPluginList);
	int32_t getLatency();
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
class midiarp;
struct track_impl_t : public audio_stage_t {
	midiarp* arp = nullptr;
	track_t* track;
	std::vector<note_t> heldNotes;
	VstEvent_t* midiEventsBuf = nullptr;
	automatable_t* selectedAutomationCtr = nullptr;
	int32_t selectedAutomationParam = -1;
	std::vector<automationlane_snapshot_t> atl;
	bool wasInHide = false;
	track_impl_t(int32_t _id, track_t* _track, const samplerate_t& _sampleRate, const uint16_t& _blockSize, int32_t nChannels);
	~track_impl_t();
	void sendNotesOff(int32_t bpm100, int32_t blockSamplePos);
	void sendNotes(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos);
	void fillAudio(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos, float** buffer, int32_t samples);
	VstEvent_t* reallocEvts(size_t size);
	void loadSubtrackLayout(const std::vector<automationlane_snapshot_t>& atl);
	void saveSubtrackLayout(std::vector<automationlane_snapshot_t>& atl);
	void updateStoreLoadSubtracks();
	void removePlugin(effectbase* _vst, bool notifyUp) override;
	std::vector<note_t>& getArpHeldNotes();
	std::vector<note_t>& getArpInputNotes();
	std::vector<marker_t>& getArpMarkers();
	void getAutomatableTrackTargets(std::vector<automatable_t*>& targets);
};
