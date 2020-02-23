#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include "samplerate.h"

#include "project.h"
#include "vst_host.h"
#include "fileio.h"
#include "track.h"
#include "basectrl.h"
#include "host/mainctrl.h"

#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"
#include "vst_window.h"

#include "../vstsdk-host-2.4/aeffectx.h"
#include "appsettings.h"

#include "logging.h"
#include "audioblock.h"
#include "audiobuffer.h"
#include "platform.h"
#include "audio_host.h"
#include "midi_host.h"
#include "midi-defs.h"
#include "midi-msg.h"

#include <stdlib.h>
#include <algorithm>
#include "assert_dbg.h"
#include <stdlib.h>
#include <memory.h>
#include "track_impl.h"
#include "projectcontroller.h"
#include "threads/threadlock.h"
#include "track_graph.h"
#include "resampler.h"

#include <deque>

#ifdef _WIN32
#include <windows.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
//#include "../util/readerwriterqueue.h"

//#define DBG_PRINT_CALLBACKS
#ifdef DBG_PRINT_CALLBACKS
#define MAX_LEN_MY_DBF 512
bool filterOpCode(int opcode) {
//	return opcode == audioMasterUpdateDisplay;
	if ( opcode == audioMasterSizeWindow)
		return true;
	if ( opcode == audioMasterBeginEdit)
		return true;
	if ( opcode == audioMasterEndEdit)
		return true;
	if ( opcode == audioMasterAutomate)
		return true;
	if ( opcode == audioMasterGetInputLatency)
		return false;
	if ( opcode == audioMasterGetOutputLatency)
		return false;
	return true;
}
void cbPrintf(vstplugin* plugin, const char *fmt, int index, int opcode, int value);
void cbPrintf(vstplugin* plugin, const char *fmt, int index, int opcode, int value) {
	if (filterOpCode(opcode)) {
		char buf[MAX_LEN_MY_DBF];
		snprintf(buf, MAX_LEN_MY_DBF - 1, fmt, index, opcode, value);
		my_printf("%s %s", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), buf);
	}
}
#else
void emptyPrinft(vstplugin* plugin, const char *fmt, ...) {
}
#define cbPrintf emptyPrinft
#endif


#define NUM_HOST_CB_SLOTS 4
namespace
{
struct vst_internal_hostslot {
	vsthost* g_instance = nullptr;
};
vst_internal_hostslot g_hostslots[4];
}

