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
#ifdef _WIN32
#include <pa_win_wasapi.h>
#endif
#include <vector>
#include <memory>
#include <numeric>
#include <functional>


namespace AudioIO {
    const std::array<uint32_t, 4> IntSamplerates = {
        44100, 48000, 96000, 192000
    };
    const std::array<uint32_t, 4> ExtSamplerates = {
        44100, 48000, 96000, 192000
    };
}// namespace AudioIO



bool error(const char* msg, PaError err) {
    log_lf(Log::L_ERROR, "%s (%d): %s", msg, err, Pa_GetErrorText(err));
    return false;
}

/*
** This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/

static int audioCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void* userData) {
    if (!userData) {
        return paComplete;
    }
    auto* stream = static_cast<audiohost::HostIOStream*>(userData);
    if (stream->streamShouldEnd) {
        return paComplete;
    }
    audiohost* host = stream->host;
    float** inputs  = (float**) inputBuffer;
    float** outputs = (float**) outputBuffer;

    dsp_util::fillChannels(outputs, stream->nOutputChannels, framesPerBuffer, 0.0f);
    if (!host) {
        return paAbort;
    }
    auto timeNow_i64 = getTimeMicros();
    if (0 != stream->lastAudioCallbackInvocationTime_i64) {
        host->audioCallbackInvocationDelay_usec = timeNow_i64 - stream->lastAudioCallbackInvocationTime_i64;
    }
    stream->lastAudioCallbackInvocationTime_i64 = timeNow_i64;
    //TODO: still a race condition on_terminate here
    AudioBuffer* block        = nullptr;
    int numOutChannelsWritten = 0;
    if (stream->try_dequeue(block)) {
        dbgassert(block);
        if (framesPerBuffer == block->output->samples) {
            int32_t channels = math::min<int32_t>(block->output->channels, stream->nOutputChannels);
            for (int32_t i = 0; i < channels; i++) {
                float* channel = block->output->buf[i];
                memcpy(outputs[i], channel, framesPerBuffer * sizeof(float));
            }
            numOutChannelsWritten = channels;
        }
        block->inUse = false;
    } else {
        host->bufferUnderuns++;
    }

    // fill channels that haven't been written to with zeroes
    for (int32_t i = numOutChannelsWritten; i < stream->nOutputChannels; i++) {
        memset(outputs[i], 0, framesPerBuffer * sizeof(float));
    }

    dsp_util::fillSaturate(outputs, stream->nOutputChannels, framesPerBuffer);
    host->blockReads++;

    audiothread_ringbuffer_t& ringbuffer = stream->getRingbuffer();
    int32_t& writePos                    = ringbuffer.writePos;
    AudioBuffer** buffers                = ringbuffer.buffers;

    AudioBuffer* bufferWrite = buffers[writePos];
    if (bufferWrite->inUse) {
        host->inputBufferUnderuns++;
    } else {
        bufferWrite->output->realloc(framesPerBuffer);
        if (inputs) {
            int32_t nChannels = math::min<int32_t>(bufferWrite->output->channels, stream->nInputChannels);
            bufferWrite->output->copyFrom(inputs, framesPerBuffer, nChannels);
            for (int32_t i = nChannels; i < stream->nInputChannels; i++) {
                memset(bufferWrite->output->buf[i], 0, bufferWrite->output->samples * sizeof(float));
            }
        } else {
            bufferWrite->output->clear();
        }
        bufferWrite->submitted      = true;
        bufferWrite->inUse          = true;
        bufferWrite->blockPosSample = timeInfo->inputBufferAdcTime;
        bufferWrite->blockPosTick   = 0;
        writePos++;
        writePos &= RING_BUF_MASK;
        if (stream) {
            stream->enqueueInput(bufferWrite);
        } else {
            bufferWrite->inUse = false;
        }
    }


    return paContinue;
}
/*
* This routine is called by portaudio when playback is done.
*/
static void StreamFinished(void* userData) {
    dbgassert(userData);
    auto* stream = static_cast<audiohost::HostIOStream*>(userData);
    stream->streamFinished         = true;
}

