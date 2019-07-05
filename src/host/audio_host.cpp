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
#include <numeric>
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
	if (!userData) {
		return paComplete;
	}
	audiohost::audiostream* stream = static_cast<audiohost::audiostream*>(userData);
	if (stream->streamShouldEnd) {
		return paComplete;
	}
	audiohost* host = stream->host;
	float **inputs = (float**)inputBuffer;
	UNUSED(inputs);
	float **outputs = (float**)outputBuffer;
//	if (!inputs) {
//		host->blockTemp->clear();
//		inputs = host->blockTemp->f;
//	}
	dsp_util::fillChannels(outputs, stream->nOutputChannels, framesPerBuffer, 0.0f);
	if (!host) {
		return paAbort;
	}
	//TODO: still a race condition on_terminate here
	AudioBuffer* block = nullptr;

	if (stream->audioQueue.try_dequeue(block)) {
		dbgassert(block);
		if (framesPerBuffer == block->output->samples) {
			int32_t channels = math::min<int32_t>(block->output->channels, stream->nOutputChannels);
			for (uint32_t i = 0; i < channels; i++) {
				float* channel = block->output->buf[i];
				memcpy(outputs[i], channel, framesPerBuffer * sizeof(float));
			}


//			if (logEveryMsec(123, 3000, StringFormat("stream %d samples %d channels OUT:%s\n", framesPerBuffer, channels, StringAsCStr(stream->outputName)))) {
//				float* vec = block->output->buf[0];
//				float lvl = std::accumulate(vec, vec+framesPerBuffer, 0.0f, [](float f, float fVal) {
//					return f+fVal*fVal;
//				});
//				log_printf("std::accumulate: %f\n", lvl);
//				lvl = lvl / framesPerBuffer;
//				log_printf("lvl / framesPerBuffer: %f\n", lvl / framesPerBuffer);
//				lvl = lvl < 0.01f ? 0.0f : sqrtf(lvl);
//				log_printf("Level: %f\n", lvl);
//			}
		}
		block->inUse = false;
	} else {
		host->bufferUnderuns++;
//		dsp_util::fillSilence(inputs, framesPerBuffer);
	}
	dsp_util::fillSaturate(outputs, stream->nOutputChannels, framesPerBuffer);
	host->blockReads++;
	//	dsp_util::fillSqare(host->fSampleRate, 440, inputs, framesPerBuffer);
	audiothread_ringbuffer_t& ringbuffer = stream->getRingbuffer();
	int32_t& writePos = ringbuffer.writePos;
	AudioBuffer** buffers = ringbuffer.buffers;

	AudioBuffer* bufferWrite = buffers[writePos];
	bufferWrite->output->realloc(framesPerBuffer);
	if (inputs) {
		bufferWrite->output->copyFrom(inputs, framesPerBuffer, stream->nInputChannels);
//		logEveryMsec(124, 3000, StringFormat("stream IN:%s\n", StringAsCStr(stream->inputName)));
	} else {
		dsp_util::fillChannels(outputs, stream->nOutputChannels, framesPerBuffer, 0.0f);
	}
	bufferWrite->submitted = true;
	bufferWrite->inUse = true;
	bufferWrite->blockPosSample = timeInfo->inputBufferAdcTime;
	bufferWrite->blockPosTick = 0;
	writePos++;
	writePos &= RING_BUF_MASK;
	if (stream) {
		stream->enqueueInput(bufferWrite);
	} else {
		bufferWrite->inUse = false;
	}


	return paContinue;
}
/*
* This routine is called by portaudio when playback is done.
*/
static void StreamFinished(void* userData)
{
	dbgassert(userData);
	audiohost::audiostream* stream = static_cast<audiohost::audiostream*>(userData);
	stream->streamFinished = true;
}