VstIntPtr audioMasterHost(vsthost* host, AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	dbgassert(host);
	if (!host)
		return 0;
	vstplugin* plugin = host->getPlugin(effect);

	switch (opcode)
	{
	case audioMasterAutomate:
		cbPrintf(plugin, "audioMasterAutomate %d %d %d\n", index, opcode, value);
		if (plugin) {
			auto* effParam = plugin->getEffectParam(index);
			if (!effParam) {
				log_printf("%s audioMasterAutomate unknown param index %d\n", StringAsCStr(plugin->getName()), index);
			} else {
				plugin->deactivateAutomation(effParam->idx);
				plugin->recvPluginEditParamUpdate(effParam->internalIdx);
			}
		}
		//return OnSetParameterAutomated(nEffect, index, opt);
		return 1;
	case audioMasterVersion:
		cbPrintf(plugin, "audioMasterVersion %d %d %d\n", index, opcode, value);
		//return OnGetVersion(nEffect);
		return 2400L;
	case audioMasterCurrentId:
		cbPrintf(plugin, "audioMasterCurrentId %d %d %d\n", index, opcode, value);
		//return OnGetCurrentUniqueId(nEffect);
		return 0L;
	case audioMasterIdle:
//		cbPrintf(plugin, "audioMasterIdle %d %d %d\n", index, opcode, value);
		//return OnIdle(nEffect);
		return 0L;
	case audioMasterGetTime:
		//cbPrintf(plugin, "audioMasterGetTime %d %d %d\n", index, opcode, value);
		return (VstIntPtr)host->getTimeInfo();
	case audioMasterProcessEvents:
//		cbPrintf(plugin, "audioMasterProcessEvents %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterIOChanged:
		cbPrintf(plugin, "audioMasterIOChanged %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterSizeWindow:
		cbPrintf(plugin, "audioMasterSizeWindow %d %d %d\n", index, opcode, value);
		if (plugin) {
			plugin->updateWindowSize();
		}
		return 1;
	case audioMasterGetSampleRate:
		cbPrintf(plugin, "audioMasterGetSampleRate %d %d %d\n", index, opcode, value);
		if (plugin) {
			return (long)plugin->format.sampleRate;
		}
		if (host) {
			return (long)host->sampleFormat.sampleRate;
		}
		return 0;
	case audioMasterGetBlockSize:
		cbPrintf(plugin, "audioMasterGetBlockSize %d %d %d\n", index, opcode, value);
		if (plugin) {
			return (long)plugin->format.blockSize;
		}
		if (host) {
			return (long)host->sampleFormat.blockSize;
		}
		return 0;
	case audioMasterGetInputLatency:
		cbPrintf(plugin, "audioMasterGetInputLatency %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterGetOutputLatency:
		cbPrintf(plugin, "audioMasterGetOutputLatency %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterGetCurrentProcessLevel:
//		cbPrintf(plugin, "audioMasterGetCurrentProcessLevel %d %d %d\n", index, opcode, value);
		return VstProcessLevels::kVstProcessLevelRealtime;
	case audioMasterGetAutomationState:
		cbPrintf(plugin, "audioMasterGetAutomationState %d %d %d\n", index, opcode, value);
		return kVstAutomationReadWrite;
	case audioMasterOfflineStart:
		cbPrintf(plugin, "audioMasterOfflineStart %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterOfflineRead:
		cbPrintf(plugin, "audioMasterOfflineRead %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterOfflineWrite:
		cbPrintf(plugin, "audioMasterOfflineWrite %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterOfflineGetCurrentPass:
		cbPrintf(plugin, "audioMasterOfflineGetCurrentPass %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterOfflineGetCurrentMetaPass:
		cbPrintf(plugin, "audioMasterOfflineGetCurrentMetaPass %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterGetVendorString:
		cbPrintf(plugin, "audioMasterGetVendorString %d %d %d\n", index, opcode, value);
		 strcpy((char *)ptr, "Seib");
		return 1L;
	case audioMasterGetProductString:
		cbPrintf(plugin, "audioMasterGetProductString %d %d %d\n", index, opcode, value);
		strcpy((char *)ptr, "Default CVSTHost");
		return 1L;
	case audioMasterGetVendorVersion:
		cbPrintf(plugin, "audioMasterGetVendorVersion %d %d %d\n", index, opcode, value);
		return 1L;
	case audioMasterVendorSpecific:
		cbPrintf(plugin, "audioMasterVendorSpecific %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterCanDo:
		cbPrintf(plugin, "audioMasterCanDo %d %d %d\n", index, opcode, value);
		return host->canDo((const char*)ptr);
	case audioMasterGetLanguage:
		cbPrintf(plugin, "audioMasterGetLanguage %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterGetDirectory:
		if (plugin == NULL) {
			cbPrintf(plugin, "audioMasterGetDirectory plugin == NULL %d %d %d\n", index, opcode, value);
			return 0;
		}
		cbPrintf(plugin, "audioMasterGetDirectory %d %d %d\n", index, opcode, value);
		return (VstIntPtr)plugin->getDir();
	case audioMasterUpdateDisplay:
		if (plugin == NULL) {
			cbPrintf(plugin, "audioMasterUpdateDisplay plugin == NULL %d %d %d\n", index, opcode, value);
			return 0;
		}
		cbPrintf(plugin, "audioMasterUpdateDisplay %d %d %d\n", index, opcode, value);

		return (VstIntPtr)plugin->updateWindow();
#ifdef VST_2_1_EXTENSIONS
	case audioMasterBeginEdit:
		cbPrintf(plugin, "audioMasterBeginEdit %d %d %d\n", index, opcode, value);
		return 1;
	case audioMasterEndEdit:
		cbPrintf(plugin, "audioMasterEndEdit %d %d %d\n", index, opcode, value);
		return 1;
	case audioMasterOpenFileSelector:
		cbPrintf(plugin, "audioMasterOpenFileSelector %d %d %d\n", index, opcode, value);
		return 0;
#endif
#ifdef VST_2_2_EXTENSIONS
	case audioMasterCloseFileSelector:
		cbPrintf(plugin, "audioMasterCloseFileSelector %d %d %d\n", index, opcode, value);
		return 0;
#endif
	case audioMasterWantMidi:
		cbPrintf(plugin, "depr audioMasterWantMidi %d %d %d\n", index, opcode, value);
		return 0;
	default:
		cbPrintf(plugin, "unhandled %d %d %d\n", index, opcode, value);

	}
	return 0L;
}
VstIntPtr VSTCALLBACK audioMaster1(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	vsthost* host = g_hostslots[0].g_instance;
	dbgassert(host);
	return audioMasterHost(host, effect, opcode, index, value, ptr, opt);
}
VstIntPtr VSTCALLBACK audioMaster2(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	vsthost* host = g_hostslots[1].g_instance;
	dbgassert(host);
	return audioMasterHost(host, effect, opcode, index, value, ptr, opt);
}
VstIntPtr VSTCALLBACK audioMaster3(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	vsthost* host = g_hostslots[2].g_instance;
	dbgassert(host);
	return audioMasterHost(host, effect, opcode, index, value, ptr, opt);
}
VstIntPtr VSTCALLBACK audioMaster4(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	vsthost* host = g_hostslots[3].g_instance;
	dbgassert(host);
	return audioMasterHost(host, effect, opcode, index, value, ptr, opt);
}

static const double fSmpteDiv[] =
{
	24.f,
	25.f,
	24.f,
	30.f,
	29.97f,
	30.f
};
bool setFlag(int& _out, int flag, bool state) {
	bool curState = _out&flag;
	if (state) {
		_out |= flag;
	} else {
		_out &= ~flag;
	}
	return curState != state;
}
#ifdef _WIN32
String getModuleName(HMODULE);
#endif
class vsthost::ModuleManager {
public:
	ModuleManager() {

	}

	void releaseModule(void* module) {
#ifdef _WIN32
		String moduleName = getModuleName((HMODULE)module);
		my_printf("Unload %s\n", StringAsCStr(moduleName));
		FreeLibrary((HMODULE)module);
#endif
#if defined(__linux__) || defined(__APPLE__)
		dlclose(module);
#endif
	}
};

class vsthost::vsthost_impl {
public:
	std::vector<std::shared_ptr<resampler_t>> resamplers;
	std::shared_ptr<oversampler_t> oversampler;

	std::shared_ptr<resampler_t> getResampler(sampleformat_t in, sampleformat_t out, int32_t idx) {
		auto it = std::find_if(resamplers.begin(), resamplers.end(), [&in,&out,idx](std::shared_ptr<resampler_t>& ptr){
			return ptr->in == in && ptr->out == out && ptr->idx == idx;
		});
		if (it == resamplers.end()) {

			oversample_config_t config;
			config.inputSampleRate = in.sampleRate;
			config.outputSampleRate = out.sampleRate;
			config.numChannels = 32;
			config.setInputLength(in.blockSize);
			std::shared_ptr<resampler_t> resampler = std::make_shared<resampler_t>(idx, in, out, config);
			resamplers.push_back(resampler);
			return resampler;

		}
		return *it;
	}
	vsthost_impl() {

	}
	~vsthost_impl() {

	}
};

vsthost::~vsthost() {
	delete moduleMgr;
	delete blockZero;
	delete impl;
}
vsthost::vsthost()
	: impl(new vsthost_impl{}), numChannels(OUTPUT_CHANNELS), moduleMgr{new vsthost::ModuleManager{}}
{
	memset(&timeinfo, 0, sizeof(timeinfo));
	allocRingBuffer(ringbuffer, 32);
	updateTime(0, 0.0, playback_state::status_stop);
	midiRealtimeInput = new clip_notes_t;
	registerPlugins();
}
void vsthost::setSampleFormat(const sampleformat_t& sampleFormat) {
	if (this->sampleFormat != sampleFormat) {
		this->sampleFormat = sampleFormat;
		for (vstplugin* plugin : this->pluginInstancesVST2) {
			plugin->sleep();
		}
		setBlockSize(sampleFormat.blockSize);
		for (auto* audio : this->allAudioStages) {
			audio->sampleFormat = sampleFormat;
			audio->input.realloc(sampleFormat.blockSize);
			audio->output.realloc(sampleFormat.blockSize);
			audio->outputPost.realloc(sampleFormat.blockSize);
		}
		for (effectbase* plugin : this->pluginInstances) {
			plugin->setSampleFormat(sampleFormat);
		}
		for (vstplugin* plugin : this->pluginInstancesVST2) {
			plugin->dispatch(effSetBlockSize, 0, sampleFormat.blockSize, 0, 0);
			plugin->dispatch(effSetSampleRate, 0, 0, NULL, (float) sampleFormat.sampleRate);
		}
		for (vstplugin* plugin : this->pluginInstancesVST2) {
			plugin->resume();
		}
		for (auto* stage: this->allAudioStages) {
			stage->pluginsChanged();
		}
	}

}
void vsthost::setBlockSize(uint16_t _blockSize) {
	if (!blockZero)
		blockZero = new AudioBlock(numChannels, _blockSize);
	this->blockZero->realloc(_blockSize);
}

inline tick_t blockToPPQ24TickRounded(int32_t block, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double seconds = (block * blocksize) / (double)samplerate;
	return (tick_t) std::round((seconds*bpm100*24.0) / 6000.0);
}
inline tick_t tick4096ToPPQ24TickRounded(tick_t tick4096, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double seconds = toSeconds(tick4096, bpm100);
	return (tick_t) std::round((seconds*bpm100*24.0) / 6000.0);
}
inline double tick4096ToPPQ24TickPrecise(tick_t tick4096, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double seconds = toSeconds(tick4096, bpm100);
	return (seconds*bpm100*24.0) / 6000.0;
}
inline int32_t PPQ24TickSample(tick_t tick, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double seconds = ((tick)/(double)(bpm100*24.0)) * 100.0 * 60.0;
	double samplePos = seconds * samplerate;
	return (int32_t) floor(samplePos);
}


//\note VstTimeInfo::samplesToNextClock :
//MIDI Clock Resolution (24 per Quarter Note), can be negative the distance to the next midi clock
//		(24 ppq, pulses per quarter) in samples. unless samplePos falls precicely on a midi clock,
//		this will either be negative such that the previous MIDI clock is addressed,
//		or positive when referencing the following (future) MIDI clock.
void vsthost::updateTime(int32_t samplePos, double dTickPos, playback_state state) {
	timeinfo.samplePos = samplePos;
	timeinfo.sampleRate = (double)sampleFormat.sampleRate;
	timeinfo.nanoSeconds = (double)getTimeMillis() * 1000000.0L;
	timeinfo.ppqPos = (dTickPos/(double)TICKS_QUARTER);
	timeinfo.tempo = project.tempo100/100.0;
	timeinfo.barStartPos = floor(dTickPos / (double) TICKS_BAR) * 4;
	timeinfo.cycleStartPos = (project.loopStart/(double)TICKS_QUARTER);
	timeinfo.cycleEndPos = ((project.loopStart+project.loopLen)/(double)TICKS_QUARTER);
	timeinfo.timeSigNumerator = project.signatureNum;
	timeinfo.timeSigDenominator = 1 << project.signatureDenom;
	if (project.loopEnabled) {
	} else {
		timeinfo.cycleStartPos = 0;
		timeinfo.cycleEndPos = 0;
	}
	{

		double dPosSeconds = timeinfo.samplePos / timeinfo.sampleRate;
		/* offset in fractions of a second   */
		double dOffsetInSecond = dPosSeconds - floor(dPosSeconds);
		timeinfo.smpteFrameRate = VstSmpteFrameRate::kVstSmpte24fps;
		timeinfo.smpteOffset = (long)(dOffsetInSecond * fSmpteDiv[timeinfo.smpteFrameRate] * 80.L);
	}


	double midiTickPPQ24 = timeinfo.ppqPos*24.0;
	tick_t midiTick = std::round(midiTickPPQ24);
	int32_t samplePosTick = PPQ24TickSample(midiTick, project.tempo100, sampleFormat.sampleRate, sampleFormat.blockSize);
	timeinfo.samplesToNextClock = samplePosTick - timeinfo.samplePos;

	{

		bool changed;
		changed = setFlag(timeinfo.flags, kVstTransportPlaying, state == playback_state::status_play);
		changed |= setFlag(timeinfo.flags, kVstTransportCycleActive, project.loopEnabled);
		changed |= setFlag(timeinfo.flags, kVstTransportRecording, false);
		setFlag(timeinfo.flags, kVstTransportChanged, changed);
		setFlag(timeinfo.flags, kVstAutomationWriting, false);
		setFlag(timeinfo.flags, kVstAutomationReading, false);
		setFlag(timeinfo.flags, kVstNanosValid, true);
		setFlag(timeinfo.flags, kVstPpqPosValid, true);
		setFlag(timeinfo.flags, kVstTempoValid, true);
		setFlag(timeinfo.flags, kVstBarsValid, true);
		setFlag(timeinfo.flags, kVstCyclePosValid, true); //project.loopEnabled
		setFlag(timeinfo.flags, kVstTimeSigValid, true);
		setFlag(timeinfo.flags, kVstSmpteValid, true);
		setFlag(timeinfo.flags, kVstClockValid, true);
	}

}

void vsthost::sendNotesOff(effectbase* plugin) {
	//TODO: check current thread, check if playthread is locked
	if (plugin && plugin->trackImpl) {
		track_t* tr = plugin->trackImpl->getTrack();
		dbgassert(tr);
		track_impl_t* audio = tr->audio;
		if (audio) {
			audio->sendNotesOff(project.tempo100);
		}
	}
}
std::vector<note_t> vsthost::getRealtimeNotes() {
	return this->midiRealtimeInput->m_list;
}
void delayAudio(DelayLine* delayLine, AudioBlock* input, AudioBlock* output, samplerate_t delay) {
	dbgassert(delay >= 0 && delay < 1<<20);
	dbgassert(delayLine);
	int32_t bufSize = (int32_t)input->samples;
	int32_t bufDelay = delay;
	int32_t numBlocks = 1;
	while (bufDelay > 0) {
		bufDelay -= bufSize;
		numBlocks++;
	}
	int32_t delayLineSize = numBlocks*bufSize;
	delayLine->blockOffset = (delayLine->blockOffset+1)%numBlocks;
	int32_t writePos = delayLine->blockOffset*bufSize;
	int32_t readPos = writePos - delay;
	if (readPos < 0) {
		readPos += delayLineSize;
	}
	AudioBlock& delayBlock = delayLine->block;
	delayBlock.realloc(delayLineSize);
	delayBlock.copyFromPosToPos(input->buf, 0, writePos, input->samples, input->channels);
	if (readPos + (int32_t)output->samples > delayLineSize) {
		int32_t read1Len = delayLineSize - readPos;
		int32_t read2Len = output->samples - read1Len;
		output->copyFromPosToPos(delayBlock.buf, readPos, 0, read1Len, delayBlock.channels);
		output->copyFromPosToPos(delayBlock.buf, 0, read1Len, read2Len, delayBlock.channels);
	} else {
		output->copyFromPosToPos(delayBlock.buf, readPos, 0, output->samples, delayBlock.channels);
	}

}
namespace DAW {

bool resolveDefaultConnection(const vsthost* const host, const project_t* const project, track_impl_t* const trImpl, const bool isInput, channel_ref_t& out) {
	if (!isInput && TRACKTYPE_TO_CTR(trImpl->track->type) == TRACK_CTR_MASTER) {
		int32_t idx = 0;
		auto type = AudioIO::getTrackTypeNumChannels(trImpl->outputPost.channels);
		String name = "External "+AudioIO::getTrackNameShort(type, idx, isInput);
		out = ChannelAudioInput(idx, 0, name, type);
		return true;
	}
	const track_t* const firstMaster = project->trackMasterCtr.size() ? project->trackMasterCtr.front() : nullptr;
	if (!isInput && TRACKTYPE_TO_CTR(trImpl->track->type) == TRACK_CTR_RETURN) {
		if (firstMaster) {
			out = ChannelStage(firstMaster->audio, true);
			return true;
		}
	}
	if (!isInput && TRACKTYPE_TO_CTR(trImpl->track->type) == TRACK_CTR_MIDIAUDIO) {
		const track_t* const dstTrack = trImpl->track->parent ? trImpl->track->parent : firstMaster;
		if (dstTrack) {
			out = ChannelStage(dstTrack->audio, true);
			return true;
		}
	}
	return false;
}
bool resolveAudioChannel(const vsthost* const host, int32_t numChannelsTrack, const channel_ref_t& inputChannel, const AudioBlock* const ptrExternalInputs, track_audio_src& out) {
	if (inputChannel.getType() == channel_input_type::INPUT_EXTERNAL_AUDIO) {
		if (ptrExternalInputs != nullptr) {
			int32_t idx = inputChannel.inputChannelOffset;
			size_t size = math::min<uint32_t>(AudioIO::getNumChannelsTrackType(inputChannel.externalInputType), numChannelsTrack);
			if (idx >= 0 && idx+size <= ptrExternalInputs->channels) {
				track_audio_src src;
				for (int i = 0; i < size; i++) {
					src.channels.push_back(ptrExternalInputs->buf[idx+i]);
				}
				src.sampleFormat = host->sampleFormatExternal;
				src.samples = ptrExternalInputs->samples;
				src.gain = 1.0f;
				src.latency = 0;
				out = std::move(src);
				return true;
			}
		}
	}
	if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
		audio_stage_t* stage = host->getAudioStage(inputChannel.stage.stageRef);
		if (stage) {
			/* Calculate audio/midi tracks gain level */
			float fGainTrack;
			if (!getGainLvl(stage->mixer.getParamValue(PARAM_TRACK_GAIN), fGainTrack)) {
				fGainTrack = 0.0f;
			}
			track_audio_src src;
			auto& buff = inputChannel.stage.isInput ? stage->input : stage->outputPost;
			for (int i = 0; i < buff.channels; i++) {
				src.channels.push_back(buff.buf[i]);
			}
			src.sampleFormat = stage->sampleFormat;
			src.samples = buff.samples;
			src.gain = fGainTrack;
			src.latency = 0;
			out = std::move(src);
			return true;
		}
	}
	return false;
};
}
void vsthost::processMidiRealtimeInput(project_controller_t* ctrl, double posDouble, playback_state state) {
	const sampleformat_t sampleFormat = this->sampleFormat;
	const double ticksPerBlock = toTickPrecise(sampleFormat.blockSize/(double)sampleFormat.sampleRate, project.tempo100);
	tick_t tickPosBlockStart = ceil(posDouble);

	int queueSizeOutput = 0;
	auto *stream = audioHost ? audioHost->getStream(0) : nullptr;
	if (stream) {
		queueSizeOutput = stream->getOutputQueueSize();
	}
	std::vector<MidiIOEvent> msgs = midihost::getInstance()->getInputMessages();
	bool notesProcessed = false;
	//TODO: This needs to be done per input and per track
	int32_t lenTicksInfinite = TICKS_BAR*16;
	if (msgs.size()) {


		std::vector<note_t> newNotes;
		for (MidiIOEvent& msg : msgs) {

			int32_t command = MidiMsgStatus(msg.message) & MIDI_CODE_MASK;
//			int32_t chan = MidiMsgStatus(msg.message) & MIDI_CHN_MASK;
			if (command == MIDI_ON_NOTE && MidiMsgData2(msg.message) != 0) {
				note_t note;
				note.setRealtime(true);
				note.time = tickPosBlockStart;
				note.len = lenTicksInfinite;
				note.pitch = MidiMsgData1(msg.message);
				note.velocity = MidiMsgData2(msg.message);
				newNotes.push_back(note);
			}
		}
		if (newNotes.size()) {
//			for (auto& note : newNotes) {
//				log_printf("%d note TRIG %s %d\n", noteName(note.pitch), note.start());
//			}
			midiRealtimeInput->addAll(newNotes);
		}
		for (MidiIOEvent& msg : msgs) {

			int32_t command = MidiMsgStatus(msg.message) & MIDI_CODE_MASK;
//			int32_t chan = MidiMsgStatus(msg.message) & MIDI_CHN_MASK;
			if ((command == MIDI_ON_NOTE && MidiMsgData2(msg.message) == 0) || command == MIDI_OFF_NOTE) {
				int32_t pitch = MidiMsgData1(msg.message);
				int32_t tickEnd = tickPosBlockStart;
				// kill oldest (first) note
				bool fnd = false;
				for (note_t& noteHeld : midiRealtimeInput->m_list) {
					if(noteHeld.pitch == pitch) {
						if (noteHeld.len != lenTicksInfinite) {
							log_printf("%s note was released before, looking for next one\n", noteName(noteHeld.pitch));
							continue;
						}
						if (noteHeld.start() > tickEnd) {
							log_printf("%s note starts after this release (tickEnd %d, noteHeld.start() %d)\n",
									noteName(noteHeld.pitch), tickEnd, noteHeld.start());
							continue;
						}
						if (noteHeld.start() == tickEnd) {
//							log_printf("%s noteHeld.start() == tickEnd %d, adding TICKS_16TH/4\n", noteName(noteHeld.pitch), tickEnd);
							tickEnd += TICKS_16TH/4;
						}
						noteHeld.len = tickEnd - noteHeld.start();
						assert(noteHeld.len >= 0);
						fnd = true;
						notesProcessed = true;
//						log_printf("%d note KILL %s %d\n", noteName(noteHeld.pitch), noteHeld.start());
						break;
					}
				}
				if (!fnd) {
					log_printf("MIDI_OFF_NOTE note not found %s tickEnd %d\n", noteName(pitch), tickEnd);
				}

			}
		}
		if (newNotes.size() || notesProcessed) {
			midiRealtimeInput->removeDuplicates();
			notesProcessed = true;
		}
	}
	if (midiRealtimeInput->m_list.size()) {
		auto it = midiRealtimeInput->m_list.begin();
		while (it != midiRealtimeInput->m_list.end()) {
			note_t& note = *it;
			if (note.end() < posDouble) {
				notesProcessed = true;
				it = midiRealtimeInput->m_list.erase(it);
			} else {
				it++;
			}
		}
	}
	if (notesProcessed) {
		midiRealtimeInput->updateBounds();
		if (state == playback_state::status_play && project.recordArmed) {
			if (ctrl->trackMidiAudioCtr.size()) {
				if (recordingClip == nullptr) {
					recordingClip = new clip_t;
					recordingClip->name = "Midi Input - Recorded";
					recordingClip->time = tickPosBlockStart;
					recordingClip->setLen(ticksPerBlock*4);
					recordingClip->loopStart = 0;
					recordingClip->loopLen = TICKS_BAR*4;
				}
				if (recordingClip) {
					if (recordingClip->start() > tickPosBlockStart) {
						recordingClip->time = tickPosBlockStart;
					}
					if (recordingClip->end() < tickPosBlockStart+ticksPerBlock) {
						recordingClip->setLen((tickPosBlockStart+ticksPerBlock)-recordingClip->start());
					}
					for (auto& note : midiRealtimeInput->m_list) {
						if (note.len != lenTicksInfinite) {

							auto noteCopy = note;
							noteCopy.time -= recordingClip->start();
							noteCopy.setEnabled(true);
							noteCopy.setRealtime(false);
							recordingClip->notes.addSingle(noteCopy);
						}
					}
					clip_t* cloned = recordingClip->clone();
					cloned->setLen(tickPosBlockStart - recordingClip->time);
					cloned->loopEnabled = false;
					cloned->loopLen = ( ( math::max ( 1, cloned->getLen() / (TICKS_BAR*4) ) )   * (TICKS_BAR*4) );
					cloned->notes.updateBounds();
					cloned->setDirty();
					std::swap(recordDataProcessed, cloned);
					delete cloned;
					hasNewRecordedData = true;
				}

			}
		}
	}
	if (!midiRealtimeInput->m_list.empty()) {
//		log_printf("Realtime midi notes %d\n", midiRealtimeInput->m_list.size());
	}
	if (recordingClip && !(state == playback_state::status_play && project.recordArmed)) {
		for (auto& note : midiRealtimeInput->m_list) {
			note_t noteCopy = note;
			if (noteCopy.time < tickPosBlockStart && noteCopy.len == lenTicksInfinite) {
				noteCopy.len = tickPosBlockStart - noteCopy.time;
			}
			if (noteCopy.len > 0 && noteCopy.len != lenTicksInfinite) {
				noteCopy.time -= recordingClip->start();
				noteCopy.setEnabled(true);
				noteCopy.setRealtime(false);
				recordingClip->notes.addSingle(noteCopy);
			}
		}
		clip_t* cloned = recordingClip->clone();
		tick_t clipLen = tickPosBlockStart - recordingClip->time + ticksPerBlock;
		tick_t loopLen = ( ( math::max ( 1, clipLen / (TICKS_BAR*4) ) )   * (TICKS_BAR*4) );


		cloned->loopEnabled = false;
		cloned->setLen(clipLen);
		cloned->loopLen = loopLen;
		cloned->notes.updateBounds();
		cloned->setDirty();
		std::swap(recordDataProcessed, cloned);
		delete cloned;
		hasNewRecordedData = true;
		delete recordingClip;
		recordingClip = nullptr;
	}
}

static int32_t dbgStep = 0;
int32_t vsthost::processPlayback(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround) {
	dbgassert(ctrl);
	dbgassert(sampleFormat.blockSize > 0);
	dbgassert(sampleFormat.sampleRate > 0);

	stats.lastInvocationTime_i64 = 0;
	auto timeNow_i64 = getTimeHPint64();
	if (0 != stats.lastInvocationTime_i64) {
		auto timeDelta = timeNow_i64 - stats.lastInvocationTime_i64;
		stats.timings["timeDelta_usec"] = static_cast<int32_t>(timeDelta);
	}
	stats.lastInvocationTime_i64 = timeNow_i64;


	timer2.reset();
	const sampleformat_t sampleFormat = this->sampleFormat;
	const double ticksPerBlock = toTickPrecise(sampleFormat.blockSize/(double)sampleFormat.sampleRate, project.tempo100);
	std::shared_ptr<resampler_t> resamplerOutput = impl->getResampler(sampleFormat, sampleFormatExternal, 0);
	std::shared_ptr<resampler_t> resamplerInput = impl->getResampler(sampleFormatExternal, sampleFormat, 1);

	processMidiRealtimeInput(ctrl, posDouble, state);

	int queueSizeInput = 0;
	int queueSizeOutput = 0;
	auto *stream = audioHost ? audioHost->getStream(0) : nullptr;
	if (stream) {
		queueSizeInput = stream->getInputQueueSize();
		queueSizeOutput = stream->getOutputQueueSize();
	}
	stats.inputQueueLen = queueSizeInput;
	stats.outputQueueLen = queueSizeOutput;
	stats.resamplerInNumBlocks = resamplerInput->numBlocksToPop();
	stats.resamplerInNumSamples = resamplerInput->getNumSamplesOutputBuffer();
	stats.resamplerOutNumBlocks = resamplerOutput->numBlocksToPop();
	stats.resamplerOutNumSamples = resamplerOutput->getNumSamplesOutputBuffer();
	int32_t dbg = dbgStep%333;
	int32_t nBlocksProcessed = 0;
	bool convert = false;
	bool canProcess = audioHost && queueSizeOutput < 8 && queueSizeInput > 2;
	int32_t blockSizeResampled = DAW::NumSamplesResampled(sampleFormat.blockSize, sampleFormat.sampleRate, sampleFormatExternal.sampleRate);
	int32_t numBlocksInternal = math::max(1, sampleFormatExternal.blockSize/blockSizeResampled);
	int32_t numBlocksExternal = (blockSizeResampled + sampleFormatExternal.blockSize - 1)/sampleFormatExternal.blockSize;
	std::vector<AudioBlock> blocksTempInput;
//	std::vector<AudioBuffer> buffersInput;
	blocksTempInput.reserve(numBlocksExternal*numBlocksInternal);
//	buffersInput.resize(numBlocksExternal*numBlocksInternal);
	std::vector<AudioBlock> blocksTempOutput;
//	std::vector<AudioBuffer> buffersOutput;
	blocksTempOutput.reserve(numBlocksExternal*numBlocksInternal);
//	buffersOutput.resize(numBlocksExternal*numBlocksInternal);
	std::vector<AudioBuffer*> actualWrittenBuffersOutput;
	std::vector<AudioBuffer*> actualWrittenBuffersInput;
	timer3.reset();
	while (queueSizeInput) {
		AudioBuffer* ptrExternalInputs = nullptr;
		if (stream->try_dequeueInput(ptrExternalInputs)) {
			resamplerInput->push(*ptrExternalInputs->output);
//			if (queueSizeOutput < 4 && resamplerInput->numBlocksToPop() <= 2) {
////				log_printf("enqueue fake input to get ahead\n", 0);
////				resamplerInput->push(*ptrExternalInputs->output);
//			} else {
//
//				log_printf("enough input for processing: queueSizeOutput %d, blocksToPop %d\n", queueSizeOutput, resamplerInput->numBlocksToPop());
//			}
			ptrExternalInputs->inUse = false;
		}
		queueSizeInput--;
	}
	if (dbg != 0)
		stats.timings["inputs.resample"] = timer3.getTime();
	canProcess = audioHost && queueSizeOutput < RING_BUF_SIZE / 2 && resamplerInput->numBlocksToPop() >= numBlocksExternal * numBlocksInternal;


	if (canProcess) {


		/*
		 * Process audio/midi tracks
		 */
		auto tracksFlatAll = ctrl->trackList.getAllTracksFlatVec(); //TODO: get rid of copy
		/**
		 * process in reverse order: first children, then parents
		 */

		timer3.reset();
		/** turn tree structure into linear pointer array with parents followed by their children **/
		std::shared_ptr<DAW::processing_graph_t> processingGraph;
		if (!DAW::buildProcessingGraph(this, ctrl, tracksFlatAll, processingGraph)) {
			log_printf("Failed building track graph\n", 0);
		}
		if (dbg != 0)
		stats.timings["graph.build"] = timer3.getTime();

		this->lastTrackGraph = processingGraph->trackGraph;
		this->lastProcessingList= processingGraph;
		int64_t timeRouting = 0;
		int64_t timeProcessing = 0;


		for (int i = 0; i < numBlocksInternal; i++) {
			int32_t samplePosProcess = sample + sampleFormat.blockSize*i;
			double tickPosProcess = posDouble + ticksPerBlock*i;
			int32_t pre = resamplerInput->numBlocksToPop();
			AudioBlock block = resamplerInput->pop();
			int32_t post = resamplerInput->numBlocksToPop();
			dbgassert(post == pre-1);
			AudioBlock blockOutput(32, sampleFormat.blockSize);
			dsp_util::fillBlock(blockOutput, 0.0f);
			timer3.reset();
			nBlocksProcessed += processBlock(ctrl, processingGraph.get(), &block, &blockOutput, samplePosProcess, tickPosProcess, state, inLoop, isLoopAround);
			timeProcessing += timer3.getTime();
			timer3.reset();
			for (auto itAudioStage = processingGraph->nodesFlatOrdered.begin(); itAudioStage != processingGraph->nodesFlatOrdered.end(); itAudioStage++) {
				const DAW::processing_track_node_t* ptrProcessingNode = *itAudioStage;
				const DAW::processing_track_node_t& trackNode = *ptrProcessingNode;
				track_t* const track = trackNode.trackOptional;
				track_impl_t* const trackImpl = track->audio;
				if (trackImpl->mixer.isEnabled()) {
					auto outputChannel = trackImpl->outputChannel;
					if (outputChannel.type == DAW::channel_input_type::INPUT_DEFAULT) {
						DAW::channel_ref_t tmp;
						if (DAW::resolveDefaultConnection(this, ctrl, trackImpl, false, tmp)) {
							outputChannel = tmp;
						}
					}
					if (DAW::isChannelConnected(outputChannel) && outputChannel.getType() == DAW::channel_input_type::INPUT_EXTERNAL_AUDIO) {

						/* Calculate master tracks gain level */
						float fGainMaster;
						if (getGainLvl(trackImpl->mixer.getParamValue(PARAM_TRACK_GAIN), fGainMaster)) {
							if (dbg == 0) {
								log_printf("Process External Audio routing from %s to %s\n", StringAsCStr(track->name), StringAsCStr(outputChannel.name));
							}

						}
						blockOutput.addFromOp(&trackImpl->output, AudioBlock::mix_op::ADD, math::clamp(fGainMaster, 0.0f, 1.0f));

					}
				}
			}
			resamplerOutput->push(blockOutput);

			timeRouting += timer3.getTime();
		}
		if (dbg != 0) {
			stats.timings["tracks.process"] = timeProcessing;
			stats.timings["tracks.route"] = timeRouting;
		}
	}


	if (nBlocksProcessed) {
		timer3.reset();
		dbgassert(nBlocksProcessed >= 1);
		int32_t& writePos = ringbuffer.writePos;
		double blockPosSample = sample;
		double blockPosTick = posDouble;
		int32_t nIt = 0;
		int32_t nBlocks = resamplerOutput->numBlocksToPop();
		while (nBlocks > 0 && stream->getOutputQueueSize() < RING_BUF_SIZE/2+4) {
			++nIt;
			AudioBlock block = resamplerOutput->pop();
			AudioBuffer** buffers = ringbuffer.buffers;
			AudioBuffer* const ptrExternalOutputs = buffers[writePos%RING_BUF_SIZE];
			dbgassert(!ptrExternalOutputs->inUse);
			ptrExternalOutputs->submitted = false;
			ptrExternalOutputs->output->realloc(sampleFormatExternal.blockSize);
			ptrExternalOutputs->output->copyFrom(&block);
			ptrExternalOutputs->inUse = true;
			ptrExternalOutputs->submitted = true;
			ptrExternalOutputs->blockPosSample = blockPosSample;
			ptrExternalOutputs->blockPosTick = blockPosTick;
			writePos = (writePos+1) & RING_BUF_MASK;
			stream->enqueue(ptrExternalOutputs);
			nBlocks--;
		}
		if (dbg != 0) {
			stats.timings["output.enqueue"] = timer3.getTime();
		}
	}


	if (nBlocksProcessed) {
		timer3.reset();
		/* Update all track meters */
		for (track_t* track : ctrl->trackList) {
			track_impl_t* trAudio = track->audio;
			if (!trAudio)
				continue;
			float fGainTrack;
			getGainLvl(trAudio->mixer.getParamValue(PARAM_TRACK_GAIN), fGainTrack);
			trAudio->meter.update(&trAudio->output, fGainTrack);
			trAudio->meterInput.update(&trAudio->input, 1.0f);
		}
		if (dbg != 0)
		stats.timings["meters.update"] = timer3.getTime();
		dbgStep++;
#ifndef NDEBUG
		lastTickEndPos = posDouble + ticksPerBlock*nBlocksProcessed;
#endif
		double since = timer.getTimeDoubleReset();
		for (track_t* tr : ctrl->trackList) {
			track_impl_t* trAudio = tr->audio;
			if (trAudio) {
				trAudio->onTick(since);
			}
		}

		if (convert) {
			int32_t bytesCopied = 0;
			hires_timer_t timerConvert;
			for (track_t* tr : ctrl->trackList) {
				track_impl_t* trAudio = tr->audio;
				if (trAudio) {
					bytesCopied += trAudio->audioOutput.convertToSamples(this);
				}
			}
			int64_t timeConvert = timerConvert.getTime();
			stats.timings["convert"] = timeConvert;
			stats.timings["convertBytes"] = bytesCopied;
		}

	}
	if (nBlocksProcessed) {
		bool convert = false;

		stats.blocksProcessed += nBlocksProcessed;
		stats.samplesProcessed += nBlocksProcessed*sampleFormatExternal.blockSize;
		int32_t tickQuarterStart = static_cast<int32_t>(math::floor((posDouble) / (float) TICKS_QUARTER));
		int32_t tickQuarterEnd = static_cast<int32_t>(math::floor((posDouble+ticksPerBlock) / (float) TICKS_QUARTER));
		if (tickQuarterEnd > tickQuarterStart) {
			stats.tickBar += tickQuarterStart - tickQuarterEnd;
		}
		int64_t microSecsPerBlock = (int64_t)sampleFormatExternal.blockSize * 1000000L / (int64_t)sampleFormatExternal.sampleRate;

		int64_t timeTaken = timer2.getTime();
		if (dbg != 0) {
			auto curTimeProcess = stats.timings["blocktime"];
			curTimeProcess -= curTimeProcess/NUM_BINS_STATS;
			curTimeProcess += timeTaken/NUM_BINS_STATS;
			stats.timings["blocktime"] = curTimeProcess;
			stats.timings["blocktimeRaw"] = timeTaken;
		}
		if (convert) {
			stats.timings["convertBlockTime"] = timeTaken;
		}
		stats.timings["microSecsPerBlock"] = microSecsPerBlock;
		stats.usage = stats.timings["blocktime"] / (double) microSecsPerBlock;
		stats.usageRaw = stats.timings["blocktimeRaw"] / (double) microSecsPerBlock;
	}
	return nBlocksProcessed;
}

int32_t vsthost::processBlock(project_controller_t* ctrl, const DAW::processing_graph_t* const processingGraph, AudioBlock* const ptrExternalInputs, AudioBlock* const ptrExternalOutputs, int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround) {

	const sampleformat_t sampleFormat = this->sampleFormat;
	const int32_t lBlockSize = sampleFormat.blockSize;
	const double ticksPerBlock = toTickPrecise(sampleFormat.blockSize/(double)sampleFormat.sampleRate, project.tempo100);



	/*
	 * We try to stay 4 blocks ahead of the audiothread read position
	 * This should be adjusted depending on samplerate and blocksize
	 */
//	int readWriteDist = writePos >= readPos ? writePos-readPos : writePos-(readPos-RING_BUF_SIZE);
	int32_t dbg = dbgStep%333;

#ifndef NDEBUG
	if (!isLoopAround&&state == playback_state::status_play && lastState == playback_state::status_play) {
//			dbgassert(posDouble == lastTickEndPos);
	}
	lastState = state;
#endif

	/*
	 * Clear all channels
	 */
	for (track_t* track : ctrl->trackList) {
		dbgassert(track->audio);
		track_impl_t* audio = track->audio;
		audio->input.realloc(lBlockSize);
		audio->output.realloc(lBlockSize);
		audio->outputPost.realloc(lBlockSize);
		dsp_util::fillBlock(audio->input, 0.0f);
		dsp_util::fillBlock(audio->output, 0.0f);
		dsp_util::fillBlock(audio->outputPost, 0.0f);
		audio->pluginsChanged(); // determine max latency so getLatency() is correct
	}
//
	tick_t pos = floor(posDouble);
//	if (state == playback_state::status_play) {
//		//TODO: latency compensate automation
//		updateTime(sample, posDouble, state);
//		for (track_t* tr : ctrl->trackList) {
//			std::vector<automatable_t*> targets;
//			tr->audio->getAutomatableTrackTargets(targets);
//			for (automatable_t* at : targets) {
//				at->updateAutomatedParameters(pos);
//			}
//		}
//	}


	tick_t loopCutStart = -1;
	tick_t loopCutEnd = -1;
	if (inLoop) {
		loopCutStart = project.loopStart;
		loopCutEnd = project.loopStart+project.loopLen;
	}


	int32_t idxDelayLine = 0;
	AudioBlock tempBlock(8, lBlockSize);
	if (dbg == 0) {
		log_printf("DelayLine.instanceCount %d\n", DelayLine::instanceCount.load());
	}
	for (auto itAudioStage = processingGraph->nodesFlatOrdered.begin(); itAudioStage != processingGraph->nodesFlatOrdered.end(); itAudioStage++) {
		const DAW::processing_track_node_t* ptrProcessingNode = *itAudioStage;
		const DAW::processing_track_node_t& trackNode = *ptrProcessingNode;
		track_t* const track = trackNode.trackOptional;
		track_impl_t* const trackImpl = track->audio;

		const double ticksLatency = toTickPrecise(trackNode.inputLatency/(double)sampleFormat.sampleRate, project.tempo100);
		const double sampleLatencyCompensated = sample - trackNode.inputLatency;
		const double tickLatencyCompensated = posDouble-ticksLatency;
		tick_t processingPos = floor(tickLatencyCompensated);
		timer4.reset();
		if (state == playback_state::status_play) {
			//TODO: latency compensate automation
			updateTime(sampleLatencyCompensated, tickLatencyCompensated, state);
			std::vector<automatable_t*> targets;
			trackImpl->getAutomatableTrackTargets(targets);
			for (automatable_t* at : targets) {
				//at->getLatency();
				at->updateAutomatedParameters(processingPos);
			}
		}
		track->getStage()->procStats.timeUpdateParameters = timer4.getTime();
		int32_t samplePosBlockEnd = sampleLatencyCompensated + lBlockSize;
		int32_t tickBlockEnd = floor(tickLatencyCompensated + ticksPerBlock);
		dbgassert(tickBlockEnd-processingPos < ceil(ticksPerBlock+1));
//			if (dbg == 0) {
//				log_printf("process track %s\n", StringAsCStr(track->name));
//				log_printf("process stage 1 %d\n", static_cast<int32_t>(track->audio->stageId));
//				log_printf("process stage 2 %d\n", static_cast<int32_t>(trackNode.stageId));
//			}

		trackImpl->input.realloc(lBlockSize);
		trackImpl->output.realloc(lBlockSize);

		dsp_util::fillBlock(trackImpl->input, 0.0f);

		int32_t midiProcessFlags = MidiFlags::PROCESS_REALTIME|MidiFlags::PROCESS_ARP;
		if (state == playback_state::status_play) {
			midiProcessFlags = MidiFlags::PROCESS_REALTIME|MidiFlags::PROCESS_CLIPS|MidiFlags::PROCESS_ARP;
		}

		timer4.reset();
		trackImpl->sendNotes(processingPos, tickBlockEnd, loopCutStart, loopCutEnd, project.tempo100, sampleLatencyCompensated, *midiRealtimeInput, midiProcessFlags);
		track->getStage()->procStats.timeSendNotes = timer4.getTime();
		if (state != playback_state::status_play) {
			//
		}
		if (state == playback_state::status_play) {
			trackImpl->fillAudio(processingPos, tickBlockEnd, loopCutStart, loopCutEnd, project.tempo100, sampleLatencyCompensated, trackImpl->input.buf, (int32_t)lBlockSize);
		}

		const uint32_t numChannelsTrack = trackImpl->input.channels;

		std::vector<DAW::track_source_t> allSources = trackNode.pulls; // copy
		allSources.insert(allSources.end(), trackNode.pushs.begin(), trackNode.pushs.end()); // copy

#if 1
		struct Func_CheckHasSolo {
			bool operator()(const DAW::track_source_t& src) const {
				return (src.flags & audiostageflags_t::SOLO) != audiostageflags_t::NONE;
			}
		};
		Func_CheckHasSolo funcCheckSolo;
		bool hasSolo = std::any_of(allSources.cbegin(), allSources.cend(), funcCheckSolo);

		timer4.reset();
		for (const DAW::track_source_t& tracksrc : allSources)
		{
			if (hasSolo && !funcCheckSolo(tracksrc))
				continue;
			if (DAW::isChannelConnected(tracksrc.channel)) {
				track_audio_src src;
//					if (dbg == 0) {
// 						log_printf("track %s has input %s\n", StringAsCStr(track->name), StringAsCStr(tracksrc.channel.name));
//					}

				if (DAW::resolveAudioChannel(this, numChannelsTrack, tracksrc.channel, ptrExternalInputs, src)) {
					/**
					 * Mix routed tracks
					 *
					 * Mix level is fGainInput * src.gain * tracksrc.gain
					 * src.gain:			block-wise automated track gain
					 * tracksrc.gain:		block-wise automated send level, 1.0f for non-sends
					 *
					 * sends are with track gain applied (post-mixer)
					 *
					 */
					/* compensate at input stage */
					/* figure out max latency of all inputs */
					/* delay signal by maxLatency - trackImpl->getLatency() */
					/* Compensate audio midi track to pre-return latency */
					dbgassert(trackNode.inputLatency >= tracksrc.latency);
					samplerate_t delayToMaxInputLatency = trackNode.inputLatency - tracksrc.latency;
					dbgassert(delayToMaxInputLatency >= 0);
					if (delayLines.size() <= idxDelayLine) {
						delayLines.push_back(std::shared_ptr<DelayLine>(new DelayLine((uint32_t)src.channels.size(), 16)));
					}
					dbgassert(delayLines.size() > idxDelayLine);
					AudioBlock srcBlock = src.toAudioBlock();
					dbgassert(srcBlock.samples == tempBlock.samples);
					dbgassert(srcBlock.channels <= tempBlock.channels);
					dbgassert(delayLines[idxDelayLine].get()->block.channels == srcBlock.channels);

					//todo: one of the delay lines will always be 0 samples delay
					delayAudio(delayLines[idxDelayLine].get(), &srcBlock, &tempBlock, delayToMaxInputLatency);
					trackImpl->addAudio(tempBlock, src.gain * tracksrc.gain);
					idxDelayLine++;
				}
			} else {

				if (dbg == 0) {
					log_printf("track %s has no connected input %s\n", StringAsCStr(trackImpl->inputChannel.name));
				}
			}
		}
		track->getStage()->procStats.timeMixInputs = timer4.getTime();
#endif

		dbgassert(
				vsthost::getInstance()->sampleFormat == trackImpl->sampleFormat
				&& trackImpl->input.samples == trackImpl->sampleFormat.blockSize
				&& trackImpl->output.samples == trackImpl->sampleFormat.blockSize
				&& trackImpl->outputPost.samples == trackImpl->sampleFormat.blockSize
				&& trackImpl->sampleFormat.blockSize > 0
				&& trackImpl->sampleFormat.sampleRate > 0);

		/* Processes audio/midi tracks plugin chain */
		processAudio(trackImpl, &trackImpl->input, &trackImpl->output, sampleLatencyCompensated, lBlockSize, state);



		trackImpl->outputPost.copyFrom(&trackImpl->output);

		/* Store block in audioOutput memory */
		if (state == playback_state::status_play) {
			int32_t offset = sample - (int32_t)(trackImpl->getLatency());
			if (offset >= 0) {
#if 0
				trackImpl->audioOutput.store(&trackImpl->outputPost, offset);
#endif
			} else {
				log_printf("cannot write to negative offset %d (samplepos %d - stage.latency %d)\n", offset, sample, trackImpl->getLatency());
			}

		}
	}
	/* Profiling/Timings: Accumulate timings */
	int64_t timeProcessingArr[5] = {0, 0, 0, 0, 0};
	for (track_t* track : ctrl->trackList) {
		timeProcessingArr[0] += track->getStage()->procStats.timeProcessRaw;
		timeProcessingArr[1] += track->getStage()->procStats.timeMixInputs;
		timeProcessingArr[2] += track->getStage()->procStats.timeUpdateParameters;
		timeProcessingArr[3] += track->getStage()->procStats.timeSendNotes;
		timeProcessingArr[4] += track->getStage()->procStats.timeGetNotesInRange;
	}
	int64_t timeTotal = timeProcessingArr[0];
	stats.timings["timeMixInputs"] = timeProcessingArr[1];
	stats.timings["timeUpdateParameters"] = timeProcessingArr[2];
	stats.timings["timeSendNotes"] = timeProcessingArr[3];
	stats.timings["timeGetNotesInRange"] = timeProcessingArr[4];
	stats.timeProcessRaw = timeTotal;
	auto curTimeProcess = stats.timeProcess;
	curTimeProcess -= curTimeProcess/NUM_BINS_STATS;
	curTimeProcess += timeTotal/NUM_BINS_STATS;
	stats.timeProcess = curTimeProcess;
	return 1;
}
void vsthost::onStartPlayback(project_controller_t* ctrl) {
	lastTickEndPos = 0;
	lastState = playback_state::status_stop;
}
void vsthost::onStopPlayback(project_controller_t* ctrl) {
	midiRealtimeInput->m_list.clear();

	for (track_t* track : ctrl->trackList) {
		auto trackImpl = track->audio;
		if (!trackImpl->heldNotes.empty())
		{
			trackImpl->sendNotesOff(project.tempo100);
		}
	}
}
void vsthost::onTrackLayoutChange() {
	delayLines.clear();
}
void vsthost::setOutput(audiohost* audioHost) {
	this->audioHost = audioHost;
	sampleformat_t sampleFormatExternal;
	samplerate_t extSampleRate = audioHost && audioHost->lSampleRate > 0 ? audioHost->lSampleRate : 48000;
	int32_t extBlockSize = audioHost && audioHost->lBlockSize > 0 ? audioHost->lBlockSize : 512;
	sampleFormatExternal = { extSampleRate, extBlockSize, sampleformat_bits_t::FLOAT_32 };
	
//	sampleformat_t sampleFormat = {sampleFormatExternal.sampleRate, sampleFormatExternal.blockSize, sampleformat_bits_t::FLOAT_32};
//	sampleformat_t sampleFormat = {audioHost->lSampleRate*2, sampleFormatExternal.blockSize, sampleformat_bits_t::FLOAT_32};
	//sampleformat_t sampleFormat = { 96000, sampleFormatExternal.blockSize, sampleformat_bits_t::FLOAT_32};
	sampleformat_t sampleFormat = {extSampleRate, sampleFormatExternal.blockSize, sampleformat_bits_t::FLOAT_32};

	oversample_config_t config;
	config.inputSampleRate = sampleFormat.sampleRate;
	config.outputSampleRate = sampleFormatExternal.sampleRate;
	config.numChannels = this->numChannels;
	config.setInputLength(sampleFormat.blockSize);
	this->impl->oversampler = std::make_shared<oversampler_t>(config);
	this->sampleFormatExternal = sampleFormatExternal;
	setSampleFormat(sampleFormat);
	audiocache::getInstance()->setSamplerate(sampleFormat.sampleRate);

}

bool vsthost::isStreaming() {
	return this->audioHost && this->audioHost->isStreaming();
}
//void vsthost::toggleAudioEngineOnOff() {
//	if (isStreaming()) {
//		stopAudio();
//		settings.startEngine = false;
//	} else {
//		if (startAudio()) {
//			settings.startEngine = true;
//		}
//	}
//}

void mulGain(AudioBlock* block, float gain) {

	for (uint32_t i = 0; i < block->channels; i++) {
		float *buf = block->buf[i];
		for (uint32_t j = 0; j < block->samples; j++) {
			*buf *= gain;
			buf++;
		}
	}
}

void vsthost::processAudio(audio_stage_t* stage, AudioBlock* input, AudioBlock* output, int32_t samplePos, int32_t numSamples, playback_state state) {
	int count = 0;
	if (stage->effects.size()) {
		count += stage->effects.size();
	}

	hires_timer_t timer;
	int64_t timeTotal = 0;
	for (int i = 0; i < count; ++i)
	{
		effectbase *current = NULL;
		current = stage->effects[i];
		if (!current->bIsSetup) {
			continue;
		}
		processing.pluginId = current->projectGlobalId;
		dbgassert(current->bIsSetup);
		timer.reset();
		bool isBypass = current->isBypass();
		AudioBlock* blockPostProcess;
		//blockIn/blockOut will always have 2 channels at least
		AudioBlock* blockIn = current->blockInputs;
		AudioBlock* blockOut = current->blockOutputs;
		blockIn->realloc(sampleFormat.blockSize);
		blockOut->realloc(sampleFormat.blockSize);
		if (isBypass || bypassEffectProcessing) {
			samplerate_t delay = current->getDelay();
			if (delay > 0) {
				if (!current->delayLine.get()) {
					current->delayLine.reset(new DelayLine(this->numChannels, sampleFormat.blockSize));
				}
				AudioBlock* blockOut = current->blockOutputs;
				delayAudio(current->delayLine.get(), input, blockOut, delay);
				input = blockOut;
			}
			blockPostProcess = blockZero;
		} else {
			blockIn->copyFrom(input);

			current->process(blockIn, blockOut, samplePos, numSamples, state);
			input = blockOut;
			blockPostProcess = blockOut;
		}
		current->postProcess(blockPostProcess, numSamples, !isBypass);
		const auto timePassed = timer.getTime();

		auto& plugStats = current->procStats;
		if (plugStats.statsProcStep%STATS_PROCESSING_INTERVAL_STEP == 0) {
			plugStats.statsProcSamples[(plugStats.statsWriteOffset+1)%STATS_PROCESSING_MAX_SAMPLES] = timePassed;
			plugStats.statsWriteOffset++;
		}
		auto curTimeProcess = plugStats.timeProcess;
		curTimeProcess -= curTimeProcess/NUM_BINS_STATS;
		curTimeProcess += timePassed/NUM_BINS_STATS;
		plugStats.timeProcess = curTimeProcess;
		plugStats.timeProcessRaw = timePassed;
		timeTotal += timePassed;
		//current->fTimePercentBlockProcess = ((current->fTimePercentBlockProcess*49.0)+(timer.getTime() / (double) microSecsPerBlock))/50.0;^^
		processing.pluginId = 0;
	}
	auto curTotalTimeProc = stage->procStats.timeProcess;
	curTotalTimeProc -= curTotalTimeProc/NUM_BINS_STATS;
	curTotalTimeProc += timeTotal/NUM_BINS_STATS;
	stage->procStats.timeProcessRaw = timeTotal;
	stage->procStats.timeProcess = curTotalTimeProc;


	//   If a plugin runs mono inputs or outputs we need to handle this manually here
	output->copyFrom(input);

}
void vsthost::updatePluginWindows() {
	for (auto* plugin : pluginInstancesVST2) {
//		plugin->dispatch(effEditIdle);
		plugin->updateWindow();
	}
}
bool vsthost::onTick() {
	int iDispatched = 0;
	int64_t now = getTimeMillis();
	for (auto* current : pluginInstancesVST2) {
		if (current->bEditOpen && !current->bInEditIdle) {
			current->bInEditIdle = true;
			current->dispatch(effEditIdle);
			current->bInEditIdle = false;
			if (current->window) {
				if (now - current->window->captureTime > 1000/25) {
					current->window->captureTime = now;
					current->window->captureWindowFrame();
				}
//				current->updateDisplay();
			}
			iDispatched++;
		}
	}
	return false;
}

void vsthost::releaseProjectResources() {
	lastProcessingList = nullptr;
	lastTrackGraph = nullptr;
}
void vsthost::unload() {
	dbgassert(!isStreaming()&&"STOP STREAM BEFORE unload()!");
	unloadAllPlugins();
}
void vsthost::destroy() {
	freeRingBuffer(ringbuffer);
	dbgassert(hostSlot > -1);
	dbgassert(g_hostslots[hostSlot].g_instance);
	g_hostslots[hostSlot].g_instance = nullptr;
}
bool vsthost::assignMasterCallback(vsthost* host)
{
	for (int i = 0; i < NUM_HOST_CB_SLOTS; i++) {
		if (g_hostslots[i].g_instance == nullptr) {
			g_hostslots[i].g_instance = host;
			host->hostSlot = i;
			if (i == 0) {
				host->masterCallBackSlot = audioMaster1;
			}
			if (i == 1) {
				host->masterCallBackSlot = audioMaster2;
			}
			if (i == 2) {
				host->masterCallBackSlot = audioMaster3;
			}
			if (i == 3) {
				host->masterCallBackSlot = audioMaster4;
			}
			return true;
		}
	}
	dbgassert(0&&"Out of host slots");
	return false;
}
vstplugin* vsthost::getPlugin(AEffect* aeffect) {
	for (auto* current : pluginInstancesVST2) {
		if (current->handle->aeffect == aeffect)
			return current;
	}
	return nullptr;
}
effectbase* vsthost::getPluginById(int32_t projectGlobalId) {
	auto it = std::find_if(pluginInstances.begin(), pluginInstances.end(),
		[projectGlobalId] (const effectbase* ptr) {
			return ptr->projectGlobalId == projectGlobalId;
		});
	if (it != pluginInstances.end()) {
		return *it;
	}
	return nullptr;
}
void vsthost::unloadTrack(track_t* track) {
	dbgassert(track->audio);
	auto audio = track->audio;
	std::vector<effectbase*> effects = audio->effects; // make a copy before unloading plugins
	for (effectbase* effect : effects) {
		unloadPlugin(effect);
	}
	dbgassert(audio->deferredEffects.empty());
}
void vsthost::removePlugin(effectbase* plugin) {
	audio_stage_t* audioStage = plugin->getTrackLink();
	audioStage->removePlugin(plugin, true);
	audioStage->pluginsChanged();
	onTrackLayoutChange();
}
void vsthost::unloadPlugin(effectbase* plugin) {

	//TODO: this shouldn't be here!
	if (MainCtrl::get())
		MainCtrl::get()->closeContextMenu();

	plugin->onPreUnload();
	audio_stage_t* audioStage = plugin->getTrackLink();
	if (audioStage) {
		audioStage->removePlugin(plugin, false);
		audioStage->pluginsChanged();
	}

	plugin->close();
	plugin->unload(this);

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

	//	PopupCtrl::get()->close(); // Make sure context controls do not reference vst
	if (plugin->getModuleType() == PLUGIN_TYPE_VST || plugin->getModuleType() == PLUGIN_TYPE_INTERNAL_EFFECT) {
		vstplugin* vst = dynamic_cast<vstplugin*>(plugin);
		if (vst->internalModuleId <= 0) {
			moduleMgr->releaseModule(vst->handle->hmodule);
		}
	}
	delete plugin;
}
bool vsthost::unloadAllPlugins() {
	dbgassert(pluginInstances.empty());
	dbgassert(pluginInstancesVST2.empty());
	dbgassert(pluginInstancesInternal.empty());
	dbgassert(allAudioStages.empty());
	dbgassert(trackAudioStages.empty());
//	int count = list.size();
//	for (int i = 0; i < count; ++i)
//	{
//		vstplugin *current = list[i];
//		if (current->trackImpl) {
//			current->trackImpl->removePlugin(current, false);
//		}
//	}
//	for (int i = 0; i < count; ++i)
//	{
//		vstplugin *current = list[i];
//		current->close();
//		list[i] = NULL;
//		current->unload(this);
//		moduleMgr->releaseModule(current->handle->hmodule);
//		delete current;
//	}
//	list.clear();
	return true;
}

vstplugin::~vstplugin() {
	if (blockInputs)
		delete blockInputs;
	if (blockOutputs)
		delete blockOutputs;
	delete handle;
}

void vsthost::getAllInstances(std::vector<effectbase*>& effects) {
//	for (auto* as : allAudioStages) {
//		effects.insert( effects.end(), as->effects.begin(), as->effects.end() );
//	}
	effects = pluginInstances;
}
void vsthost::createAudio(track_t* track) {
	auto audio = new track_impl_t(getNextGlobalAudioStageId(0), track, sampleFormat.sampleRate, sampleFormat.blockSize, OUTPUT_CHANNELS);
	allAudioStages.push_back(audio);
	trackAudioStages.push_back(audio);
	track->audio = audio;
}
void vsthost::releaseAudio(track_t* track) {
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
audio_stage_t* vsthost::createAudioStage() {
	auto audio = new audio_stage_t(getNextGlobalAudioStageId(0), sampleFormat.sampleRate, sampleFormat.blockSize, OUTPUT_CHANNELS);
	allAudioStages.push_back(audio);
	return audio;
}
void vsthost::releaseAudioStage(audio_stage_t* audioStage) {
	auto it = std::find(allAudioStages.begin(), allAudioStages.end(), audioStage);
	dbgassert(it != allAudioStages.end());
	allAudioStages.erase(it);
}
audio_stage_t* vsthost::getAudioStage(const audio_stage_ref_t& ref) const {
	if (ref.stageId == TRACKID_INVALID_I32)
		return nullptr;
	dbgassert((int32_t)ref.stageId > -1);
	auto it = std::find_if(allAudioStages.begin(), allAudioStages.end(), [ref] (const audio_stage_t* ptr) {
		return ptr->stageId == ref.stageId;
	});
//	dbgassert(it != allAudioStages.end());
	if (it != allAudioStages.end()) {
		return *it;
	}
	log_printf("null audio stage for %d\n", static_cast<int32_t>(ref.stageId));
	return nullptr;
}
bool vsthost::movePlugins(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
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
	return true;
}
bool vsthost::moveEffects(audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	dbgassert(src >= 0 && dst >= 0);
	dbgassert(src != dst);
//	src--;
//	dst--;
	dbgassert((int32_t)trp->effects.size() > src);
	dbgassert((int32_t)trp->effects.size() > dst);
	for (effectbase* effect : trp->effects) {
		dbgassert(effect->getSlot()>=0);
	}


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
			my_printf("jump input iterator from %d to %d\n", itIn-curEffects.cbegin(), itIn-curEffects.cbegin()+len);
			itIn+=len;
		}
		int srcPos;
		int outPos = itOut-tmpEffects.begin();
		if (dst2 < end && tmpEffects.cbegin()+dst2 == itOut) {
			my_printf("dst2 %d\n", dst2);
			srcPos = src2;
			*itOut++ = curEffects[src2++];
			dst2++;
			my_printf("b writing %d to %d\n", srcPos, outPos);
		} else {
			srcPos = itIn-curEffects.cbegin();
			*itOut++ = *itIn++;
			my_printf("a writing %d to %d\n", srcPos, outPos);
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

bool vsthost::replacePlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst, effectbase** prevPlugin) {
	//TODO: call pluginsChanged, update latency
	bool retVal = trp->replaceEffect(dst, plugin, prevPlugin);
	onTrackLayoutChange();
	return retVal;
}
bool vsthost::insertNewPlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst) {
//	if (plugin->isSynth) {
//		vstplugin* old = trp->setInstrument(plugin);
//		if (old) {
//			unloadPlugin(old);
//		}
//	} else {
		trp->insertEffect(dst, plugin);
//	}
	trp->pluginsChanged();
	onTrackLayoutChange();
	return true;
}
#ifdef _WIN32

int32_t loadLib(String filepath, VSTPluginMain_t** out_fn, HMODULE* out_hmodule) {
	if (!FileExists(filepath)) {
		return -2;
	}
	HMODULE hmodule = LoadLibrary(StringAsCStr(filepath));
	if (!hmodule) {
		return -3;
	}

	VSTPluginMain_t *fn = (VSTPluginMain_t*) GetProcAddress(hmodule, "VSTPluginMain");
	if (fn == NULL)
	{
		fn = (VSTPluginMain_t*) GetProcAddress(hmodule, "main");
	}
	if (fn == NULL)
	{
		FreeLibrary(hmodule);
		return -4;
	}
	*out_hmodule = hmodule;
	*out_fn = fn;

	return 0;
}
#endif
#if defined(__APPLE__)

int32_t loadLib(String filepath, VSTPluginMain_t** out_fn, void** out_hmodule);

#endif
#if defined(__linux__)
int32_t loadLib(String filepath, VSTPluginMain_t** out_fn, void** out_hmodule) {
	if (!FileExists(filepath)) {
		return -2;
	}
	void* module = dlopen(StringAsCStr(filepath), RTLD_NOW);
	if (!module) {
		return -3;
	}

	VSTPluginMain_t *fn = (VSTPluginMain_t*) dlsym(module, "VSTPluginMain");
	if (fn == NULL)
	{
		fn = (VSTPluginMain_t*) dlsym(module, "main");
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
#endif
int32_t vsthost::getNextGlobalModuleId(int32_t globalId) {
	if (globalId <= 0) {
		return ++pluginId;
	} else {
		update_maximum(pluginId, globalId);
	}
	return globalId;
}

audiostageid_i32 vsthost::getNextGlobalAudioStageId(int32_t globalId) {
	if (globalId <= 0) {
		return (audiostageid_i32)++audioStageId;
	} else {
		update_maximum(audioStageId, globalId);
	}
	return (audiostageid_i32)globalId;
}
void vsthost::updateMaximumStageId() {
	int32_t maximumStageId = 0;
	for (auto* stage : allAudioStages) {
		maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId));
	}
	this->audioStageId = maximumStageId;
}

bool vsthost::writeRecordedData() {
	dbgassert(MainCtrl::get());
	if (this->hasNewRecordedData) {
		this->hasNewRecordedData = false;
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		clip_t* pClip = nullptr;
		std::swap(recordDataProcessed, pClip);
		if (pClip) {
			track_t* tr = MainCtrl::get()->trackMidiAudioCtr.front();
			if (tr) {
//				String s = "Recorded notes: ";
//				for (note_t& note : pClip->notes.m_list) {
//					s += String(noteName(note.pitch))+",";
//				}
//				log_printf("%s\n", StringAsCStr(s));
				log_printf("Processing recorded clip with %d notes\n", pClip->notes.m_list.size());
				log_printf("Processing recorded clip. Last note time %d\n", pClip->notes.lastNote.time);
				tick_t tickBegin = pClip->time;
				tick_t tickEnd = pClip->end();
				MainCtrl::get()->cutIntersecting(tr, tickBegin, tickEnd);
				pClip->setDirty();
				pClip->notes.updateBounds();
				tr->getMidi().addClip(pClip);
				tr->getMidi().sortClips();
				return true;
			}
		}
	}
	return false;
}
int32_t vsthost::getNextSampleId(int32_t id) {
	if (id <= 0) {
		return ++sampleId;
	} else {
		update_maximum(sampleId, id);
	}
	return id;
}

vstpluginloadres vsthost::loadPlugin(String filepath, int32_t globalId) {
	dbgassert(masterCallBackSlot);
	String path, name, nameWithoutExt;
	SplitPath(filepath, &path, &nameWithoutExt, NULL, &name);
	VSTPluginMain_t* fn = NULL;
	void* moduleHandle = NULL;
	AEffect* aeffect = NULL;
#ifdef _WIN32
	HMODULE hmodule = NULL;
	int32_t ret = loadLib(filepath, &fn, &hmodule);
	if (ret != 0) {
		return vstpluginloadres(ret, NULL);
	}
	aeffect = fn(masterCallBackSlot);
	if (!aeffect) {
		FreeLibrary(hmodule);
		return vstpluginloadres(-5, NULL);
	}
	if (aeffect->magic != kEffectMagic) {
		FreeLibrary(hmodule);
		return vstpluginloadres(-6, NULL);
	}
	moduleHandle = hmodule;
#endif

#if defined(__linux__) || defined(__APPLE__)
	void* hmodule = NULL;
	int32_t ret = loadLib(filepath, &fn, &hmodule);
	if (ret != 0) {
		return vstpluginloadres(ret, NULL);
	}

	aeffect = fn(masterCallBackSlot);
	if (!aeffect) {
		dlclose(hmodule);
		return vstpluginloadres(-5, NULL);
	}
	if (aeffect->magic != kEffectMagic) {
		dlclose(hmodule);
		return vstpluginloadres(-6, NULL);
	}
	moduleHandle = hmodule;
#endif

	globalId = getNextGlobalModuleId(globalId);
	vstplugin* plugin = new vstplugin(new handles_t(nullptr, aeffect, moduleHandle), globalId, path, nameWithoutExt, -1);
	pluginInstancesVST2.push_back(plugin);
	pluginInstances.push_back(plugin);
	plugin->load(this);
	dbgassert(plugin->handle&&plugin->handle->aeffect);
	return vstpluginloadres(0, plugin);
};