audiohost::HostIOStream::HostIOStream(int32_t _nStreamId, AudioIO::io_cfg_tracks cfg, int32_t _nOutputChannels, int32_t _nInputChannels)
    : streamId(_nStreamId),
      nInputChannels(_nInputChannels),
      nOutputChannels(_nOutputChannels),
      metersInput(meterDataInput.data(), meterDataInput.size()),
      metersOutput(meterDataOutput.data(), meterDataOutput.size())
{
    using AudioIO::io_cfg_channel;
    allocRingBuffer(ringbuffer, nInputChannels);
    channelsInput.resize(cfg.input.size());
    channelsOutput.resize(cfg.output.size());
    int32_t channelOffset = 0;
    for (int32_t i = 0; i < cfg.input.size(); i++) {
        io_cfg_channel& track = cfg.input[i];
        channelsInput[i] = std::make_shared<HostIOStream::IOChannel>(i, track.type, channelOffset, metersInput.getSubChannelMeter(channelOffset, AudioIO::getNumChannelsFromTrackType(track.type)));
        channelOffset += getNumChannelsFromTrackType(track.type);
    }
    channelOffset = 0;
    for (int32_t i = 0; i < cfg.output.size(); i++) {
        io_cfg_channel& track = cfg.output[i];
        channelsOutput[i] = std::make_shared<HostIOStream::IOChannel>(i, track.type, channelOffset, metersOutput.getSubChannelMeter(channelOffset, AudioIO::getNumChannelsFromTrackType(track.type)));
        channelOffset += getNumChannelsFromTrackType(track.type);
    }
}

audiohost::HostIOStream::~HostIOStream() {
    freeRingBuffer(ringbuffer);
}

