#include "appsettings.h"
#include "assert_dbg.h"
#include "types.h"
#include "audiotrack.h"
#include "track_impl.h"
#include "audiosample.h"
#include "audioblock.h"
#include "logging.h"
#include "host/vst_host.h"
#include "mainctrl.h"
#include <cstring>
#include <memory>
#include <vector>

static constexpr size_t PER_BLOCK_BYTES          = (1024ULL * 512ULL);
static constexpr samplecount_t PER_BLOCK_SAMPLES = (PER_BLOCK_BYTES / (sizeof(float)));

/*static*/ samplecount_t audiotrack_t::GetSplitSampleLength() {
    return PER_BLOCK_SAMPLES;
}
std::shared_ptr<audiotrack_split_t> audiotrack_t::getSampleById(int64_t sampleId) {
    //TODO: this lock could be narrowed
    ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    for (const std::shared_ptr<audiotrack_split_t>& sample : samples) {
        if (sample && sample->sampleId == sampleId)
            return sample;
    }
    return nullptr;
}
std::shared_ptr<audiotrack_split_t> audiotrack_t::getSample(samplecount_t samplePos) {
    //TODO: this lock could be narrowed
    ThreadLock lock    = MainCtrl::getPlayThread()->lockThread();
    auto startBlock = static_cast<size_t>((samplePos) / PER_BLOCK_SAMPLES);
    if (samples.size() > startBlock && samples[startBlock]) {
        return samples[startBlock];
    }
    return nullptr;
}
int64_t audiotrack_t::readSamples(samplecount_t samplePos, samplecount_t numSamples, channelnum_t numChannels, std::vector<samplechannel_t>& outChannels) {
    dbgassert(numChannels == 2);
    const samplecount_t SPLIT_SAMPLECOUNT = audiotrack_t::GetSplitSampleLength();
    const samplecount_t samplePosEnd = samplePos + numSamples;

    std::vector<audiotrack_split_t*> samples;
    visitSamples_NoLock([&samples, SPLIT_SAMPLECOUNT, samplePos, samplePosEnd](std::shared_ptr<audiotrack_split_t>& split) {
        auto* ptrSplit = split.get();
        if (ptrSplit && ptrSplit->samplePos + SPLIT_SAMPLECOUNT >= samplePos && ptrSplit->samplePos < samplePosEnd) {
            samples.push_back(ptrSplit);
        }
    });

    if (samples.empty()) {
        //unexpected
        dbgassert(0);
        return 0;
    }

    std::sort(samples.begin(), samples.end(), [](audiotrack_split_t* lhs, audiotrack_split_t* rhs) {
        return lhs->samplePos < rhs->samplePos;
    });


    outChannels.resize(numChannels);
    for (auto& channel : outChannels) {
        channel.resize(numSamples);
        memset(channel.data(), 0, numSamples * sizeof(float));
    }
    samplecount_t samplesWritten = 0;
    for (audiotrack_split_t* split : samples) {
        auto* sample = split->getSample();
        dbgassert(split->samplePos + SPLIT_SAMPLECOUNT >= samplePos && split->samplePos < samplePosEnd);

        const size_t readBeginOffset = math::clamp<samplecount_t>(samplePos - split->samplePos, 0, SPLIT_SAMPLECOUNT);
        const size_t readEndOffset = math::clamp<samplecount_t>(samplePosEnd - split->samplePos, 0, SPLIT_SAMPLECOUNT);
        const size_t readLen = math::clamp<samplecount_t>(readEndOffset - readBeginOffset, 0, SPLIT_SAMPLECOUNT);

        dbgassert(sample->nChannels == numChannels);
        dbgassert(sample->nChannels == sample->samples.size());
        dbgassert(sample->nSamples == SPLIT_SAMPLECOUNT);
        dbgassert(sample->nSamples == static_cast<samplecount_t>(sample->samples[0].size()));
        dbgassert(sample->nSamples == static_cast<samplecount_t>(sample->samples[1].size()));

        if (sample->samples.size() >= 2) {
            memcpy(outChannels[0].data() + samplesWritten, sample->samples[0].data() + readBeginOffset, readLen * sizeof(float));
            memcpy(outChannels[1].data() + samplesWritten, sample->samples[1].data() + readBeginOffset, readLen * sizeof(float));
            samplesWritten += readLen;
        }
    }
    return samplesWritten;
}
samplecount_t audiotrack_t::convertToSamples(DAW::pluginhost* host) {
    auto isInSync = [this]() -> bool {
        if (data.size() != this->samples.size()) {
            return false;
        }
        for (size_t i = 0; i < data.size(); i++) {
            if (data[i]) {
                if (!this->samples[i] || this->samples[i]->version != data[i]->version) {
                    return false;
                }
            }
        }
        return true;
    };
    if (isInSync())
        return 0;

    samplecount_t samplesCopied = 0;
    for (size_t i = 0; i < data.size(); i++) {
        samplecount_t samplePos = i * PER_BLOCK_SAMPLES;
        if (data[i]) {
            while (samples.size() <= i) {
                samples.push_back(nullptr);
            }
            auto& block = data[i]->data;
            if (!this->samples[i]) {
                auto split = std::make_shared<audiotrack_split_t>();
                split->samplePos            = samplePos;
                split->sample.sampleRate    = host->m_sampleFormatInternal.sampleRate;
                split->sampleId             = host->getNextSampleId(0);
                split->sample.bitsPerSample = 32;
                this->samples[i] = split;
            }
            audiotrack_split_t* split = this->samples[i].get();
            split->sample.nChannels = block.channels;
            split->sample.nSamples  = block.samples;
            if (split->version == data[i]->version) {
                continue;
            }
            //bool resized   = false;
            //int preVersion = split->version;
            split->version = data[i]->version;

            std::vector<samplechannel_t>& channels = split->sample.samples;
            if (block.channels != channels.size()) {
                channels.resize(block.channels);
                //resized = true;
            }
            for (channelnum_t j = 0; j < block.channels; j++) {
                float* srcPtr  = block.buf[j];
                size_t srcSize = block.samples;
                channels[j].resize(srcSize);
                float* dstPtr  = channels[j].data();
                size_t dstSize = channels[j].size();
#ifndef _WIN32
                memcpy(dstPtr, srcPtr, math::min(dstSize * sizeof(float), srcSize * sizeof(float)));
#else
                memcpy_s(dstPtr, dstSize * sizeof(float), srcPtr, srcSize * sizeof(float));
#endif
                samplesCopied += srcSize;
            }
            //log_lf(Log::L_DEBUG, "block #%d copy %d bytes, present %d, resized %d, version %d/%d\n", i, bytesCopied, present, resized, preVersion, data[i]->version);
        }
    }
    return samplesCopied;
}

