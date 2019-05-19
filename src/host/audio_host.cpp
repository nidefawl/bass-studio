#include "audio_host.h"
#include "config.h"
#include "samplerate.h"
#include "str_util.h"
#include "seq_time.h"
#include "seq_util.h"
#include "dsp_util.h"
#include "audiobuffer.h"
#include "audioblock.h"
#include "logging.h"
#include "appsettings.h"
#include "platform.h"
#include <portaudio.h>
#include <vector>
//#include <memory>


bool error(const char* msg, PaError err) {
	log_printf("Error in %s\n", msg);
	log_printf("Error number: %d\n", err);
	log_printf("Error message: %s\n", Pa_GetErrorText(err));
	return false;
}

/*
** This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int audioCallback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	audiohost* host = static_cast<audiohost*>(userData);
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
	AudioBuffer* block = nullptr;
	if (host->try_dequeue(block)) {
		dbgassert(block);
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
	dbgassert(userData);
	audiohost* host = static_cast<audiohost*>(userData);
	host->onStreamEnd();
}

audiohost::audiohost(uint32_t _sampleRate, uint16_t _blockSize)
	: lSampleRate(_sampleRate),
	  lBlockSize(_blockSize),
	  numChannels(OUTPUT_CHANNELS)
{
}

void audiohost::enqueue(AudioBuffer* buf) {
//	dbgassert(isStreaming());
	this->audioQueue.enqueue(buf);
}
bool audiohost::try_dequeue(AudioBuffer*& buf) {
	return this->audioQueue.try_dequeue(buf);
}
bool audiohost::initPa() {
	if (!paIsInitalized) {
		PaError err;
		err = Pa_Initialize();
		if (err != paNoError) {
			Pa_Terminate();
			error("Pa_Initialize", err);
		} else {
			paIsInitalized = true;
		}
	}
	return paIsInitalized;
}
void audiohost::deinitPa() {
	if (paIsInitalized) {
		Pa_Terminate();
		paIsInitalized = false;
	}
}
void audiohost::onStreamEnd() {
	my_printf("onStreamEnd.\n", 0);
	stream = NULL;
	AudioBuffer* block;
	while (audioQueue.try_dequeue(block)) {
		block->inUse = false;
	}
}


bool audiohost::startAudio() {
	my_printf("startAudio\n", 0);
	if (!initPa())
		return false;
	int apiCount = Pa_GetHostApiCount();
	const char* selApiNameCStr = StringAsCStr(settings.iosettings.device_api);
	if (settings.iosettings.getConfig(settings.iosettings.device_api).outputs.size() < 1)
		return error("settings.iosettings.outputs.size() < 1", 0);
	const char* selDevNameCStr = StringAsCStr(settings.iosettings.getConfig(settings.iosettings.device_api).outputs[0].deviceName);
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
		return error("outputParameters.device == paNoDevice", 0);
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

	PaError err = Pa_OpenStream(
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
	return true;
}
bool audiohost::stopAudio() {
	PaStream* stream = this->stream;
	if (stream) {
		my_printf("stopAudio.\n", 0);
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
