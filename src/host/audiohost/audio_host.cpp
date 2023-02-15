#include "audio_host.h"
#include "config.h"
#include "host/audio_config.h"
#include "math/seq_math.h"
#include "samplerate.h"
#include "str_util.h"
#include "seq_time.h"
#include "seq_util.h"
#include "thread.h"
#include "dsp_util.h"
#include "host/audiobuffer/audiobuffer.h"
#include "host/audiobuffer/audioblock.h"
#include "logging.h"
#include "appsettings.h"
#include "platform.h"
#include "types.h"
#include <portaudio.h>
#ifdef _WIN32
#include <pa_win_wasapi.h>
#endif
#include <vector>
#include <memory>
#include <numeric>
#include <functional>


namespace DAW::AudioIO {
    const std::array<samplerate_t, 4> IntSamplerates = {
        44100U, 48000U, 96000U, 192000U
    };
    const std::array<samplerate_t, 4> ExtSamplerates = {
        44100U, 48000U, 96000U, 192000U
    };
    channel_pairing getTrackTypeFromNumChannels(channelnum_t t) {
        if (t < 2)
            return channel_pairing::MONO;

        if (t < 3)
            return channel_pairing::STEREO;

        if (t < 5)
            return channel_pairing::MULTI_CHANNEL_4;

        return channel_pairing::MULTI_CHANNEL_6;
    }

    channelnum_t getNumChannelsFromTrackType(channel_pairing t) {
        switch (t) {
            default:
            case channel_pairing::MONO:
                return 1;
            case channel_pairing::STEREO:
                return 2;
            case channel_pairing::MULTI_CHANNEL_4:
                return 4;
            case channel_pairing::MULTI_CHANNEL_6:
                return 6;
        }
    }

    channelnum_t getNumChannelsInConfig(const std::vector<io_cfg_channel>& cfg) {
        channelnum_t val = std::accumulate(cfg.cbegin(), cfg.cend(), 0, [](auto cnt, auto& cfgEntry) -> channelnum_t {
            return cnt + getNumChannelsFromTrackType(cfgEntry.type);
        });
        return val;
    }
    //static_assert(getNumChannelsFromTrackType(tracktype::MULTI_CHANNEL_6) == 6);

    String getTrackNameShort(channel_pairing type, channelnum_t index, stage_bufferpoint isInput) {
        String s = StringFormat("%u", index);
        if (isInput == stage_bufferpoint::INPUT) {
            s += " IN";
        } else {
            s += " OUT";
        }
        switch (type) {
            default:
            case channel_pairing::MONO:
                s = "Mono " + s;
                break;
            case channel_pairing::STEREO:
                s = "St. " + s;
                break;
            case channel_pairing::MULTI_CHANNEL_4:
                s = "4CH " + s;
                break;
            case channel_pairing::MULTI_CHANNEL_6:
                s = "6CH " + s;
                break;
        }
        return s;
    }

    String getTrackName(channel_pairing type, channelnum_t index, bool isInput) {
        String s = StringFormat("%u", index);
        if (isInput) {
            s += " Input";
        } else {
            s += " Output";
        }
        switch (type) {
            default:
            case channel_pairing::MONO:
                s = "Mono " + s;
                break;
            case channel_pairing::STEREO:
                s = "Stereo " + s;
                break;
            case channel_pairing::MULTI_CHANNEL_4:
                s = "4 Channel " + s;
                break;
            case channel_pairing::MULTI_CHANNEL_6:
                s = "6 Channel " + s;
                break;
        }
        return s;
    }

    channel_pairing getNextTrackType(channel_pairing type) {
        switch (type) {
            default:
            case channel_pairing::MONO:
                return channel_pairing::STEREO;
            case channel_pairing::STEREO:
                return channel_pairing::MONO;
        //    case channel_pairing::MULTI_CHANNEL_4:
        //        return channel_pairing::MULTI_CHANNEL_6;
        //    case channel_pairing::MULTI_CHANNEL_6:
        //        return channel_pairing::MONO;
        }
    }

