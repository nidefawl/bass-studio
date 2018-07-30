#pragma once
#include "config.h"
#include <vector>
#include <array>
#include <memory>
#include "automation.h"
#include "meter.h"
#include "audioblock.h"
#include "samplerate.h"
#include "note.h"
#include "dsp_util.h"
#include "leak_detect.h"
#include "snapshot.h"

struct VstEvent_t;

class vstplugin;
class effectbase;
class guictr_plugins;
struct track_params_snapshot_t;

struct track_params_t : public automatable_t {
private:
	struct track_param_entry_t {
		String name;
		float val;
	};

public:
	track_params_t()
	  : automatable_t() {
		int32_t idx = 0;
		const std::array<track_param_entry_t, 2> parameterTypes { {
			track_param_entry_t{"Enabled", 1.0f},
			track_param_entry_t{"Gain", 1.0f},
		} };
		params.reserve(parameterTypes.size());
		for (const track_param_entry_t& paramEntry : parameterTypes) {
			automatable_param_t automatable{0};
			automatable.idx = idx;
			automatable.internalIdx = -1;
			automatable.category = 0;
			automatable.value = paramEntry.val;
			automatable.label = paramEntry.name;
			automatable.shortLabel = paramEntry.name;
			params.push_back(std::move(automatable));
			idx++;
		}
		getAutomation(0)->quantizationSteps = 1;
	}
	const float lvlRange = dsp_util::DBFS_MUTE_POS - dsp_util::MTR_CEIL;
	const float EXP = 2.0f;
	float gainToLinScale(float f) {
		float db = dsp_util::dBFS(f);
		float f2 = ((max(dsp_util::DBFS_MUTE_POS, min(db, dsp_util::MTR_CEIL)) - dsp_util::MTR_CEIL) / lvlRange);
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
		if (idx >= 0 && idx < (int)params.size()) {
			return convertValFrom(idx, params[idx].value);
		}
		return 0.0f;
	}
	void setParamValue(int32_t idx, float val) override {
		if (idx >= 0 && idx < (int)params.size()) {
			params[idx].value = convertValTo(idx, val);
		}
	}
	automationlane_snapshot_t toRef() override {
		automationlane_snapshot_t ref;
		ref.type = 1;
		ref.refId = 0;
		return ref;
	}
	void createSnapshot(track_params_snapshot_t& snapshot);
	void loadSnapshot(const track_params_snapshot_t& snapshot);
	float getGain() {
		return params[1].value;
	}
	void setGain(float f) {
		params[1].value = f;
	}
	bool isEnabled() {
		return params[0].value >= 0.5f;
	}
};
struct audio_stage_t {
	audio_stage_t* parent;
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
	std::vector<audio_stage_t*> children;
	audio_stage_t(/*track_t* _track, */const samplerate_t& _sampleRate, const uint16_t& _blockSize, int32_t nChannels, int _type = 1)
	: parent(nullptr),/*track(_track),*/
	  pluginCtr(nullptr),
	  input(nChannels, _blockSize),
	  output(nChannels, _blockSize),
	  delayLine(nChannels, _blockSize),
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
	void pluginsChanged();
	void onTick(double since);
	track_t* getTrack();
	void addAudioStage(audio_stage_t* stage);
	void removeAudioStage(audio_stage_t* stage);
};
class midiarp;
struct track_impl_t : public audio_stage_t {
	midiarp* arp = nullptr;
	track_t* track;
	std::vector<note_t> heldNotes;
	VstEvent_t* midiEventsBuf = nullptr;
	automatable_t* selectedAutomationCtr = nullptr;
	int32_t selectedAutomationParam = -1;
	std::vector<automationlane_snapshot_t> atl;
	bool atlStored = false;
	track_impl_t(track_t* _track, const samplerate_t& _sampleRate, const uint16_t& _blockSize, int32_t nChannels);
	~track_impl_t();
	effectbase* getPluginById(int32_t projectGlobalId);
	void sendNotesOff(int32_t bpm100, int32_t blockSamplePos);
	void sendNotes(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos);
	void fillAudio(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos, float** buffer, int32_t samples);
	VstEvent_t* reallocEvts(size_t size);
	void getAutomatableTargets(std::vector<automatable_t*>& targets);
	void loadAutomationLanes(const std::vector<automationlane_snapshot_t>& atl);
	void saveAutomationLanes(std::vector<automationlane_snapshot_t>& atl);
	void showAutomationLanes();
	void removePlugin(effectbase* _vst, bool notifyUp) override;
	std::vector<note_t>& getArpHeldNotes();
	std::vector<note_t>& getArpInputNotes();
	std::vector<marker_t>& getArpMarkers();
};
