#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "seq_math.h"
#include "dsp_util.h"
#include "vst_host.h"
#include "vst_plugin.h"
#include "fileio.h"
#include "track.h"
#include "track_audiodata.h"
#include "mainctrl.h"

#include "../vst_sdk_2.4/aeffectx.h"
#include "portaudio.h"
#include "settings.h"

#include "logging.h"
#include "audioblock.h"
#include "vst_plugin_handles.h"
#include "platform.h"

#include <stdlib.h>
#include <algorithm>
#include <stdlib.h>
#include <windows.h>
#include <memory.h>
#include <mutex>
#ifdef __MINGW32__
#include "../threads/mingw.mutex.h"
#endif
#include "leak_detect.h"

//#define DBG_PRINT_CALLBACKS
#ifdef DBG_PRINT_CALLBACKS
#define MAX_LEN_MY_DBF 512
void cbPrintf(vstplugin* plugin, const char *fmt, ...);
void cbPrintf(vstplugin* plugin, const char *fmt, ...) {
	char buf[MAX_LEN_MY_DBF];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, MAX_LEN_MY_DBF - 1, fmt, args);
	va_end(args);

	my_printf("%s %s", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), buf);
}
#else
void emptyPrinft(vstplugin* plugin, const char *fmt, ...) {
}
#define cbPrintf emptyPrinft
#endif

#define ERROR_LOG(x) (my_printf("ERROR: %s\n", x))
//-------------------------------------------------------------------------------------------------------
/*! hostCanDos strings Plug-in -> Host */
namespace HostCanDos
{
	const char* canDoSendVstEvents = "sendVstEvents"; ///< Host supports send of Vst events to plug-in
	const char* canDoSendVstMidiEvent = "sendVstMidiEvent"; ///< Host supports send of MIDI events to plug-in
	const char* canDoSendVstTimeInfo = "sendVstTimeInfo"; ///< Host supports send of VstTimeInfo to plug-in
	const char* canDoReceiveVstEvents = "receiveVstEvents"; ///< Host can receive Vst events from plug-in
	const char* canDoReceiveVstMidiEvent = "receiveVstMidiEvent"; ///< Host can receive MIDI events from plug-in 
	const char* canDoReportConnectionChanges = "reportConnectionChanges"; ///< Host will indicates the plug-in when something change in plug-in´s routing/connections with #suspend/#resume/#setSpeakerArrangement 
	const char* canDoAcceptIOChanges = "acceptIOChanges"; ///< Host supports #ioChanged ()
	const char* canDoSizeWindow = "sizeWindow"; ///< used by VSTGUI
	const char* canDoOffline = "offline"; ///< Host supports offline feature
	const char* canDoOpenFileSelector = "openFileSelector"; ///< Host supports function #openFileSelector ()
	const char* canDoCloseFileSelector = "closeFileSelector"; ///< Host supports function #closeFileSelector ()
	const char* canDoStartStopProcess = "startStopProcess"; ///< Host supports functions #startProcess () and #stopProcess ()
	const char* canDoShellCategory = "shellCategory"; ///< 'shell' handling via uniqueID. If supported by the Host and the Plug-in has the category #kPlugCategShell
	const char* canDoSendVstMidiEventFlagIsRealtime = "sendVstMidiEventFlagIsRealtime"; ///< Host supports flags for #VstMidiEvent
}

//-------------------------------------------------------------------------------------------------------
/*! plugCanDos strings Host -> Plug-in */
namespace PlugCanDos
{
	const char* canDoSendVstEvents = "sendVstEvents"; ///< plug-in will send Vst events to Host
	const char* canDoSendVstMidiEvent = "sendVstMidiEvent"; ///< plug-in will send MIDI events to Host
	const char* canDoReceiveVstEvents = "receiveVstEvents"; ///< plug-in can receive MIDI events from Host
	const char* canDoReceiveVstMidiEvent = "receiveVstMidiEvent"; ///< plug-in can receive MIDI events from Host 
	const char* canDoReceiveVstTimeInfo = "receiveVstTimeInfo"; ///< plug-in can receive Time info from Host 
	const char* canDoOffline = "offline"; ///< plug-in supports offline functions (#offlineNotify, #offlinePrepare, #offlineRun)
	const char* canDoMidiProgramNames = "midiProgramNames"; ///< plug-in supports function #getMidiProgramName ()
	const char* canDoBypass = "bypass"; ///< plug-in supports function #setBypass ()
}