    String getTrackTypeStr(channel_pairing type) {
        switch (type) {
            default:
            case channel_pairing::MONO:
                return "MONO";
            case channel_pairing::STEREO:
                return "STEREO";
            case channel_pairing::MULTI_CHANNEL_4:
                return "4CH";
            case channel_pairing::MULTI_CHANNEL_6:
                return "6CH";
        }
    }
}// namespace DAW::AudioIO


/*
** This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/

class audiohost_callback {
    public:
    static int audioCallback(const void* inputBuffer, void* outputBuffer,
                            unsigned long framesPerBufferUlong,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void* userData) {
        if (!userData) {
            return paComplete;
        }
        samplecount_t callbackNumSamples = static_cast<samplecount_t>(framesPerBufferUlong);
        auto* stream = static_cast<audiohost::HostIOStream*>(userData);
        auto& timings = stream->audioCallbackInvocationDelay;
        stream->numInvocations++;
        audiohost* host = stream->host;
        float** inputs  = (float**) inputBuffer;
        float** outputs = (float**) outputBuffer;
        if (stream->streamShouldEnd) {
            for (channelnum_t i = 0; outputs && i < stream->nOutputChannels; i++) {
                if (outputs[i]) {
                    memset(outputs[i], 0, callbackNumSamples * sizeof(float));
                }
            }
            return paComplete;
        }


        dsp_util::fillChannels(outputs, stream->nOutputChannels, callbackNumSamples, 0.0f);
        if (!host) {
            return paAbort;
        }
        auto timeNow_i64 = getTimeMicros();
        if (0 != stream->lastAudioCallbackInvocationTime_i64) {
            auto tmDelta_i64 = timeNow_i64 - stream->lastAudioCallbackInvocationTime_i64;
            if (timings.tmDeltaCbAvg < 0) {
                timings.tmDeltaCbAvg = tmDelta_i64;
                timings.tmDeltaCbMin = tmDelta_i64;
                timings.tmDeltaCbMax = tmDelta_i64;
            } else {
                timings.tmDeltaCbAvg = (timings.tmDeltaCbAvg * 99 + tmDelta_i64) / 100;
                timings.tmDeltaCbMin = math::min(timings.tmDeltaCbMin, tmDelta_i64);
                timings.tmDeltaCbMax = math::max(timings.tmDeltaCbMax, tmDelta_i64);
            }
        }
        stream->lastAudioCallbackInvocationTime_i64 = timeNow_i64;
        //TODO: still a race condition on_terminate here
        AudioBuffer* block        = nullptr;
        channelnum_t numOutChannelsWritten = 0;
        if (stream->try_dequeue(block)) {
            dbgassert(block);
            auto meterOutput = stream->getMeterCallbackOutput();
            if (meterOutput) {
                meterOutput->update(block->output, 1.0f);
                meterOutput->onTick(block->output->samples / double(stream->getSampleRate()));
            }
            if (callbackNumSamples != block->output->samples) {
                log_lf(Log::L_ERROR, "audioCallback: framesPerBuffer != block->output->samples (%zu != %zu)\n", callbackNumSamples, block->output->samples);
            } else if (callbackNumSamples == block->output->samples) {
                auto channels = math::min<channelnum_t>(block->output->channels, stream->nOutputChannels);
                for (channelnum_t i = 0; outputs && i < channels; i++) {
                    float* channel = block->output->buf[i];
                    memcpy(outputs[i], channel, callbackNumSamples * sizeof(float));
                }
                numOutChannelsWritten = channels;
                timings.samplePosProcOut += block->output->samples;
            }
            // if (stream->firstInputTimeSeconds < 0.0) {
            //     stream->firstInputTimeSeconds = timeInfo->outputBufferDacTime;
            // }
            stream->outputTimeSeconds = block->time.inputTimeSeconds;
            block->inUse = false;
        } else {
            stream->bufferUnderuns++;
        }

        // fill channels that haven't been written to with zeroes
        for (channelnum_t i = numOutChannelsWritten; i < stream->nOutputChannels; i++) {
            memset(outputs[i], 0, callbackNumSamples * sizeof(float));
        }

        dsp_util::fillSaturate(outputs, stream->nOutputChannels, callbackNumSamples);

        auto& ringbuffer      = stream->getRingbuffer();
        auto& writePos        = ringbuffer.writePos;
        AudioBuffer** buffers = ringbuffer.buffers;

        AudioBuffer* bufferWrite = buffers[writePos];
        if (bufferWrite->inUse) {
            stream->inputBufferUnderuns++;
        } else {
            bufferWrite->output->realloc(callbackNumSamples);
            if (inputs) {
                channelnum_t nChannels = math::min<channelnum_t>(bufferWrite->output->channels, stream->nInputChannels);
                bufferWrite->output->copyFrom(inputs, callbackNumSamples, nChannels);
                for (channelnum_t i = nChannels; i < stream->nInputChannels; i++) {
                    memset(bufferWrite->output->buf[i], 0, bufferWrite->output->samples * sizeof(float));
                }
            } else {
                bufferWrite->output->clear();
            }
            auto meterInput = stream->getMeterCallbackInput();
            if (meterInput) {
                meterInput->update(bufferWrite->output, 1.0f);
                meterInput->onTick(bufferWrite->output->samples / double(stream->getSampleRate()));
            }
            bufferWrite->submitted      = true;
            bufferWrite->inUse          = true;
            bufferWrite->time = {};
            bufferWrite->time.inputTimeSeconds = timeInfo->inputBufferAdcTime;
            timings.samplePosProcIn += bufferWrite->output->samples;
            writePos++;
            writePos &= RING_BUF_MASK;
            if (stream) {
                stream->enqueueInput(bufferWrite);
            } else {
                bufferWrite->inUse = false;
            }
        }
        timings.samplePos += callbackNumSamples;


        return paContinue;
    }
};

/*
* This routine is called by portaudio when playback is done.
*/
static void StreamFinished(void* userData) {
    dbgassert(userData);
    auto* stream = static_cast<audiohost::HostIOStream*>(userData);
    stream->streamFinished         = true;
}