audiohost::HostIOStream* audiohost::getStream(int idx) {
    if (streams.size() > idx) {
        auto it = std::find_if(streams.begin(), streams.end(), [idx](std::shared_ptr<HostIOStream>& ptr) {
            return ptr->idx == idx;
        });
        if (it != streams.end()) {
            return it->get();
        }
    }
    return nullptr;
}
std::shared_ptr<audiohost::HostIOStream> audiohost::getStreamSharedPtr(int idx) {
    if (streams.size() > idx) {
        auto it = std::find_if(streams.begin(), streams.end(), [idx](std::shared_ptr<HostIOStream>& ptr) {
            return ptr->idx == idx;
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
    AudioBlock* blockIn = buf->output;
    metersInput.update(blockIn, 1.0f);
    metersInput.onTick(blockIn->samples / (double) this->host->lSampleRate);
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
    this->audioQueueInput.enqueue(buf);
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
        assert(track->buf.channels <= buf->output->channels);
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
    return this->audioQueue.try_dequeue(buf);
}

bool audiohost::startAudio(app_iosettings& iosettings) {
    using AudioIO::io_cfg_tracks;
    if (!initPa())
        return false;
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
    PaHostApiTypeId hostApiType = PaHostApiTypeId::paInDevelopment;
    int apiIdxASIO                  = -1;
    for (int i = 0; i < apiCount; i++) {
        const PaHostApiInfo* info = Pa_GetHostApiInfo(i);
        if (info) {
            if (info->type == PaHostApiTypeId::paASIO) {
                apiIdxASIO = i;
            }
            const char* pref = "[ ] ";
            if (!strcmp(selApiNameCStr, info->name)) {
                deviceApiIdxSelected = i;
                hostApiType = info->type;

                pref = "[x] ";
            }
            log_printf("%sAPI[%d] = %s %d devices\n", pref, i, info->name, info->deviceCount);
        }
    }
    if (deviceApiIdxSelected >= 0) {
        int deviceCount = Pa_GetDeviceCount();
        for (int i = 0; i < deviceCount; i++) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (!info) {
                error("!info for Pa_GetDeviceInfo", i);
                continue;
            }
            if (info->hostApi == deviceApiIdxSelected) {
                bool bAsioMatches   = apiIdxASIO == info->hostApi && asioConfig.deviceName == info->name;
                bool bOutputMatches = bAsioMatches || (apiIdxASIO != info->hostApi && cfg.deviceNameOutput == info->name);
                bool bInputMatches  = bAsioMatches || (apiIdxASIO != info->hostApi && cfg.deviceNameInput == info->name);
                if (bOutputMatches && info->maxOutputChannels > 0) {
                    deviceIdxSelectedOutput = i;
                    log_printf("deviceIdxSelectedOutput DEVICE[%d] = %s %d IN/%d OUT channels\n", i, info->name, info->maxInputChannels, info->maxOutputChannels);
                }
                if (bInputMatches && info->maxInputChannels > 0) {
                    deviceIdxSelectedInput = i;
                    log_printf("deviceIdxSelectedInput DEVICE[%d] = %s %d IN/%d OUT channels\n", i, info->name, info->maxInputChannels, info->maxOutputChannels);
                }
            }
        }
    }

    if (deviceIdxSelectedOutput == paNoDevice && deviceIdxSelectedInput == paNoDevice) {
        log_lf(Log::L_ERROR, "Error: No input or output device");
        return false;
    }

#ifdef _WIN32
    if (hostApiType == PaHostApiTypeId::paWASAPI && PaWasapi_IsLoopback(deviceIdxSelectedInput)) {
        log_lf(Log::L_DEBUG, "Using WASAPI Loopback Device as Input");
    }
#endif


    const PaDeviceInfo* devInfo      = deviceIdxSelectedOutput == paNoDevice ? nullptr : Pa_GetDeviceInfo(deviceIdxSelectedOutput);
    const PaDeviceInfo* devInfoInput = deviceIdxSelectedInput == paNoDevice ? nullptr : Pa_GetDeviceInfo(deviceIdxSelectedInput);
    const PaHostApiInfo* apiInfo     = Pa_GetHostApiInfo(deviceApiIdxSelected);

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
    log_printf("Open stream on device %s | %s\n", apiInfo->name, devInfo ? devInfo->name : devInfoInput->name);
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

    log_printf("With %d output channels\n", outputParams.channelCount);
    log_printf("With %d input channels\n", inputParams.channelCount);

    int32_t streamId = ++nextStreamId;
    io_cfg_tracks chCfg;

    if (channelConfig.isInit &&
        getNumChannelsInConfig(channelConfig.input) == inputParams.channelCount &&
        getNumChannelsInConfig(channelConfig.output) == outputParams.channelCount) {
        chCfg = channelConfig;
    } else {
        using AudioIO::io_cfg_channel;
        using AudioIO::tracktype;
        int32_t chIdx = 0;
        for (int i = 0; i < outputParams.channelCount;) {
            io_cfg_channel channels;
            channels.idx = chIdx++;
            if (i + 1 < outputParams.channelCount) {
                channels.type = tracktype::STEREO;
            } else {
                channels.type = tracktype::MONO;
            }
            channels.name = AudioIO::getTrackName(channels.type, channels.idx, false);
            channels.channelOffset = i;
            i += getNumChannelsFromTrackType(channels.type);
            chCfg.output.push_back(channels);
        }
        chIdx = 0;
        for (int i = 0; devInfoInput && i < inputParams.channelCount;) {
            io_cfg_channel channels;
            channels.idx = chIdx++;
            if (i + 1 < outputParams.channelCount) {
                channels.type = tracktype::STEREO;
            } else {
                channels.type = tracktype::MONO;
            }
            channels.name = AudioIO::getTrackName(channels.type, channels.idx, true);
            channels.channelOffset = i;
            i += getNumChannelsFromTrackType(channels.type);
            chCfg.input.push_back(channels);
        }
        chCfg.isInit  = true;
        channelConfig = chCfg;
    }
    auto stream  = std::make_shared<HostIOStream>(streamId, chCfg, outputParams.channelCount, inputParams.channelCount);
    stream->idx  = vecStreams.size();
    stream->host = this;

    stream->outputName      = devInfo ? devInfo->name : devInfoInput->name;
    stream->nOutputChannels = outputParams.channelCount;
    stream->device_api      = selApiNameCStr;
    if (devInfoInput) {
        stream->nInputChannels = inputParams.channelCount;
        stream->inputName      = devInfoInput->name;
    }

    PaStream* paStream = nullptr;

    PaError err = Pa_OpenStream(
            &paStream,
            pInputParams,
            pOutputParams,
            (double) samplerate,
            blocksize,
            paClipOff, /* Portaudio internal clipping is disabled */
            audioCallback,
            stream.get());

    if (err != paNoError) {
        return error("Pa_OpenStream", err);
    }

    err = Pa_SetStreamFinishedCallback(paStream, &StreamFinished);
    if (err != paNoError)
        return error("Pa_SetStreamFinishedCallback", err);

    stream->stream = paStream;
    this->streams.push_back(stream);
    this->lSampleRate = samplerate;
    this->lBlockSize  = blocksize;

    err = Pa_StartStream(paStream);
    if (err != paNoError)
        return error("Pa_StartStream", err);

    return true;
}
bool audiohost::stopAudio() {
    int numStreamsStopped = 0;

    std::vector<std::shared_ptr<HostIOStream>> streamsCopy = streams;
    for (auto& sharedPtrStream : streamsCopy) {
        sharedPtrStream->streamShouldEnd = true;
    }
    for (auto& sharedPtrStream : streamsCopy) {
        assert(sharedPtrStream->stream);
        PaError err = Pa_StopStream(sharedPtrStream->stream);
        if (err != paNoError) {
            error("Pa_StopStream", err);
        }
        numStreamsStopped++;
    }
    for (auto& sharedPtrStream : streamsCopy) {
        PaError err = Pa_CloseStream(sharedPtrStream->stream);
        if (err != paNoError) {
            error("Pa_CloseStream", err);
        }
        removeStream(sharedPtrStream.get());
    }
    assert(streams.empty());
    return numStreamsStopped > 0;
}

namespace AudioIO {
    tracktype getTrackTypeFromNumChannels(int32_t t) {
        if (t < 2)
            return MONO;

        if (t < 3)
            return STEREO;

        if (t < 5)
            return MULTI_CHANNEL_4;

        return MULTI_CHANNEL_6;
    }

    int32_t getNumChannelsFromTrackType(tracktype t) {
        switch (t) {
            default:
            case MONO:
                return 1;
            case STEREO:
                return 2;
            case MULTI_CHANNEL_4:
                return 4;
            case MULTI_CHANNEL_6:
                return 6;
        }
    }

    int32_t getNumChannelsInConfig(const std::vector<io_cfg_channel>& cfg) {
        int32_t val = std::accumulate(cfg.cbegin(), cfg.cend(), 0, [](int cnt, auto& cfgEntry) {
            return cnt + getNumChannelsFromTrackType(cfgEntry.type);
        });
        return val;
    }
    //static_assert(getNumChannelsFromTrackType(AudioIO::tracktype::MULTI_CHANNEL_6) == 6);

    String getTrackNameShort(AudioIO::tracktype type, int32_t index, stagebuffer_point isInput) {
        String s = StringFormat("%d", index);
        if (isInput == stagebuffer_point::INPUT) {
            s += " IN";
        } else {
            s += " OUT";
        }
        switch (type) {
            default:
            case AudioIO::MONO:
                s = "Mono " + s;
                break;
            case AudioIO::STEREO:
                s = "St. " + s;
                break;
            case AudioIO::MULTI_CHANNEL_4:
                s = "4CH " + s;
                break;
            case AudioIO::MULTI_CHANNEL_6:
                s = "6CH " + s;
                break;
        }
        return s;
    }

    String getTrackName(AudioIO::tracktype type, int32_t index, bool isInput) {
        String s = StringFormat("%d", index);
        if (isInput) {
            s += " Input";
        } else {
            s += " Output";
        }
        switch (type) {
            default:
            case AudioIO::MONO:
                s = "Mono " + s;
                break;
            case AudioIO::STEREO:
                s = "Stereo " + s;
                break;
            case AudioIO::MULTI_CHANNEL_4:
                s = "4 Channel " + s;
                break;
            case AudioIO::MULTI_CHANNEL_6:
                s = "6 Channel " + s;
                break;
        }
        return s;
    }

    tracktype getNextTrackType(tracktype type) {
        switch (type) {
            default:
            case MONO:
                return STEREO;
            case STEREO:
                return MONO;
        //    case MULTI_CHANNEL_4:
        //        return MULTI_CHANNEL_6;
        //    case MULTI_CHANNEL_6:
        //        return MONO;
        }
    }

    String getTrackTypeStr(tracktype type) {
        switch (type) {
            default:
            case MONO:
                return "MONO";
            case STEREO:
                return "STEREO";
            case MULTI_CHANNEL_4:
                return "4CH";
            case MULTI_CHANNEL_6:
                return "6CH";
        }
    }
}