VstIntPtr VSTCALLBACK audioMaster(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	vstplugin* plugin = vsthost::getInstance()->getPlugin(effect);

	switch (opcode)
	{
	case audioMasterAutomate:
		cbPrintf(plugin, "audioMasterAutomate %d %d %d\n", index, opcode, value);
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
		return (VstIntPtr)vsthost::getInstance()->getTimeInfo();
	case audioMasterProcessEvents:
		cbPrintf(plugin, "audioMasterProcessEvents %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterIOChanged:
		cbPrintf(plugin, "audioMasterIOChanged %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterSizeWindow:
		cbPrintf(plugin, "audioMasterSizeWindow %d %d %d\n", index, opcode, value);
		return 0;
	case audioMasterGetSampleRate:
		cbPrintf(plugin, "audioMasterGetSampleRate %d %d %d\n", index, opcode, value);
		return (long)vsthost::getInstance()->lSampleRate;
	case audioMasterGetBlockSize:
		cbPrintf(plugin, "audioMasterGetBlockSize %d %d %d\n", index, opcode, value);
		return vsthost::getInstance()->lBlockSize;
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
		return kVstAutomationOff;
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
		return vsthost::getInstance()->canDo((const char*)ptr);
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

		return (VstIntPtr)plugin->updateDisplay();
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

bool error(const char* msg, PaError err) {
	my_printf("Error in %s\n", msg);
	my_printf("Error number: %d\n", err);
	my_printf("Error message: %s\n", Pa_GetErrorText(err));
	return false;
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/



static int audioCallback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	vsthost* host = vsthost::getInstance();
	float **inputs = (float**)inputBuffer;
	UNUSED(inputs);
	float **outputs = (float**)outputBuffer;
//	if (!inputs) {
//		host->blockTemp->clear();
//		inputs = host->blockTemp->f;
//	}
	dsp_util::fillSilence(outputs, framesPerBuffer);
	AudioBuffer* block;
	if (host->audioQueue.try_dequeue(block)) {
		if (framesPerBuffer == block->output->samples) {
			block->output->copyTo(outputs);
//			dsp_util::copyBuffer(outputs, block->output->f, block->output->samples);
		}
		block->inUse = false;
	} else {
		host->bufferUnderuns++;
//		dsp_util::fillSilence(inputs, framesPerBuffer);
	}
//	dsp_util::fillSqare(host->fSampleRate, 440, inputs, framesPerBuffer);

	if (framesPerBuffer == host->lBlockSize) {
		//host->processAudio(inputs, outputs, framesPerBuffer);
	}
	else {
	}

	dsp_util::fillSaturate(outputs, framesPerBuffer);
	host->blockReads++;
	return paContinue;
}
/*
* This routine is called by portaudio when playback is done.
*/
static void StreamFinished(void* userData)
{
	vsthost* host = vsthost::getInstance();
	host->onStreamEnd();
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
struct UnloadModule {
	uint64_t time;
	HMODULE handle;
};
String getModuleName(HMODULE module);
class vsthost::ModuleManager {
    std::mutex m_mtx;
	std::vector<UnloadModule> modules;
	int32_t cnt = 0;
	const uint64_t timeout = 1500;
public:
	ModuleManager() {

	}
	void queueRelease(HMODULE module) {
	    std::unique_lock<std::mutex> lock(m_mtx);
		UnloadModule ulModule{getTimeMillis(), module};
		modules.push_back(ulModule);
		cnt++;
	}
	void onTick() {
		if (!cnt)
			return;
	    std::unique_lock<std::mutex> lock(m_mtx);
		auto it = modules.begin();
		while (it != modules.end()) {
			UnloadModule& ulModule = *it;
			if (getTimeMillis() - ulModule.time > timeout) {
				String moduleName = getModuleName(ulModule.handle);
				my_printf("Unloading module %s\n", StringAsCStr(moduleName));
				FreeLibrary(ulModule.handle);
				it = modules.erase(it);
			} else {
				it++;
			}
		}
	}
};


AudioBuffer* allocateBuffer() {
	AudioBuffer* buffer = (AudioBuffer*) aligned_malloc(sizeof(AudioBuffer), 128);
	buffer->output = new AudioBlock(OUTPUT_CHANNELS, 1);
	buffer->input = new AudioBlock(OUTPUT_CHANNELS, 1);
	buffer->submitted = false;
	std::atomic_init(&buffer->inUse, false);
	return buffer;
}
vsthost::~vsthost() {
	delete moduleMgr;
}
vsthost::vsthost(project_globals_t& _project, uint32_t _sampleRate, uint16_t _blockSize)
	: moduleMgr{new vsthost::ModuleManager{}},
	  project(_project),
	  lSampleRate(_sampleRate),
	  lBlockSize(_blockSize),
	  numChannels(OUTPUT_CHANNELS)
{
	memset(&timeinfo, 0, sizeof(timeinfo));
	for (int i = 0; i < RING_BUF_SIZE; i++) {
		ringbuffer.buffers[i] = allocateBuffer();
	}
	updateTime(0, 0, playback_state::status_stop);
}
//\note VstTimeInfo::samplesToNextClock :
//MIDI Clock Resolution (24 per Quarter Note), can be negative the distance to the next midi clock
//		(24 ppq, pulses per quarter) in samples. unless samplePos falls precicely on a midi clock,
//		this will either be negative such that the previous MIDI clock is addressed,
//		or positive when referencing the following (future) MIDI clock.
void vsthost::updateTime(int32_t samplePos, tick_t pos, playback_state state) {
	timeinfo.samplePos = samplePos;
	timeinfo.sampleRate = (double)lSampleRate;
	timeinfo.nanoSeconds = (double)timeGetTime() * 1000000.0L;
	timeinfo.ppqPos = (pos/(double)TICKS_QUARTER);
	timeinfo.tempo = project.tempo100/100.0;
	timeinfo.barStartPos = floor(pos / (double) TICKS_BAR) * 4;
	timeinfo.cycleStartPos = (project.loopStart/(double)TICKS_QUARTER);
	timeinfo.cycleEndPos = ((project.loopStart+project.loopLen)/(double)TICKS_QUARTER);
	timeinfo.timeSigNumerator = project.signatureNum;
	timeinfo.timeSigDenominator = 1 << project.signatureDenom;
	if (project.loopEnabled) {
	} else {
		timeinfo.cycleStartPos = 0;
		timeinfo.cycleEndPos = 0;
	}
	double dPos = timeinfo.samplePos / timeinfo.sampleRate;
	/* offset in fractions of a second   */
	double dOffsetInSecond = dPos - floor(dPos);
	timeinfo.smpteFrameRate = VstSmpteFrameRate::kVstSmpte24fps;
	timeinfo.smpteOffset = (long)(dOffsetInSecond * fSmpteDiv[timeinfo.smpteFrameRate] * 80.L);
	tick_t midiTick = tick4096ToPPQ24TickRounded(samplePos, project.tempo100, lSampleRate, lBlockSize);
	int32_t samplePosTick = PPQ24TickSample(midiTick, project.tempo100, lSampleRate, lBlockSize);
	timeinfo.samplesToNextClock = samplePosTick - timeinfo.samplePos;
//	my_printf("midi tick %d (%d offset %d)\n", midiTick, samplePosTick, timeinfo.samplesToNextClock);
	bool changed;
	changed = setFlag(timeinfo.flags, kVstTransportPlaying, state != playback_state::status_stop);
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

void vsthost::sendNotesOff(vstplugin* plugin) {
	handles_t* handles = plugin->handle;
	if (handles && handles->tr_plugins) {
		track_plugins_t* audio = handles->tr_plugins;
		if (audio) {
			audio->sendNotesOff(project.tempo100, 0);
		}
	}
}
int32_t vsthost::processPlayback(int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround) {
	double since = timer.getTimeDoubleReset();
	MainCtrl* ctrl = MainCtrl::get(); //TODO: still not synchronized.
//	static AudioBuffer* master = allocateBuffer();
//	master->input->realloc(lBlockSize);
//	master->output->realloc(lBlockSize);
	int32_t& readPos = ringbuffer.readPos;
	int32_t& writePos = ringbuffer.writePos;
	AudioBuffer** buffers = ringbuffer.buffers;
	while (stream != NULL) {
		AudioBuffer* buffer = buffers[readPos];
		if (!buffer->submitted || buffer->inUse) {
			break;
		}
		buffer->submitted = false;
		readPos++;
		readPos &= RING_BUF_MASK;
	}
	int nBlocksProcessed = 0;
	const double ticksPerBlock = toTickPrecise(lBlockSize/(double)lSampleRate, project.tempo100);

#ifndef NDEBUG

	static double lastTickEndPos = 0;
	static playback_state lastState = playback_state::status_stop;
#endif
	if (stream != NULL) {
		/*
		 * We try to stay 4 blocks ahead of the audiothread read position
		 * This should be adjusted depending on samplerate and blocksize
		 */
		int readWriteDist = writePos >= readPos ? writePos-readPos : writePos-(readPos-RING_BUF_SIZE);
		if (readWriteDist < 8) {
#ifndef NDEBUG
		if (!isLoopAround&&state == playback_state::status_play && lastState == playback_state::status_play) {
			assert(posDouble == lastTickEndPos);
		}
		lastState = state;
#endif
		tick_t pos = floor(posDouble);
		updateTime(sample, pos, state);
		for (vstplugin* plugin : this->list) {
			plugin->updateAutomatedParameters(pos);
		}
		int32_t samplePosBlockEnd = sample + lBlockSize;
		int32_t tickBlockEnd = floor(posDouble + ticksPerBlock);
		assert(tickBlockEnd-pos < ceil(ticksPerBlock+1));
		/*
		 * Clear all master channels first
		 */
		for (track_t* trackMaster : ctrl->trackMasterCtr) {
			assert(trackMaster->audio);
			track_plugins_t* audioMaster = trackMaster->audio;
//			if (!audioMaster) {
//				trackMaster->audio = audioMaster = vsthost::getInstance()->createAudio(trackMaster);
//			}
			audioMaster->input.realloc(lBlockSize);
			audioMaster->output.realloc(lBlockSize);
			dsp_util::fillSilence(audioMaster->input.buf, lBlockSize);
			dsp_util::fillSilence(audioMaster->output.buf, lBlockSize);
		}

		/*
		 * Process all normal channels
		 */
		for (track_t* track : ctrl->trackCtr) {
			track_plugins_t* audioTrack = track->audio;
			if (!audioTrack) {
				track->audio = audioTrack = vsthost::getInstance()->createAudio(track);
			}
			if (state == playback_state::status_play && audioTrack->instrument && audioTrack->instrument->bIsEnabled) {
				tick_t loopCutStart = -1;
				tick_t loopCutEnd = -1;
				if (inLoop) {
					loopCutStart = project.loopStart;
					loopCutEnd = project.loopStart+project.loopLen;
				}
				audioTrack->sendNotes(pos, tickBlockEnd, loopCutStart, loopCutEnd, project.tempo100, sample);
			} else if (!audioTrack->heldNotes.empty()) {
				audioTrack->sendNotesOff(project.tempo100, sample);
			}
			audioTrack->input.realloc(lBlockSize);
			audioTrack->output.realloc(lBlockSize);
			dsp_util::fillSilence(audioTrack->input.buf, lBlockSize);
			/* Processes a whole plugin chain */
			processAudio(audioTrack, &audioTrack->input, &audioTrack->output, lBlockSize);
			for (track_t* trackMaster : ctrl->trackMasterCtr) {
				track_plugins_t* audioMaster = trackMaster->audio;
				audioMaster->input.addFrom(&audioTrack->output);
			}
		}

		for (track_t* trackMaster : ctrl->trackMasterCtr) {
			track_plugins_t* audioMaster = trackMaster->audio;
			processAudio(audioMaster, &audioMaster->input, &audioMaster->output, lBlockSize);
		}

		/*
		 * Output all masters
		 * Right now only first, until I figured out configuring and streaming multiple audiostreams
		 */
		AudioBuffer* bufferWrite = buffers[writePos];
		assert(!bufferWrite->inUse);
		bufferWrite->input->realloc(lBlockSize);
		bufferWrite->output->realloc(lBlockSize);
		dsp_util::fillSilence(bufferWrite->input->buf, lBlockSize);
		dsp_util::fillSilence(bufferWrite->output->buf, lBlockSize);
		AudioBlock* bufOut = bufferWrite->output;
		for (track_t* trackMaster : ctrl->trackMasterCtr) {
			track_plugins_t* audioMaster = trackMaster->audio;
			AudioBlock* bufMaster = &audioMaster->output;
			for (int n = 0; n < OUTPUT_CHANNELS; n++) {
				float* channelWriteBuffer = bufOut->buf[n];
				float* channelMaster = bufMaster->buf[n];
				for (int j = 0; j < lBlockSize; j++) {
					channelWriteBuffer[j] += channelMaster[j];
				}
			}
			break;
		}
		/* Update all track meters */
		for (track_t* track : ctrl->trackList) {
			track_plugins_t* trAudio = track->audio;
			if (!trAudio)
				continue;
			trAudio->meter.update(&trAudio->output);
		}
		sample = samplePosBlockEnd;
		posDouble += ticksPerBlock;
#ifndef NDEBUG
		lastTickEndPos = posDouble;
#endif
//		dsp_util::fillSqare(fSampleRate, 440, bufferWrite->master->f, bufferWrite->master->samples);
		bufferWrite->submitted = true;
		bufferWrite->inUse = true;
		writePos++;
		writePos &= RING_BUF_MASK;
		audioQueue.enqueue(bufferWrite);
		nBlocksProcessed++;
		}
	}
	for (track_t* tr : ctrl->trackList) {
		track_plugins_t* trAudio = tr->audio;
		if (trAudio) {
			tr->audio->onTick(since);
		}
	}
	return nBlocksProcessed;
}
void vsthost::onStreamEnd() {
	stream = NULL;
	AudioBuffer* block;
	while (audioQueue.try_dequeue(block)) {
		block->inUse = false;
	}
}
void vsthost::onStartPlayback(int32_t block) {
	blockReads = block;
	bufferUnderuns = 0;
}
void vsthost::onStopPlayback() {
}

void mulGain(AudioBlock* block, float gain) {

	for (uint32_t i = 0; i < block->channels; i++) {
		float *buf = block->buf[i];
		for (uint32_t j = 0; j < block->samples; j++) {
			*buf *= gain;
			buf++;
		}
	}
}
void vsthost::processAudio(track_plugins_t* channel, AudioBlock* input, AudioBlock* output, unsigned long samples) {
//	float** bufOut = outputs;
//	float** bufIn = inputs;
	int count = 0;
	if (channel->instrument) {
		count++;
	}
	if (channel->effects.size()) {
		count+=channel->effects.size();
	}


//	AudioBlock* input = block->input;
	for (int i = 0; i < count; ++i)
	{
		vstplugin *current = NULL;
		if (channel->instrument) {
			current = i == 0 ? channel->instrument : channel->effects[i-1];
		} else {
			current = channel->effects[i];
		}
		if (!current->bIsSetup) {
			continue;
		}
		if (!current->bIsEnabled) {
			continue;
		}
		AudioBlock* blockIn = current->blockInputs;
		AudioBlock* blockOut = current->blockOutputs;
		blockIn->realloc(lBlockSize);
		blockOut->realloc(lBlockSize);

		//TODO: maybe fill silence here, we never know how plugins can screw up
		//TODO: blockIn/blockOut will always have 2 channels at least
		//   If a plugin runs mono inputs or outputs we need to handle this manually here
		blockIn->copyFrom(input);

		handles_t* handle = current->handle;
		if (handle->aeffect != NULL) {
			if (handle->aeffect->flags & effFlagsCanReplacing) {
 				handle->aeffect->processReplacing(handle->aeffect, blockIn->buf, blockOut->buf, samples);
			} else {
				handle->aeffect->process(handle->aeffect, blockIn->buf, blockOut->buf, samples);
			}
			//TODO: maybe sanitize plugins output floats here (NaN/Inf/ >50 dBFS)
			input = blockOut;
		}
	}
	//   If a plugin runs mono inputs or outputs we need to handle this manually here
	output->copyFrom(input);

	float gain = dsp_util::clampReadGain(channel->mixer.gain);
	mulGain(output, gain);

}
bool vsthost::onTick() {
	int count = list.size();
	int iDispatched = 0;
	for (int i = 0; i < count; ++i)
	{
		vstplugin *current = list[i];
		if (current && current->bEditOpen && !current->bInEditIdle) {
			current->bInEditIdle = true;
			current->dispatch(effEditIdle);
			current->bInEditIdle = false;
			iDispatched++;
		}
	}
	moduleMgr->onTick();
	return false;
}

bool vsthost::postInit() {
	if (settings.startEngine)
		startAudio();
	return true;
}
bool vsthost::stopAudio() {
	PaStream* stream = this->stream;
	if (stream) {
		PaError err;
		err = Pa_StopStream(stream);
		if (err != paNoError) {
			return error("Pa_StopStream", err);
		}
		err = Pa_CloseStream(stream);
		if (err != paNoError) {
			return error("Pa_CloseStream", err);
		}
		while (this->stream) {
			Sleep(100);
		}
		return true;
	}
	return false;
}
void vsthost::unload() {
	assert(this->stream==NULL&&"STOP STREAM BEFORE unload()!");
	Pa_Terminate();
	unloadAllPlugins();
}
void vsthost::destroy() {
	for (int i = 0; i < RING_BUF_SIZE; i++) {
		if (ringbuffer.buffers[i]) {
			delete ringbuffer.buffers[i]->input;
			delete ringbuffer.buffers[i]->output;
			aligned_free(ringbuffer.buffers[i]);
			ringbuffer.buffers[i] = nullptr;
		}
	}
}
bool vsthost::startAudio() {
	my_printf("startAudio\n", 0);
	PaError err;
	err = Pa_Initialize();
	if (err != paNoError) {
		Pa_Terminate();
		return error("Pa_Initialize", err);
	}
	int apiCount = Pa_GetHostApiCount();
	const char* selApiNameCStr = StringAsCStr(settings.device_api);
	const char* selDevNameCStr = StringAsCStr(settings.device_selected);
	int32_t deviceApiIdxSelected = paNoDevice;
	int32_t deviceIdxSelected = paNoDevice;
	for (int i = 0; i < apiCount; i++) {
		const PaHostApiInfo *info = Pa_GetHostApiInfo(i);
		if (info) {
			if (!strcmp(selApiNameCStr, info->name)) {
				deviceApiIdxSelected = i;
			}
		}
	}
	if (deviceApiIdxSelected >= 0) {
		int deviceCount = Pa_GetDeviceCount();
		for (int i = 0; i < deviceCount; i++) {
			const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
			if (info && info->hostApi == deviceApiIdxSelected && info->maxOutputChannels > 0 && !strcmp(selDevNameCStr, info->name)) {
				deviceIdxSelected = i;
			}
		}
	}

	if (deviceIdxSelected < 0) {
		my_printf("Error: No output device.\n", 0);
		return false;
	}



	const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(deviceIdxSelected);
	const PaHostApiInfo* apiInfo = Pa_GetHostApiInfo(deviceApiIdxSelected);
	PaStreamParameters outputParameters;
	outputParameters.device = deviceIdxSelected;
	if (outputParameters.device == paNoDevice) {
		return error("outputParameters.device == paNoDevice", err);
	}
	outputParameters.channelCount = OUTPUT_CHANNELS;       /* stereo output */
	outputParameters.sampleFormat = paFloat32 | paNonInterleaved; /* 32 bit floating point output */
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;

	my_printf("Open stream on device %s | %s\n", apiInfo->name, devInfo->name);
	my_printf("samplerate %u\n", lSampleRate);
	my_printf("channelCount %f\n", outputParameters.channelCount);
	my_printf("lBlockSize %d\n", this->lBlockSize);
	PaStream* paStream = NULL;
	err = Pa_OpenStream(
		&paStream,
		NULL, /* no input */
		&outputParameters,
		(double)this->lSampleRate,
		this->lBlockSize,
		paClipOff,      /* we won't output out of range samples so don't bother clipping them */
		audioCallback,
		NULL);

	if (err != paNoError) {
		return error("Pa_OpenStream", err);
	}

	err = Pa_SetStreamFinishedCallback(paStream, &StreamFinished);
	if (err != paNoError)
		return error("Pa_SetStreamFinishedCallback", err);

	err = Pa_StartStream(paStream);
	if (err != paNoError)
		return error("Pa_StartStream", err);
	this->stream = paStream;
	return true;
}
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
		FreeLibrary(hmodule);
		return -4;
	}
	*out_hmodule = hmodule;
	*out_fn = fn;

	return 0;
};
vstplugin* vsthost::getPlugin(AEffect* aeffect) {

	int count = list.size();
	for (int i = 0; i < count; ++i)
	{
		vstplugin *current = list[i];
		if (current) {
			if (current->handle->aeffect == aeffect)
				return current;
		}
	}

	return NULL;
}
void vsthost::unloadPlugin(vstplugin* plugin) {
	PopupCtrl::get()->close(); // Make sure context controls do not reference vst
	plugin->close();
	auto it = std::find(list.begin(), list.end(), plugin);
	if (it != list.end()) {
		list.erase(it);
	}
	if (plugin->handle->tr_plugins) {
		plugin->handle->tr_plugins->removePlugin(plugin);
	}
	plugin->unload();
	moduleMgr->queueRelease(plugin->handle->hmodule);
	delete plugin;
}
bool vsthost::unloadAllPlugins() {
	int count = list.size();
	for (int i = 0; i < count; ++i)
	{
		vstplugin *current = list[i];
		if (current->handle && current->handle->tr_plugins) {
			current->handle->tr_plugins->removePlugin(current);
		}
	}
	for (int i = 0; i < count; ++i)
	{
		vstplugin *current = list[i];
		current->close();
		list[i] = NULL;
		current->unload();
		delete current;
	}
	list.clear();
	return NULL;
}
vstplugin* vsthost::getPluginIdx(uint32_t i) {
	return list[i];
}
uint32_t vsthost::pluginCount() {

	return list.size();
}


vstplugin::~vstplugin() {
	if (blockInputs)
		delete blockInputs;
	if (blockOutputs)
		delete blockOutputs;
	delete handle;
}
track_plugins_t* vsthost::createAudio(track_t* track) {
	return new track_plugins_t(track, this->lSampleRate, this->lBlockSize, OUTPUT_CHANNELS);
}
bool vsthost::movePlugin(track_t* dstTr, track_plugins_t* trp, int32_t src, int32_t dst) {
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	assert((src==0?src==dst:(src>0&&dst>0)));
	my_printf("move from %s:%d to %s:%d\n", StringAsCStr(dstTr->name), src, StringAsCStr(trp->track->name), dst);
	assert(dstTr->audio);
//	if (!dstTr->audio) { //TODO: move me some central place
//		dstTr->audio = vsthost::getInstance()->createAudio(dstTr);
//	}
	if (src > 0) {
		assert(src > 0 && dst > 0);
		src--;
		dst--;
		vstplugin* tmpPlugin = trp->effects[src];
		trp->removePlugin(tmpPlugin);
		dstTr->audio->insertEffect(dst, tmpPlugin);
	} else {
		assert(src == 0 && dst == 0);
		vstplugin* tmpPlugin = trp->instrument;
		trp->removePlugin(tmpPlugin);
		vstplugin* old = dstTr->audio->setInstrument(tmpPlugin);
		if (old) {
			unloadPlugin(old);
		}
	}
	return true;
}
bool vsthost::swapEffects(track_plugins_t* trp, int32_t src, int32_t dst) {
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	assert(src > 0 && dst > 0);
	src--;
	dst--;

	assert((int32_t)trp->effects.size() > src);
	assert((int32_t)trp->effects.size() > dst);
	vstplugin* tmpPlugin = trp->effects[dst];
	trp->effects[dst] = trp->effects[src];
	trp->effects[src] = tmpPlugin;
	trp->effects[src]->handle->slot = src+1;
	trp->effects[dst]->handle->slot = dst+1;
	return true;
}
bool vsthost::insertNewPlugin(track_plugins_t* trp, vstplugin* plugin, int32_t dst) {
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	if (plugin->isSynth) {
		vstplugin* old = trp->setInstrument(plugin);
		if (old) {
			unloadPlugin(old);
		}
	} else {
		trp->insertEffect(dst-1, plugin);
	}
	return true;
}
vstpluginloadres vsthost::loadPlugin(String filepath) {
	String path, name, nameWithoutExt;
	SplitPath(filepath, &path, &nameWithoutExt, NULL, &name);
	VSTPluginMain_t* fn = NULL;
	HMODULE hmodule = NULL;
	int32_t ret = loadLib(filepath, &fn, &hmodule);
	if (ret != 0) {
		return vstpluginloadres(ret, NULL);
	}

	AEffect* aeffect = fn(audioMaster);
	if (!aeffect) {
		FreeLibrary(hmodule);
		return vstpluginloadres(-5, NULL);
	}
	if (aeffect->magic != kEffectMagic) {
		FreeLibrary(hmodule);
		return vstpluginloadres(-6, NULL);
	}

	vstplugin* plugin = new vstplugin(new handles_t(aeffect, hmodule), path, nameWithoutExt);
	list.push_back(plugin);
	plugin->load(this);
	return vstpluginloadres(0, plugin);
};