// using namespace DAW::AudioIO;
using DAW::channel_pairing;

audiohost::HostIOStream::HostIOStream(audiohost* const _host, int32_t _streamId, int32_t _streamIdx, DAW::AudioIO::io_cfg_tracks& cfg, channelnum_t _nOutputChannels, channelnum_t _nInputChannels)
    : metersInput(meterDataInput.data(), meterDataInput.size()),
      metersOutput(meterDataOutput.data(), meterDataOutput.size()),
      meterCallbackInput(!_nInputChannels ? nullptr : std::make_shared<DAW::rmsmeter>(meterDataCBInput.data(), math::min<channelnum_t>(_nInputChannels, 2))),
      meterCallbackOutput(!_nOutputChannels ? nullptr : std::make_shared<DAW::rmsmeter>(meterDataCBOutput.data(), math::min<channelnum_t>(_nOutputChannels, 2))),
      host(_host), streamId(_streamId), streamIdx(_streamIdx),
      nInputChannels(_nInputChannels),
      nOutputChannels(_nOutputChannels) {
    allocRingBuffer(ringbuffer, math::max<channelnum_t>(nInputChannels, 2));
    channelsInput.resize(cfg.input.size());
    channelsOutput.resize(cfg.output.size());
    for (auto& track : cfg.input) {
        dbgassert(track.idx < CtrSize(channelsInput));
        if (track.idx >= CtrSize(channelsInput)) {
            continue;
        }
        channelsInput[track.idx] = std::make_shared<HostIOStream::IOChannel>(track.idx,
                                                                     track.type,
                                                                     track.offset,
                                                                     metersInput.getSubChannelMeter(track.offset, DAW::AudioIO::getNumChannelsFromTrackType(track.type)));
    }
    for (auto& track : cfg.output) {
        dbgassert(track.idx < CtrSize(channelsOutput));
        if (track.idx >= CtrSize(channelsOutput)) {
            continue;
        }
        channelsOutput[track.idx] = std::make_shared<HostIOStream::IOChannel>(track.idx,
                                                                      track.type,
                                                                      track.offset,
                                                                      metersOutput.getSubChannelMeter(track.offset, DAW::AudioIO::getNumChannelsFromTrackType(track.type)));
    }
}

