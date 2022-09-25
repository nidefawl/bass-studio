#pragma once
#include "types.h"
#include "audiosample.h"
#include "audioblock.h"
#include "host/mainctrl.h"
#include <memory>


namespace DAW {
    class pluginhost;
}
struct audiotrack_split_t : public samplesource_t {
    int64_t sampleId = 0;
    int64_t version  = 0;
    samplecount_t samplePos;
    audiosample_t sample;
    audiosample_t* getSample() override {
        return &sample;
    }
};
struct audiotrack_block_t {
    int64_t version = 0;
    AudioBlock data;
    audiotrack_block_t(channelnum_t _channels, samplecount_t _samples) : data(_channels, _samples) {
    }
};
struct audiotrack_t {
    int64_t samplesStored = 0;
    std::vector<std::shared_ptr<audiotrack_block_t>> data;
    std::vector<std::shared_ptr<audiotrack_split_t>> samples;
    std::shared_ptr<audiotrack_split_t> getSample(samplecount_t samplePos);
    std::shared_ptr<audiotrack_split_t> getSampleById(int64_t sampleId);
    template<typename Functor>
    void visitSamples(Functor f) {
        ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
        std::for_each(samples.begin(), samples.end(), f);
    }
    template<typename Functor>
    void visitSamples_NoLock(Functor f) {
        std::for_each(samples.begin(), samples.end(), f);
    }
    /**
     * Convert tracks backing buffers to audiotrack_split_t
     * @param host
     * @return number of bytes copied
     */
    samplecount_t convertToSamples(DAW::pluginhost* host);
    samplecount_t readSamples(samplecount_t samplePos, samplecount_t numSamples, channelnum_t numChannels, std::vector<samplechannel_t>& outChannels);
    void store(AudioBlock* input, const samplecount_t samplePos);
    static samplecount_t GetSplitSampleLength();
};