void copyFromToSample(audiosample_t* dstSample, float** srcBuf, samplecount_t offsetIn, samplecount_t offsetOut, samplecount_t srcSamples, channelnum_t srcChannels) {
    const auto nChannels = math::max<channelnum_t>(srcChannels, dstSample->nChannels);
    const auto nSamples  = math::min<samplecount_t>(srcSamples, dstSample->nSamples);
    for (channelnum_t i = 0; i < nChannels; i++) {
        auto srcChannelIdx = math::min<channelnum_t>(srcChannels - 1, i);
        auto dstChannelIdx = math::min<channelnum_t>(dstSample->nChannels - 1, i);
        float* srcBufChannel   = srcBuf[srcChannelIdx];
        float* dstBufChannel   = dstSample->samples[dstChannelIdx].data();
        //TODO: this does 2 copys to the same destination when going from stereo to mono (MIX FIRST)
        memcpy(dstBufChannel + offsetOut, srcBufChannel + offsetIn, nSamples * sizeof(float));
    }
}
void copyBlockChannelsToSample(audiosample_t* dstSample, AudioBlock* input, samplecount_t offsetIn, samplecount_t offsetOut, samplecount_t len) {
    dbgassert(input->channels >= dstSample->nChannels);
    for (channelnum_t i = 0; i < dstSample->nChannels; i++) {
        float* srcBufChannel = input->buf[i];
        float* dstBufChannel = dstSample->samples[i].data();
        //TODO: this does 2 copys to the same destination when going from stereo to mono (MIX FIRST)
        memcpy(dstBufChannel + offsetOut, srcBufChannel + offsetIn, len * sizeof(float));
    }
}
void audiotrack_t::store(AudioBlock* input, const samplecount_t samplePos) {
    auto startBlock = math::max<size_t>(0, (samplePos) / PER_BLOCK_SAMPLES);
    auto endBlock   = math::max<size_t>(0, (samplePos + input->samples - 1) / PER_BLOCK_SAMPLES);
    while (data.size() <= endBlock) {
        data.push_back(nullptr);
    }
    auto split = std::make_shared<audiotrack_split_t>();
    if (!data[startBlock]) {
        //log_lf(Log::L_DEBUG, "alloc new block #%d\n", startBlock);
        data[startBlock] = std::make_shared<audiotrack_block_t>(input->channels, PER_BLOCK_SAMPLES);
        samplesStored+=PER_BLOCK_SAMPLES;
    }
    if (!data[endBlock]) {
        //log_lf(Log::L_DEBUG, "alloc new block #%d\n", endBlock);
        data[endBlock] = std::make_shared<audiotrack_block_t>(input->channels, PER_BLOCK_SAMPLES);
        samplesStored+=PER_BLOCK_SAMPLES;
    }
    samplecount_t readLen    = input->samples;
    samplecount_t readOffset = 0;
    samplecount_t readbegin = samplePos;
    if (readbegin < 0) {
        if (readbegin < -PER_BLOCK_SAMPLES) {
            return;
        }
        readLen    = readbegin + input->samples;
        readOffset = -readbegin;
        readbegin  = 0;
    }
    samplecount_t startOffsetBlock0 = readbegin - (static_cast<samplecount_t>(startBlock) * PER_BLOCK_SAMPLES);
    samplecount_t lenBlock0         = math::min(PER_BLOCK_SAMPLES - startOffsetBlock0, readLen);
    samplecount_t lenOver           = readLen - lenBlock0;
    auto* blockStart          = data[startBlock].get();
    blockStart->version++;
    blockStart->data.copyFromPosToPos(input->buf, readOffset, startOffsetBlock0, lenBlock0, input->channels);
    if (startBlock != endBlock) {
        dbgassert(lenOver > 0);
        auto* blockEnd = data[endBlock].get();
        blockEnd->version++;
        blockEnd->data.copyFromPosToPos(input->buf, lenBlock0, 0, lenOver, input->channels);
    } else {
        dbgassert(lenOver == 0);
    }
}