audiohost::HostIOStream::~HostIOStream() {
    freeRingBuffer(ringbuffer);
}

audiohost::HostIOStream* audiohost::getStream(size_t idx) {
    if (streams.size() > idx) {
        auto it = std::find_if(streams.begin(), streams.end(), [idx](std::shared_ptr<HostIOStream>& ptr) {
            return ptr->streamIdx == idx;
        });
        if (it != streams.end()) {
            return it->get();
        }
    }
    return nullptr;
}

std::shared_ptr<audiohost::HostIOStream> audiohost::getStreamSharedPtr(size_t idx) {
    if (streams.size() > idx) {
        auto it = std::find_if(streams.begin(), streams.end(), [idx](std::shared_ptr<HostIOStream>& ptr) {
            return ptr->streamIdx == idx;
        });
        if (it != streams.end()) {
            return *it;
        }
    }
    return nullptr;
}

bool audiohost::initPa() {
    if (!paIsInitalized) {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            Pa_Terminate();
            onError("Pa_Initialize", err);
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

void audiohost::removeStream(HostIOStream* stream) {
    stream->stream = nullptr;
    AudioBuffer* block = nullptr;
    while (stream->audioQueue.try_dequeue(block)) {
        block->inUse = false;
    }
    while (stream->audioQueueInput.try_dequeue(block)) {
        block->inUse = false;
    }
    streams.erase(std::remove_if(
                          begin(streams),
                          end(streams),
                          [stream](std::shared_ptr<HostIOStream>& x) { return x.get() == stream; }),
                  end(streams));
}

void audiohost::HostIOStream::enqueueInput(AudioBuffer* buf) {
    if (streamFinished) {
        return;
    }
    if (firstInputTimeSeconds < 0) {
        firstInputTimeSeconds = buf->time.inputTimeSeconds;
    }
    buf->time.inputTimeSeconds = buf->time.inputTimeSeconds - firstInputTimeSeconds;
    inputTimeSeconds = buf->time.inputTimeSeconds;
    AudioBlock* blockIn = buf->output;
    metersInput.update(blockIn, 1.0f);
    metersInput.onTick(blockIn->samples / (double) this->host->lSampleRate);
    //TODO: skip this copy step if guictr_input_meters is not visible
    for (auto & nTrack : channelsInput) {
        auto* track = nTrack.get();
        //if (track->buf.channels != buf->output->channels) {
        //    log_printf("mismatch! tracksInput.size %d, track.channels %d, input.channels %d\n", tracksInput.size(), track->buf.channels, buf->output->channels);
        //}
        track->buf.realloc(blockIn->samples);
        track->buf.copyFrom(blockIn, [offset = track->channelOffset](uint32_t dstIdx, uint32_t srcIdx) {
            return offset + dstIdx;
        });
    }
    buf->time.samplePosInput = inputSamplePos;
    this->audioQueueInput.enqueue(buf);
    inputSamplePos += blockIn->samples;
}

bool audiohost::HostIOStream::try_dequeueInput(AudioBuffer*& buf) {
    if (streamFinished) {
        return false;
    }
    return this->audioQueueInput.try_dequeue(buf);
}

void audiohost::HostIOStream::enqueue(AudioBuffer* buf) {
    if (streamFinished) {
        return;
    }
    AudioBlock* blockIn = buf->output;
    metersOutput.update(blockIn, 1.0f);
    metersOutput.onTick(blockIn->samples / (double) this->host->lSampleRate);
    for (auto & nTrack : channelsOutput) {
        auto* track = nTrack.get();
        dbgassert(track->buf.channels <= buf->output->channels);
        track->buf.realloc(blockIn->samples);
        track->buf.copyFrom(blockIn, [offset = track->channelOffset](uint32_t dstIdx, uint32_t srcIdx) {
            return offset + dstIdx;
        });
    }
    this->audioQueue.enqueue(buf);
}

bool audiohost::HostIOStream::try_dequeue(AudioBuffer*& buf) {
    if (streamFinished) {
        return false;
    }
    auto success = this->audioQueue.try_dequeue(buf);
    if (success) {
        buf->time.samplePosOutput = outputSamplePos;
        outputSamplePos += buf->output->samples;
    //     if (firstOutputSampleTime < 0) {
    //         firstOutputSampleTime = buf->blockPosSample;
    //     }
    //     this->samplePosOutput = buf->blockPosSample - firstOutputSampleTime;
    }
    return success;
}

bool audiohost::startAudio(app_iosettings& iosettings) {
    lastErrorMessage.clear();
    if (!initPa()) {
        lastErrorMessage = "Failed to initialize PortAudio";
        return false;
    }
    stopAudio();

    int apiCount               = Pa_GetHostApiCount();
    auto samplerate            = iosettings.samplerate;
    auto blocksize             = iosettings.blocksize;
    const char* selApiNameCStr = StringAsCStr(iosettings.device_api);
    auto cfg                   = iosettings.getConfig(iosettings.device_api);
    auto asioConfig            = iosettings.asioConfig;
    auto& channelConfig        = iosettings.getChannelConfig(iosettings.device_api);
    std::vector<std::shared_ptr<HostIOStream>> vecStreams;

    int32_t deviceApiIdxSelected    = paNoDevice;
    int32_t deviceIdxSelectedInput  = paNoDevice;
    int32_t deviceIdxSelectedOutput = paNoDevice;
    const PaHostApiInfo* apiSelectedInfo = nullptr;
    for (int i = 0; i < apiCount; i++) {
        const PaHostApiInfo* info = Pa_GetHostApiInfo(i);
        if (info) {
            const char* pref = "[ ] ";
            if (!strcmp(selApiNameCStr, info->name)) {
                apiSelectedInfo = info;
                deviceApiIdxSelected = i;
                pref = "[x] ";
            }
            log_printf("%sAPI[%d] = %s %d devices\n", pref, i, info->name, info->deviceCount);
        }
    }
    if (apiSelectedInfo) {
        int deviceCount = Pa_GetDeviceCount();
        for (int i = 0; i < deviceCount; i++) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (!info) {
                onError("!info for Pa_GetDeviceInfo", i);
                continue;
            }
            if (info->hostApi == deviceApiIdxSelected) {
                bool bAsioMatches   = apiSelectedInfo->type == PaHostApiTypeId::paASIO && asioConfig.deviceName == info->name;
                bool bOutputMatches = bAsioMatches || (apiSelectedInfo->type != PaHostApiTypeId::paASIO && cfg.deviceNameOutput == info->name);
                bool bInputMatches  = bAsioMatches || (apiSelectedInfo->type != PaHostApiTypeId::paASIO && cfg.deviceNameInput == info->name);
                bInputMatches |= cfg.deviceNameInput == "default" && apiSelectedInfo->defaultInputDevice == i;
                bOutputMatches |= cfg.deviceNameOutput == "default" && apiSelectedInfo->defaultOutputDevice == i;
#ifdef _WIN32
                if (apiSelectedInfo->type == PaHostApiTypeId::paWASAPI && cfg.deviceNameInput == "loopback" && PaWasapi_IsLoopback(i) && deviceIdxSelectedInput == paNoDevice) {
                    bInputMatches = true;
                    log_lf(Log::L_DEBUG, "Using WASAPI Loopback Device as Input\n");
                }
#endif
                if (bOutputMatches && info->maxOutputChannels > 0) {
                    if (samplerate == 0 && info->defaultSampleRate > 0) {
                        samplerate = static_cast<samplerate_t>(info->defaultSampleRate);
                    }
                    deviceIdxSelectedOutput = i;
                    log_printf("deviceIdxSelectedOutput DEVICE[%d] = %s %d IN/%d OUT channels at samplerate %d\n", i, info->name, info->maxInputChannels, info->maxOutputChannels, samplerate);
                }
                if (bInputMatches && info->maxInputChannels > 0) {
                    if (samplerate == 0 && info->defaultSampleRate > 0) {
                        samplerate = static_cast<samplerate_t>(info->defaultSampleRate);
                    }
                    deviceIdxSelectedInput = i;
                    log_printf("deviceIdxSelectedInput DEVICE[%d] = %s %d IN/%d OUT channels at samplerate %d\n", i, info->name, info->maxInputChannels, info->maxOutputChannels, samplerate);
                }
            }
        }
    }

    if (deviceIdxSelectedOutput == paNoDevice && deviceIdxSelectedInput == paNoDevice) {
        return onError("No input or output device", paBadIODeviceCombination);
    }

    const PaDeviceInfo* devInfo      = deviceIdxSelectedOutput == paNoDevice ? nullptr : Pa_GetDeviceInfo(deviceIdxSelectedOutput);
    const PaDeviceInfo* devInfoInput = deviceIdxSelectedInput == paNoDevice ? nullptr : Pa_GetDeviceInfo(deviceIdxSelectedInput);

    PaStreamParameters* pOutputParams = nullptr;
    PaStreamParameters outputParams;
    if (deviceIdxSelectedOutput != paNoDevice) {
        pOutputParams                 = &outputParams;
        outputParams.device           = deviceIdxSelectedOutput;
        outputParams.channelCount     = devInfo->maxOutputChannels;   /* use all channels */
        outputParams.sampleFormat     = paFloat32 | paNonInterleaved; /* 32 bit floating point output */
        outputParams.suggestedLatency = devInfo->defaultLowOutputLatency;

        outputParams.hostApiSpecificStreamInfo = nullptr;
    } else {
        outputParams.channelCount = 0;
    }
    log_printf("Open stream on device %s | %s\n", apiSelectedInfo->name, devInfo ? devInfo->name : devInfoInput->name);
    PaStreamParameters* pInputParams = nullptr;
    PaStreamParameters inputParams;
    if (devInfoInput) {
        pInputParams                 = &inputParams;
        inputParams.device           = deviceIdxSelectedInput;
        inputParams.channelCount     = devInfoInput->maxInputChannels; /* use all channels */
        inputParams.sampleFormat     = paFloat32 | paNonInterleaved;   /* 32 bit floating point output */
        inputParams.suggestedLatency = devInfoInput->defaultLowInputLatency;

        inputParams.hostApiSpecificStreamInfo = nullptr;
    } else {
        inputParams.channelCount = 0;
    }
    inputParams.channelCount = math::clamp<int>(inputParams.channelCount, 0, MAX_AUDIO_IO_CHANNELS);
    outputParams.channelCount = math::clamp<int>(outputParams.channelCount, 0, MAX_AUDIO_IO_CHANNELS);
    log_printf("With %d output channels\n", outputParams.channelCount);
    log_printf("With %d input channels\n", inputParams.channelCount);

    int32_t streamId = ++nextStreamId;
    using DAW::AudioIO::io_cfg_tracks;
    using DAW::AudioIO::io_cfg_channel;
    io_cfg_tracks chCfg;

    if (channelConfig.isInit &&
        getNumChannelsInConfig(channelConfig.input) == inputParams.channelCount &&
        getNumChannelsInConfig(channelConfig.output) == outputParams.channelCount) {
        chCfg = channelConfig;
    } else {
        channelnum_t chIdx = 0;
        for (channelnum_t i = 0; i < outputParams.channelCount;) {
            io_cfg_channel channels;
            channels.idx = chIdx++;
            if (i + 1 < outputParams.channelCount) {
                channels.type = channel_pairing::STEREO;
            } else {
                channels.type = channel_pairing::MONO;
            }
            channels.name = DAW::AudioIO::getTrackName(channels.type, channels.idx, false);
            channels.offset = i;
            i += DAW::AudioIO::getNumChannelsFromTrackType(channels.type);
            chCfg.output.push_back(channels);
        }
        chIdx = 0;
        for (channelnum_t i = 0; devInfoInput && i < inputParams.channelCount;) {
            io_cfg_channel channels;
            channels.idx = chIdx++;
            if (i + 1 < outputParams.channelCount) {
                channels.type = channel_pairing::STEREO;
            } else {
                channels.type = channel_pairing::MONO;
            }
            channels.name = DAW::AudioIO::getTrackName(channels.type, channels.idx, true);
            channels.offset = i;
            i += DAW::AudioIO::getNumChannelsFromTrackType(channels.type);
            chCfg.input.push_back(channels);
        }
        chCfg.isInit  = true;
        channelConfig = chCfg;
    }

    const auto streamIdx = vecStreams.size();
    auto stream  = std::make_shared<HostIOStream>(this,
                                                 streamId,
                                                 streamIdx,
                                                 chCfg,
                                                 static_cast<channelnum_t>(outputParams.channelCount),
                                                 static_cast<channelnum_t>(inputParams.channelCount));
    stream->outputName = devInfo ? devInfo->name : devInfoInput->name;
    stream->device_api = selApiNameCStr;
    if (devInfoInput) {
        stream->inputName = devInfoInput->name;
    }

    PaStream* paStream = nullptr;

    PaError err = Pa_OpenStream(
            &paStream,
            pInputParams,
            pOutputParams,
            (double) samplerate,
            blocksize,
            paClipOff | paDitherOff, /* Portaudio internal clipping is disabled */
            audiohost_callback::audioCallback,
            stream.get());

    if (err != paNoError) {
        return onError("Pa_OpenStream", err);
    }
    auto info = Pa_GetStreamInfo(paStream);
    if (info->sampleRate > 0)
        samplerate = static_cast<samplerate_t>(info->sampleRate);
    err = Pa_SetStreamFinishedCallback(paStream, &StreamFinished);
    if (err != paNoError)
        return onError("Pa_SetStreamFinishedCallback", err);

    stream->stream = paStream;
    this->streams.push_back(stream);
    this->lSampleRate = samplerate;
    this->lBlockSize  = blocksize;

    err = Pa_StartStream(paStream);
    if (err != paNoError)
        return onError("Pa_StartStream", err);

    return true;
}
bool audiohost::stopAudio() {
    int numStreamsStopped = 0;

    std::vector<std::shared_ptr<HostIOStream>> streamsCopy = streams;
    for (auto& sharedPtrStream : streamsCopy) {
        sharedPtrStream->streamShouldEnd = true;
    }
    auto now = getTimeMillis();
    while (getTimeMillis() - now < 2000) {
        bool anyRunning = false;
        for (auto& sharedPtrStream : streamsCopy) {
            anyRunning |= !sharedPtrStream->streamFinished;
        }
        if (!anyRunning) {
            break;
        }
        seqthreads::threadSleep(100);
    }
    for (auto& sharedPtrStream : streamsCopy) {
        dbgassert(sharedPtrStream->stream);
        PaError err = Pa_StopStream(sharedPtrStream->stream);
        if (err != paNoError) {
            onError("Pa_StopStream", err);
        }
        numStreamsStopped++;
    }
    now = getTimeMillis();
    while (getTimeMillis() - now < 2000) {
        bool anyRunning = false;
        for (auto& sharedPtrStream : streamsCopy) {
            anyRunning |= !sharedPtrStream->streamFinished;
        }
        if (!anyRunning) {
            break;
        }
        seqthreads::threadSleep(100);
    }
    for (auto& sharedPtrStream : streamsCopy) {
        PaError err = Pa_CloseStream(sharedPtrStream->stream);
        if (err != paNoError) {
            onError("Pa_CloseStream", err);
        }
        removeStream(sharedPtrStream.get());
    }
    dbgassert(streams.empty());
    return numStreamsStopped > 0;
}
bool audiohost::onError(const char* msg, int err) {
    lastErrorMessage = StringFormat("%s (%d): %s", msg, err, Pa_GetErrorText(err));
    log_lf(Log::L_ERROR, "%s\n", lastErrorMessage.c_str());
    return false;
}
