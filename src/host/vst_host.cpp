#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "seq_math.h"
#include "dsp_util.h"

#include "vst_host.h"
#include "fileio.h"
#include "track.h"
#include "mainctrl.h"

#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"

#include "../vst_sdk_2.4/aeffectx.h"
#include "portaudio.h"
#include "settings.h"

#include "logging.h"
#include "audioblock.h"
#include "platform.h"

#include <stdlib.h>
#include <algorithm>
#include <stdlib.h>
#include <memory.h>
#include "track_impl.h"

#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __MINGW32__
#include "../platform/mingw/mingw.mutex.h"
#endif
#ifdef __linux__
#include <dlfcn.h>
#endif
#include "leak_detect.h"

#define DBG_PRINT_CALLBACKS
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
	const char* canDoReportConnectionChanges = "reportConnectionChanges"; ///< Host will indicates the plug-in when something change in plug-in�s routing/connections with #suspend/#resume/#setSpeakerArrangement 
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
namespace
{
	std::unique_ptr<vsthost> g_instance;
}


VstIntPtr VSTCALLBACK audioMaster(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	vstplugin* plugin = g_instance->getPlugin(effect);

	switch (opcode)
	{
	case audioMasterAutomate:
		cbPrintf(plugin, "audioMasterAutomate %d %d %d\n", index, opcode, value);
		if (plugin) {
			plugin->deactivateAutomation(index);
			plugin->recvPluginEditParamUpdate(index);
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
		return (VstIntPtr)vsthost::getInstance()->getTimeInfo();
	case audioMasterProcessEvents:
		cbPrintf(plugin, "audioMasterProcessEvents %d %d %d\n", index, opcode, value);
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
//String getModuleName(HMODULE module);
class vsthost::ModuleManager {
public:
	ModuleManager() {

	}

	void releaseModule(void* module) {
#ifdef _WIN32
		FreeLibrary((HMODULE)module);
#endif
#ifdef __linux__
		dlclose(module);
#endif
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
vsthost::vsthost(uint32_t _sampleRate, uint16_t _blockSize)
	: moduleMgr{new vsthost::ModuleManager{}},
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
	timeinfo.nanoSeconds = (double)getTimeMillis() * 1000000.0L;
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

void vsthost::sendNotesOff(vstplugin* plugin) {
	handles_t* handles = plugin->handle;
	if (plugin && plugin->trackImpl) {
		track_t* tr = plugin->trackImpl->getTrack();
		assert(tr);
		track_impl_t* audio = tr->audio;
		if (audio) {
			audio->sendNotesOff(project.tempo100, 0);
		}
	}
}
void delayAudio(DelayLine* delayLine, AudioBlock* output, samplerate_t delay) {
	int32_t bufSize = (int32_t)output->samples;
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
	delayBlock.copyFromPosToPos(output->buf, 0, writePos, output->samples, output->channels);
	if (readPos + (int32_t)output->samples > delayLineSize) {
		int32_t read1Len = delayLineSize - readPos;
		int32_t read2Len = output->samples - read1Len;
		output->copyFromPosToPos(delayBlock.buf, readPos, 0, read1Len, delayBlock.channels);
		output->copyFromPosToPos(delayBlock.buf, 0, read1Len, read2Len, delayBlock.channels);
	} else {
		output->copyFromPosToPos(delayBlock.buf, readPos, 0, output->samples, delayBlock.channels);
	}

}

int32_t vsthost::processPlayback(int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround) {
	double since = timer.getTimeDoubleReset();
	MainCtrl* ctrl = MainCtrl::get();
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
		for (track_t* tr : ctrl->trackList) {
			std::vector<automatable_t*> targets;
			tr->audio->getAutomatableTargets(targets);
			for (automatable_t* at : targets) {
				at->updateAutomatedParameters(pos);
			}
		}
		int32_t samplePosBlockEnd = sample + lBlockSize;
		int32_t tickBlockEnd = floor(posDouble + ticksPerBlock);
		assert(tickBlockEnd-pos < ceil(ticksPerBlock+1));
		/*
		 * Clear all master channels first
		 */
		for (track_t* trackMaster : ctrl->trackMasterCtr) {
			assert(trackMaster->audio);
			track_impl_t* audioMaster = trackMaster->audio;
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
		samplerate_t maxLatency = 0;
		for (track_t* track : ctrl->trackCtr) {
			track_impl_t* audioTrack = track->audio;
			assert(audioTrack);
			audioTrack->pluginsChanged();
			samplerate_t latency = audioTrack->getLatency();
			maxLatency = max(latency, maxLatency);
		}
		tick_t loopCutStart = -1;
		tick_t loopCutEnd = -1;
		if (inLoop) {
			loopCutStart = project.loopStart;
			loopCutEnd = project.loopStart+project.loopLen;
		}
		for (track_t* track : ctrl->trackCtr) {
			track_impl_t* trackImpl = track->audio;
			if (!trackImpl) {
				track->audio = trackImpl = vsthost::getInstance()->createAudio(track);
			}
			trackImpl->input.realloc(lBlockSize);
			trackImpl->output.realloc(lBlockSize);
			dsp_util::fillSilence(trackImpl->input.buf, lBlockSize);
			if (state == playback_state::status_play) {
				trackImpl->sendNotes(pos, tickBlockEnd, loopCutStart, loopCutEnd, project.tempo100, sample);
			} else if (!trackImpl->heldNotes.empty()) {
				trackImpl->sendNotesOff(project.tempo100, sample);
			}
			if (state == playback_state::status_play) {
				trackImpl->fillAudio(pos, tickBlockEnd, loopCutStart, loopCutEnd, project.tempo100, sample, trackImpl->input.buf, (int32_t)lBlockSize);
			}


			/* Processes a whole plugin chain */
			processAudio(trackImpl, &trackImpl->input, &trackImpl->output, lBlockSize);
			samplerate_t delay = maxLatency - trackImpl->getLatency();
			delayAudio(&trackImpl->delayLine, &trackImpl->output, delay);
			if (trackImpl->mixer.isEnabled()) {
				for (track_t* trackMaster : ctrl->trackMasterCtr) {
					track_impl_t* audioMaster = trackMaster->audio;
					audioMaster->input.addFrom(&trackImpl->output);
				}
			}
		}

		for (track_t* trackMaster : ctrl->trackMasterCtr) {
			track_impl_t* audioMaster = trackMaster->audio;
//			audioMaster->mixer.setGain(0);
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
			track_impl_t* audioMaster = trackMaster->audio;
			if (audioMaster->mixer.isEnabled()) {
				AudioBlock* bufMaster = &audioMaster->output;
				for (int n = 0; n < OUTPUT_CHANNELS; n++) {
					float* channelWriteBuffer = bufOut->buf[n];
					float* channelMaster = bufMaster->buf[n];
					for (int j = 0; j < lBlockSize; j++) {
						channelWriteBuffer[j] += channelMaster[j];
					}
				}
			}
			break;
		}
		/* Update all track meters */
		for (track_t* track : ctrl->trackList) {
			track_impl_t* trAudio = track->audio;
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
		track_impl_t* trAudio = tr->audio;
		if (trAudio) {
			trAudio->onTick(since);
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
	lastTickEndPos = 0;
	lastState = playback_state::status_stop;
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
void vsthost::processAudio(audio_stage_t* channel, AudioBlock* input, AudioBlock* output, unsigned long samples) {
	int count = 0;
	if (channel->effects.size()) {
		count+=channel->effects.size();
	}


	for (int i = 0; i < count; ++i)
	{
		effectbase *current = NULL;
		current = channel->effects[i];
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

//		handles_t* handle = current->handle;
		current->process(blockIn, blockOut, samples);
		//TODO: maybe sanitize plugins output floats here (NaN/Inf/ >50 dBFS)
		input = blockOut;
		current->meter.update(blockOut);
	}
	//   If a plugin runs mono inputs or outputs we need to handle this manually here
	output->copyFrom(input);

	float gain = dsp_util::clampReadGain(channel->mixer.getGain());
	mulGain(output, gain);

}
void vsthost::updateDisplay() {
	int count = list.size();
	for (int i = 0; i < count; ++i)
	{
		vstplugin *current = list[i];
		if (current) {
//			current->dispatch(effEditIdle);
			current->updateDisplay();
		}
	}
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
//			if (current->window) {
//				current->updateDisplay();
//			}
			iDispatched++;
		}
	}
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
			threadSleep(100);
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
	g_instance.reset();
}
vsthost* vsthost::getInstance()
{
	return g_instance.get();
}
void vsthost::setInstance(std::unique_ptr<vsthost> host)
{
	g_instance = std::move(host);
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
			my_printf("API[%d] = %s %d devices\n", i, info->name, info->deviceCount);
			if (!strcmp(selApiNameCStr, info->name)) {
				deviceApiIdxSelected = i;
			}
		}
	}
	if (deviceApiIdxSelected >= 0) {
		int deviceCount = Pa_GetDeviceCount();
		for (int i = 0; i < deviceCount; i++) {
			const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
			if (info && info->hostApi == deviceApiIdxSelected && info->maxOutputChannels > 0) {
				my_printf("DEVICE[%d] = %s %d output channels\n", i, info->name, info->maxOutputChannels);
				if (!strcmp(selDevNameCStr, info->name)) {
					deviceIdxSelected = i;
				}

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
void vsthost::unloadTrack(track_t* track) {
	assert(track->audio);
	auto audio = track->audio;
	std::vector<effectbase*> effects = audio->effects; // make a copy before unloading plugins
	for (effectbase* effect : effects) {
		unloadPlugin(effect);
	}
}
void vsthost::removePlugin(effectbase* plugin) {
	audio_stage_t* audioStage = plugin->getTrackLink();
	audioStage->removePlugin(plugin, false);
	audioStage->pluginsChanged();
}
void vsthost::unloadPlugin(effectbase* plugin) {
	if (MainCtrl::get())
		MainCtrl::get()->closeContextMenu();
	plugin->onPreUnload();
	removePlugin(plugin);
	plugin->unload();
//	PopupCtrl::get()->close(); // Make sure context controls do not reference vst
	plugin->close();
	auto it = std::find(list.begin(), list.end(), plugin);
	if (it != list.end()) {
		list.erase(it);
	}
	vstplugin* vst = dynamic_cast<vstplugin*>(plugin);
	if (vst) {
		moduleMgr->releaseModule(vst->handle->hmodule);
	}
	delete plugin;
}
bool vsthost::unloadAllPlugins() {
	int count = list.size();
	for (int i = 0; i < count; ++i)
	{
		vstplugin *current = list[i];
		if (current->trackImpl) {
			current->trackImpl->removePlugin(current, false);
		}
	}
	for (int i = 0; i < count; ++i)
	{
		vstplugin *current = list[i];
		current->close();
		list[i] = NULL;
		current->unload();
		moduleMgr->releaseModule(current->handle->hmodule);
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
track_impl_t* vsthost::createAudio(track_t* track) {
	return new track_impl_t(track, this->lSampleRate, this->lBlockSize, OUTPUT_CHANNELS);
}
audio_stage_t* vsthost::createAudioStage() {
	return new audio_stage_t(this->lSampleRate, this->lBlockSize, OUTPUT_CHANNELS);
}
bool vsthost::movePlugin(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t dst) {
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	assert(dstTr);
	assert(trp);
	assert(src < (int)trp->effects.size());
	assert(dst-1 <= (int)dstTr->effects.size());
		effectbase* tmpPlugin = trp->effects[src];
		trp->removePlugin(tmpPlugin, true);
		dstTr->insertEffect(dst, tmpPlugin);
	trp->pluginsChanged();
	dstTr->pluginsChanged();
	return true;
}
bool vsthost::moveEffect(audio_stage_t* trp, int32_t src, int32_t dst) {
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	assert(src >= 0 && dst >= 0);
	assert(src != dst);
//	src--;
//	dst--;
	assert((int32_t)trp->effects.size() > src);
	assert((int32_t)trp->effects.size() > dst);
	for (effectbase* effect : trp->effects) {
		assert(effect->getSlot()>=0);
	}


	//shift element
	std::vector<effectbase*> curEffects = trp->effects;
	std::vector<effectbase*> tmpEffects;
	tmpEffects.resize(trp->effects.size());
	auto itIn = curEffects.cbegin();
	auto itOut = tmpEffects.begin();
	for (;itOut!=tmpEffects.cend();) {
		if (curEffects.cbegin()+src == itIn) {
			itIn++;
		} else if (tmpEffects.cbegin()+dst == itOut) {
			*itOut++ = curEffects[src];
		} else {
			*itOut++ = *itIn++;
		}
	}
	trp->effects = std::move(tmpEffects);
	int slot = 0;
	for (effectbase* effect : trp->effects) {
		effect->setSlot(slot++);
	}
	return true;
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
		FreeLibrary(hmodule);
		return -4;
	}
	*out_hmodule = hmodule;
	*out_fn = fn;

	return 0;
}
#endif
#ifdef __linux__

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
vstpluginloadres vsthost::loadPlugin(String filepath, int32_t globalId) {
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

	aeffect = fn(audioMaster);
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

#ifdef __linux__
	void* hmodule = NULL;
	int32_t ret = loadLib(filepath, &fn, &hmodule);
	if (ret != 0) {
		return vstpluginloadres(ret, NULL);
	}

	aeffect = fn(audioMaster);
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
	vstplugin* plugin = new vstplugin(new handles_t(aeffect, moduleHandle), globalId, path, nameWithoutExt);
	list.push_back(plugin);
	plugin->load(this);
	return vstpluginloadres(0, plugin);
};