audiohost::audiostream::audiostream(int32_t _nStreamId, int32_t _nOutputChannels, int32_t _nInputChannels)
	: nInputChannels(_nInputChannels),
	  streamId(_nStreamId),
	  nOutputChannels(_nOutputChannels)
{
	allocRingBuffer(ringbuffer, nInputChannels);
	metersInput.resize(nInputChannels);
	metersOutput.resize(nOutputChannels);
	for (int32_t i = 0; i < nInputChannels; i++) {
		metersInput[i] = std::make_shared<meterType>();
	}
	for (int32_t i = 0; i < nOutputChannels; i++) {
		metersOutput[i] = std::make_shared<meterType>();
	}
}
audiohost::audiostream::~audiostream() {
	freeRingBuffer(ringbuffer);
}
audiohost::audiohost(uint32_t _sampleRate, uint16_t _blockSize)
	: lSampleRate(_sampleRate),
	  lBlockSize(_blockSize),
	  numChannels(OUTPUT_CHANNELS)
{
	nextStreamId = 0;
}
audiohost::audiostream* audiohost::getStream(int idx) {
	if (streams.size() > idx) {
		auto it = std::find_if(streams.begin(), streams.end(), [idx](std::shared_ptr<audiostream>& ptr) {
			return ptr->idx == idx;
		});
		if (it != streams.end()) {
			return it->get();
		}
	}
	return nullptr;
}
audiohost::~audiohost() {
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
void audiohost::removeStream(audiostream* stream) {
	stream->stream = nullptr;
	my_printf("onStreamEnd.\n", 0);
	AudioBuffer* block;
	while (stream->audioQueue.try_dequeue(block)) {
		block->inUse = false;
//		block->submitted = false;
	}
	while (stream->audioQueueInput.try_dequeue(block)) {
		block->inUse = false;
//		block->submitted = false;
	}
	streams.erase( std::remove_if( begin(streams),end(streams),
	    [stream](std::shared_ptr<audiostream>& x){return x.get() == stream;}), end(streams) );
}

void audiohost::audiostream::enqueueInput(AudioBuffer* buf) {
	if (streamFinished) {
		return;
	}
	int32_t nChannels = math::min<int32_t>(buf->output->channels, this->nInputChannels);
	for (int32_t i = 0; i < nChannels; i++) {
		float* arr[1] = { buf->output->buf[i] };
		AudioBlock bufTmp(arr, 1, buf->output->samples);
		metersInput[i]->update(&bufTmp, 1.0f);
		metersInput[i]->onTick(buf->output->samples/(double)this->host->lSampleRate);
	}
//	dbgassert(isStreaming());
	this->audioQueueInput.enqueue(buf);
}
bool audiohost::audiostream::try_dequeueInput(AudioBuffer*& buf) {
	if (streamFinished) {
		return false;
	}
	return this->audioQueueInput.try_dequeue(buf);
}
void audiohost::audiostream::enqueue(AudioBuffer* buf) {
	if (streamFinished) {
		return;
	}
	int32_t nChannels = math::min<int32_t>(buf->output->channels, this->nOutputChannels);
	for (int32_t i = 0; i < nChannels; i++) {
		float* arr[1] = { buf->output->buf[i] };
		AudioBlock bufTmp(arr, 1, buf->output->samples);
		metersOutput[i]->update(&bufTmp, 1.0f);
		metersOutput[i]->onTick(buf->output->samples/(double)this->host->lSampleRate);
	}
//	dbgassert(isStreaming());
	this->audioQueue.enqueue(buf);
}
bool audiohost::audiostream::try_dequeue(AudioBuffer*& buf) {
	if (streamFinished) {
		return false;
	}
	return this->audioQueue.try_dequeue(buf);
}

bool audiohost::startAudio() {
	my_printf("startAudio\n", 0);
	if (!initPa())
		return false;
	stopAudio();
	int apiCount = Pa_GetHostApiCount();
	const char* selApiNameCStr = StringAsCStr(settings.iosettings.device_api);
	auto cfg = settings.iosettings.getConfig(settings.iosettings.device_api);
	auto asioConfig = settings.iosettings.asioConfig;
	using ptr_audiostream = std::shared_ptr<audiostream>;
	std::vector<ptr_audiostream> vecStreams;

//	const char* selDevNameCStr = StringAsCStr(
//			settings.iosettings.getConfig(settings.iosettings.device_api).outputs[0].deviceName);
	int32_t deviceApiIdxSelected = paNoDevice;
	int32_t deviceIdxSelectedInput = paNoDevice;
	int32_t deviceIdxSelectedOutput = paNoDevice;
	int apiIdxASIO = -1;
	for (int i = 0; i < apiCount; i++) {
		const PaHostApiInfo *info = Pa_GetHostApiInfo(i);
		if (info) {
			if (info->type == PaHostApiTypeId::paASIO) {
				apiIdxASIO = i;
			}
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
			if (!info) {
				error("!info for Pa_GetDeviceInfo", i);
				continue;
			}
			const char* pref = "[ ] ";
			if (info->hostApi == deviceApiIdxSelected) {
				bool bAsioMatches = apiIdxASIO == info->hostApi && asioConfig.deviceName == info->name;
				bool bOutputMatches = bAsioMatches || (apiIdxASIO != info->hostApi && cfg.deviceNameOutput == info->name);
				bool bInputMatches = bAsioMatches || (apiIdxASIO != info->hostApi && cfg.deviceNameInput == info->name);
				if (bOutputMatches  && info->maxOutputChannels > 0) {
					deviceIdxSelectedOutput = i;
					pref = "[x] ";
				}
				if (bInputMatches  && info->maxInputChannels > 0) {
					deviceIdxSelectedInput = i;
					pref = "[x] ";
				}
				my_printf("%sDEVICE[%d] = %s %d %s channels\n", pref, i, info->name, info->maxOutputChannels, info->maxInputChannels ? "input" : "output");
			}

		}
	}

	if (deviceIdxSelectedOutput < 0) {
		my_printf("Error: No output device.\n", 0);
		return false;
	}


	const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(deviceIdxSelectedOutput);
	const PaDeviceInfo* devInfoInput = deviceIdxSelectedInput == paNoDevice ? nullptr : Pa_GetDeviceInfo(deviceIdxSelectedInput);
	const PaHostApiInfo* apiInfo = Pa_GetHostApiInfo(deviceApiIdxSelected);
	PaStreamParameters outputParams;
	outputParams.device = deviceIdxSelectedOutput;
	if (outputParams.device == paNoDevice) {
		return error("outputParameters.device == paNoDevice", 0);
	}
	outputParams.channelCount = devInfo->maxOutputChannels;       /* stereo output */
	outputParams.sampleFormat = paFloat32 | paNonInterleaved; /* 32 bit floating point output */
	outputParams.suggestedLatency = devInfo->defaultLowOutputLatency;
	outputParams.hostApiSpecificStreamInfo = NULL;
	my_printf("Open stream on device %s | %s\n", apiInfo->name, devInfo->name);
	my_printf("samplerate %u\n", lSampleRate);
	my_printf("channelCount %d\n", outputParams.channelCount);
	my_printf("lBlockSize %d\n", this->lBlockSize);
	PaStreamParameters* pInputParams = nullptr;
	PaStreamParameters inputParams;
	if (devInfoInput) {
		pInputParams = &inputParams;
		inputParams.device = deviceIdxSelectedInput;
		inputParams.channelCount = devInfoInput->maxInputChannels;       /* stereo output */
		inputParams.sampleFormat = paFloat32 | paNonInterleaved; /* 32 bit floating point output */
		inputParams.suggestedLatency = devInfoInput->defaultLowOutputLatency;
		inputParams.hostApiSpecificStreamInfo = NULL;
	} else {
		inputParams.channelCount = 0;
	}
	my_printf("With %d input channels\n", inputParams.channelCount);
	int32_t streamId = ++nextStreamId;
	auto stream = std::make_shared<audiostream>(streamId, outputParams.channelCount, inputParams.channelCount);
	stream->idx = vecStreams.size();
	stream->host = this;
	stream->outputName = devInfo->name;
	stream->nOutputChannels = devInfo->maxOutputChannels;
	stream->device_api = selApiNameCStr;
	if (devInfoInput) {
		stream->nInputChannels = devInfoInput->maxInputChannels;
		stream->inputName = devInfoInput->name;
	}
	PaStream* paStream = NULL;

	PaError err = Pa_OpenStream(
		&paStream,
		pInputParams,
		&outputParams,
		(double)this->lSampleRate,
		this->lBlockSize,
		paClipOff,      /* we won't output out of range samples so don't bother clipping them */
		audioCallback,
		stream.get());

	if (err != paNoError) {
		return error("Pa_OpenStream", err);
	}

	err = Pa_SetStreamFinishedCallback(paStream, &StreamFinished);
	if (err != paNoError)
		return error("Pa_SetStreamFinishedCallback", err);

	err = Pa_StartStream(paStream);
	if (err != paNoError)
		return error("Pa_StartStream", err);
	stream->stream = paStream;
	log_printf("NEW STREAM HANDLE: %X (%d)\n", (int64_t)stream.get(), stream->streamId);
	this->streams.push_back(stream);

	return true;
}
bool audiohost::stopAudio() {
	int n = 0;
	std::vector<std::shared_ptr<audiostream>> streamsCopy = streams;
	for (auto& sharedPtrStream : streamsCopy) {
		sharedPtrStream->streamShouldEnd = true;
	}
	for (auto& sharedPtrStream : streamsCopy) {
		assert(sharedPtrStream->stream);
		PaError err = Pa_StopStream(sharedPtrStream->stream);
		if (err != paNoError) {
			error("Pa_StopStream", err);
		}
		n++;
	}
	for (auto& sharedPtrStream : streamsCopy) {
		PaError err = Pa_CloseStream(sharedPtrStream->stream);
		if (err != paNoError) {
			error("Pa_CloseStream", err);
		}
		removeStream(sharedPtrStream.get());
	}
	assert(streams.empty());
	return n > 0;
}
