#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "dsp_util.h"

#include "vst_host.h"
#include "fileio.h"
#include "track.h"
#include "basectrl.h"
#include "host/mainctrl.h"

#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"

#include "../vstsdk-host-2.4/aeffectx.h"
#include "portaudio.h"
#include "appsettings.h"

#include "logging.h"
#include "audioblock.h"
#include "audiobuffer.h"
#include "platform.h"

#include <stdlib.h>
#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include "track_impl.h"
#include "projectcontroller.h"
#include "threads/threadlock.h"

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __linux__
#include <dlfcn.h>
#endif
#include "../util/readerwriterqueue.h"

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


#define NUM_HOST_CB_SLOTS 4
namespace
{
struct vst_internal_hostslot {
	vsthost* g_instance = nullptr;
};
vst_internal_hostslot g_hostslots[4];
}

VstIntPtr audioMasterHost(vsthost* host, AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	assert(host);
	if (!host)
		return 0;
	vstplugin* plugin = host->getPlugin(effect);

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
	assert(host);
	return audioMasterHost(host, effect, opcode, index, value, ptr, opt);
}
VstIntPtr VSTCALLBACK audioMaster2(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	vsthost* host = g_hostslots[1].g_instance;
	assert(host);
	return audioMasterHost(host, effect, opcode, index, value, ptr, opt);
}
VstIntPtr VSTCALLBACK audioMaster3(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	vsthost* host = g_hostslots[2].g_instance;
	assert(host);
	return audioMasterHost(host, effect, opcode, index, value, ptr, opt);
}
VstIntPtr VSTCALLBACK audioMaster4(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
	vsthost* host = g_hostslots[3].g_instance;
	assert(host);
	return audioMasterHost(host, effect, opcode, index, value, ptr, opt);
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
	vsthost* host = static_cast<vsthost*>(userData);
	float **inputs = (float**)inputBuffer;
	UNUSED(inputs);
	float **outputs = (float**)outputBuffer;
//	if (!inputs) {
//		host->blockTemp->clear();
//		inputs = host->blockTemp->f;
//	}
	dsp_util::fillSilence(outputs, framesPerBuffer);
	if (!host) {
		return paAbort;
	}

	//still a race condition on_terminate here
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
	vsthost* host = static_cast<vsthost*>(userData);
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

String getModuleName(HMODULE);
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
#ifdef __linux__
		dlclose(module);
#endif
	}
};


vsthost::~vsthost() {
	delete moduleMgr;
	delete blockZero;
}
vsthost::vsthost(uint32_t _sampleRate, uint16_t _blockSize)
	: moduleMgr{new vsthost::ModuleManager{}},
	  lSampleRate(_sampleRate),
	  lBlockSize(_blockSize),
	  numChannels(OUTPUT_CHANNELS)
{
	memset(&timeinfo, 0, sizeof(timeinfo));
	allocRingBuffer(ringbuffer);
	updateTime(0, 0, playback_state::status_stop);
	setBlockSize(_blockSize);
}
void vsthost::setBlockSize(uint16_t _blockSize) {
	if (blockZero)
		delete blockZero;
	blockZero = new AudioBlock(numChannels, _blockSize);
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

void vsthost::sendNotesOff(effectbase* plugin) {
	if (plugin && plugin->trackImpl) {
		track_t* tr = plugin->trackImpl->getTrack();
		assert(tr);
		track_impl_t* audio = tr->audio;
		if (audio) {
			audio->sendNotesOff(project.tempo100, 0);
		}
	}
}
void delayAudio(DelayLine* delayLine, AudioBlock* input, AudioBlock* output, samplerate_t delay) {
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

int32_t vsthost::processPlayback(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround) {
	double since = timer.getTimeDoubleReset();
	timer2.reset();
//	MainCtrl* ctrl = MainCtrl::get();
//	static AudioBuffer* master = allocateBuffer();
//	master->input->realloc(lBlockSize);
//	master->output->realloc(lBlockSize);
	int32_t& readPos = ringbuffer.readPos;
	int32_t& writePos = ringbuffer.writePos;
	AudioBuffer** buffers = ringbuffer.buffers;
	while (ctrl) {
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
	if (ctrl) {
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
		for (track_t* track : ctrl->trackCtr) {
			assert(track->audio);
		}
		tick_t pos = floor(posDouble);
		updateTime(sample, pos, state);
		for (track_t* tr : ctrl->trackList) {
			std::vector<automatable_t*> targets;
			tr->audio->getAutomatableTrackTargets(targets);
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
			audioTrack->pluginsChanged();
			samplerate_t latency = audioTrack->getLatency();
			maxLatency = math::max(latency, maxLatency);
		}
		tick_t loopCutStart = -1;
		tick_t loopCutEnd = -1;
		if (inLoop) {
			loopCutStart = project.loopStart;
			loopCutEnd = project.loopStart+project.loopLen;
		}
		for (track_t* track : ctrl->trackCtr) {
			track_impl_t* trackImpl = track->audio;
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
			delayAudio(&trackImpl->delayLine, &trackImpl->output, &trackImpl->output, delay);
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
		bufferWrite->output->realloc(lBlockSize);
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
		double blockPosSample = sample;
		double blockPosTick = posDouble;
		sample = samplePosBlockEnd;
		posDouble += ticksPerBlock;
#ifndef NDEBUG
		lastTickEndPos = posDouble;
#endif
//		dsp_util::fillSqare(fSampleRate, 440, bufferWrite->master->f, bufferWrite->master->samples);
		bufferWrite->submitted = true;
		bufferWrite->inUse = true;
		bufferWrite->blockPosSample = blockPosSample;
		bufferWrite->blockPosTick = blockPosTick;
		writePos++;
		writePos &= RING_BUF_MASK;
		audioQueue.enqueue(bufferWrite);
		nBlocksProcessed++;
		}
	}
	if (ctrl) {
		for (track_t* tr : ctrl->trackList) {
			track_impl_t* trAudio = tr->audio;
			if (trAudio) {
				trAudio->onTick(since);
			}
		}
	}
	int64_t timeTaken = timer2.getTime();

	int64_t microSecsPerBlock = (int64_t)this->lBlockSize * 1000000L / (int64_t)this->lSampleRate;
	stats.timeLastBlock = (stats.timeLastBlock*99 + timeTaken) / 100;
	stats.usage = stats.timeLastBlock / (double) microSecsPerBlock;
	stats.blocksProcessed += nBlocksProcessed;
	stats.samplesProcessed += nBlocksProcessed*lBlockSize;
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
void vsthost::toggleAudioEngineOnOff() {
	if (isStreaming()) {
		stopAudio();
		settings.startEngine = false;
	} else {
		if (startAudio()) {
			settings.startEngine = true;
		}
	}
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

void vsthost::processAudio(audio_stage_t* stage, AudioBlock* input, AudioBlock* output, unsigned long samples) {
	int count = 0;
	if (stage->effects.size()) {
		count += stage->effects.size();
	}

	int64_t microSecsPerBlock = (int64_t)this->lBlockSize * 1000000L / (int64_t)this->lSampleRate;
	hires_timer_t timer;
	for (int i = 0; i < count; ++i)
	{
		effectbase *current = NULL;
		current = stage->effects[i];
		if (!current->bIsSetup) {
			continue;
		}
		processing.pluginId = current->projectGlobalId;
		assert(current->bIsSetup);
		timer.reset();
		bool isBypass = current->isBypass();
		AudioBlock* blockPostProcess;
		if (isBypass) {
			samplerate_t delay = current->getDelay();
			if (delay > 0) {
				if (!current->delayLine.get()) {
					current->delayLine.reset(new DelayLine(this->numChannels, this->lBlockSize));
				}
				AudioBlock* blockOut = current->blockOutputs;
				delayAudio(current->delayLine.get(), input, blockOut, delay);
				input = blockOut;
			}
			blockPostProcess = blockZero;
		} else {
			//blockIn/blockOut will always have 2 channels at least
			AudioBlock* blockIn = current->blockInputs;
			AudioBlock* blockOut = current->blockOutputs;
			blockIn->realloc(lBlockSize);
			blockOut->realloc(lBlockSize);
			//TODO: respect pin configuration and mono plugins
			blockIn->copyFrom(input);

			current->process(blockIn, blockOut, samples);
			input = blockOut;
			blockPostProcess = blockOut;
		}
		current->postProcess(blockPostProcess, samples, isBypass);
		current->fTimePercentBlockProcess = ((current->fTimePercentBlockProcess*49.0)+(timer.getTime() / (double) microSecsPerBlock))/50.0;
		processing.pluginId = 0;
	}
	//   If a plugin runs mono inputs or outputs we need to handle this manually here
	output->copyFrom(input);

	float gain = dsp_util::clampReadGain(stage->mixer.getGain());
	mulGain(output, gain);

}
void vsthost::updatePluginWindows() {
	for (auto* plugin : pluginInstancesVST2) {
//		plugin->dispatch(effEditIdle);
		plugin->updateWindow();
	}
}
bool vsthost::onTick() {
	int iDispatched = 0;
	for (auto* current : pluginInstancesVST2) {
		if (current->bEditOpen && !current->bInEditIdle) {
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
	freeRingBuffer(ringbuffer);
	assert(hostSlot > -1);
	assert(g_hostslots[hostSlot].g_instance);
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
	assert(0&&"Out of host slots");
	return false;
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
			const char* pref = "[ ] ";
			if (!strcmp(selApiNameCStr, info->name)) {
				deviceApiIdxSelected = i;
				pref = "[x] ";
			}
			my_printf("%sAPI[%d] = %s %d devices\n", pref, i, info->name, info->deviceCount);
		}
	}
	if (deviceApiIdxSelected >= 0) {
		int deviceCount = Pa_GetDeviceCount();
		for (int i = 0; i < deviceCount; i++) {
			const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
			const char* pref = "[ ] ";
			if (info && info->hostApi == deviceApiIdxSelected && info->maxOutputChannels > 0) {
				if (!strcmp(selDevNameCStr, info->name)) {
					deviceIdxSelected = i;
					pref = "[x] ";
				}
				my_printf("%sDEVICE[%d] = %s %d output channels\n", pref, i, info->name, info->maxOutputChannels);
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
	my_printf("channelCount %d\n", outputParameters.channelCount);
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
		this);

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
	this->blockZero->realloc(lBlockSize);
	return true;
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
	assert(track->audio);
	auto audio = track->audio;
	std::vector<effectbase*> effects = audio->effects; // make a copy before unloading plugins
	for (effectbase* effect : effects) {
		unloadPlugin(effect);
	}
}
void vsthost::removePlugin(effectbase* plugin) {
	audio_stage_t* audioStage = plugin->getTrackLink();
	audioStage->removePlugin(plugin, true);
	audioStage->pluginsChanged();
}
template<typename T>
void removeErase(std::vector<T> t, T& t2) {

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
		assert(removeEntry(pluginsDeferred, plugin));
		break;
	case PLUGIN_TYPE_INTERNAL_EFFECT:
	case PLUGIN_TYPE_VST:
		assert(removeEntry(pluginInstancesVST2, plugin));
		assert(removeEntry(pluginInstances, plugin));
		break;
	default:
		assert(removeEntry(pluginInstancesInternal, plugin));
		assert(removeEntry(pluginInstances, plugin));
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
	assert(pluginInstances.empty());
	assert(pluginInstancesVST2.empty());
	assert(pluginInstancesInternal.empty());
	assert(allAudioStages.empty());
	assert(trackAudioStages.empty());
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
	auto audio = new track_impl_t(getNextGlobalAudioStageId(0), track, this->lSampleRate, this->lBlockSize, OUTPUT_CHANNELS);
	allAudioStages.push_back(audio);
	trackAudioStages.push_back(audio);
	track->audio = audio;
}
void vsthost::releaseAudio(track_t* track) {
	auto audioStage = track->audio;
	assert(audioStage);
	track->audio = nullptr;
	auto it = std::find(allAudioStages.begin(), allAudioStages.end(), audioStage);
	assert(it != allAudioStages.end());
	allAudioStages.erase(it);
	auto it2 = std::find(trackAudioStages.begin(), trackAudioStages.end(), audioStage);
	assert(it2 != trackAudioStages.end());
	trackAudioStages.erase(it2);
	delete audioStage;
}
audio_stage_t* vsthost::createAudioStage() {
	auto audio = new audio_stage_t(getNextGlobalAudioStageId(0), this->lSampleRate, this->lBlockSize, OUTPUT_CHANNELS);
	allAudioStages.push_back(audio);
	return audio;
}
void vsthost::releaseAudioStage(audio_stage_t* audioStage) {
	auto it = std::find(allAudioStages.begin(), allAudioStages.end(), audioStage);
	assert(it != allAudioStages.end());
	allAudioStages.erase(it);
}
audio_stage_t* vsthost::getAudioStage(const audio_stage_ref_t& ref) {
	auto it = std::find_if(allAudioStages.begin(), allAudioStages.end(), [ref] (const audio_stage_t* ptr) {
		return ptr->id == ref.id;
	});
	assert(it != allAudioStages.end());
	if (it != allAudioStages.end()) {
		return *it;
	}
	return nullptr;
}
bool vsthost::movePlugins(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	assert(dstTr);
	assert(trp);
	assert(src < (int)trp->effects.size());
	assert(src+len <= (int)trp->effects.size());
	assert(dst-1 <= (int)dstTr->effects.size());
	std::vector<effectbase*> tmpEffects = trp->effects;
	for (int32_t i = 0; i < len; i++) {
		effectbase* tmpPlugin = tmpEffects[src + i];
		trp->removePlugin(tmpPlugin, true);
		dstTr->insertEffect(dst+i, tmpPlugin);
	}
	trp->pluginsChanged();
	dstTr->pluginsChanged();
	return true;
}
bool vsthost::moveEffects(audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
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
	return true;
}

bool vsthost::replacePlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst, effectbase** prevPlugin) {
	//TODO: call pluginsChanged, update latency
	return trp->replaceEffect(dst, plugin, prevPlugin);
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

int32_t vsthost::getNextGlobalAudioStageId(int32_t globalId) {
	if (globalId <= 0) {
		return ++audioStageId;
	} else {
		update_maximum(audioStageId, globalId);
	}
	return globalId;
}

vstpluginloadres vsthost::loadPlugin(String filepath, int32_t globalId) {
	assert(masterCallBackSlot);
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
	vstplugin* plugin = new vstplugin(new handles_t(nullptr, aeffect, moduleHandle), globalId, path, nameWithoutExt, -1);
	pluginInstancesVST2.push_back(plugin);
	pluginInstances.push_back(plugin);
	plugin->load(this);
	return vstpluginloadres(0, plugin);
};
