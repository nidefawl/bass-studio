#include "audiocache.h"
#include <unordered_map>
#include <atomic>
#include "assert_dbg.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "audiosample.h"
#include <dr_libs/dr_wav.h>
#include "wave/downsample.h"
#include "wave/waveform_render_impl.h"
#include "platform.h"
#include "fileio.h"
#include "logging.h"
#include <soxr.h>

void audiocache::getLoaded(std::vector<audiofile_t*>& v) {
    v.reserve(list.size());
    for (auto& w : list) {
        v.push_back(w.get());
    }
}
audiofile_t* audiocache::get(int32_t i) {
    size_t count = this->mapId.count(i);
    return count ? this->mapId.at(i) : nullptr;
}
void audiocache::unloadSampleId(int32_t id) {
    mapId.erase(id);
    auto it = std::remove_if(list.begin(), list.end(), [id](std::unique_ptr<audiofile_t>& r) {
        return r->id == id;
    });
    if (it != list.end()) {
        list.erase(it, list.end());
    }
}

void audiocache::setSamplerate(samplerate_t _samplerate) {
    this->samplerate = _samplerate;
    std::vector<audiofile_path_t> reloadFiles;
    for (auto it = list.begin(); it != list.end();) {
        auto& w = *it;
        if (w->sample->sampleRate != static_cast<uint32_t>(_samplerate)) {
            reloadFiles.push_back(w->getPath());
            mapId.erase(w->id);
            it = list.erase(it);
        } else {
            it++;
        }
    }
    for (auto& f : reloadFiles) {
        log_printf("reloading file %s with new samplerate %d\n", StringAsCStr(f.path), samplerate);
        loadFile(f.path, f.id);
    }
}

audiofile_t* audiocache::loadFile(const String& path, int32_t id) {
    drwav wav;

    //TODO: satinize path so comparison matches, or ask os if path equals a file we already loaded before
    for (auto& w : list) {
        if (w.get()->path == path) {
            log_printf("skipping file %s (requested id %d), already loaded (id %d)\n", StringAsCStr(path), id, w.get()->id);
            return w.get();
        }
    }

    log_printf("Loading %s...\n", path.c_str());
    if (drwav_init_file(&wav, StringAsCStr(path))) {
        std::vector<float> pSamples(wav.totalSampleCount);
        memset(pSamples.data(), 0, sizeof(float) * pSamples.size());
        size_t nSamples = drwav_read_f32(&wav, wav.totalSampleCount, pSamples.data());

        log_lf(Log::L_DEBUG, "totalSampleCount: %d\n", wav.totalSampleCount);
        log_lf(Log::L_DEBUG, "channels: %d\n", wav.fmt.channels);
        log_lf(Log::L_DEBUG, "sampleRate: %d\n", wav.fmt.sampleRate);
        log_lf(Log::L_DEBUG, "bitsPerSample: %d\n", wav.fmt.bitsPerSample);
        log_lf(Log::L_DEBUG, "samples: %d\n", nSamples);

        std::unique_ptr<audiosample_t> sample = std::make_unique<audiosample_t>();

        sample->bitsPerSample = wav.bitsPerSample;
        sample->nChannels     = math::clamp<channelnum_t>(wav.channels, 0, 255);
        sample->sampleRate    = wav.sampleRate;
        sample->nSamples      = 0;

        std::vector<samplechannel_t> loadedSampleChannels;
        samplecount_t numSamplesInput = 0;
        auto wavSamples = static_cast<samplecount_t>(wav.totalSampleCount);
        // deinterleave
        for (channelnum_t i = 0; i < sample->nChannels; i++) {
            samplechannel_t channel(wavSamples / wav.channels);
            float* out = channel.data();
            // interleaved sample is at samples[ chIdx + sampleIdx * chCount ]
            for (samplecount_t j = i; j < wavSamples; j += wav.channels) {
                *out = pSamples[j];
                out++;
            }
            auto samplesInChannel = static_cast<samplecount_t>(channel.size());
            numSamplesInput = i == 0 ? samplesInChannel : math::min<samplecount_t>(numSamplesInput, samplesInChannel);
            loadedSampleChannels.push_back(std::move(channel));
        }
        if ((samplerate_t) sample->sampleRate != this->samplerate) {

            std::vector<samplechannel_t> resampledChannels;
            std::vector<float*> channelPtrsOut(sample->nChannels);
            std::vector<float*> channelPtrsIn(sample->nChannels);
            auto numSamplesResampled = static_cast<samplecount_t>(numSamplesInput * this->samplerate / (double) wav.sampleRate + .5); /* Assay output len. */

            for (channelnum_t ch = 0; ch < sample->nChannels; ch++) {
                channelPtrsIn[ch] = loadedSampleChannels[ch].data();
                samplechannel_t channel(numSamplesResampled);
                resampledChannels.push_back(std::move(channel));
                channelPtrsOut[ch] = resampledChannels[ch].data();
            }


            //log_lf(Log::L_DEBUG, "soxr_oneshot from %d to %d, samples %d -> %d, channels %d\n", wav.sampleRate, this->samplerate, wavSamples, olen, wav.channels);
            //log_lf(Log::L_DEBUG, "pSamples.size %d\n", pSamples.size());
            //log_lf(Log::L_DEBUG, "pSamples2.size %d\n", pSamples2.size());

            soxr_quality_spec_t q_spec             = soxr_quality_spec(0, 0);
            soxr_io_spec_t io_spec                 = soxr_io_spec(SOXR_FLOAT32_S, SOXR_FLOAT32_S);
            soxr_runtime_spec_t const runtime_spec = soxr_runtime_spec(0);

            soxr_error_t error = 0;
            size_t offset = 0;

            soxr_t soxr = soxr_create(sample->sampleRate, this->samplerate, sample->nChannels, &error, &io_spec, &q_spec, &runtime_spec);
            if (!!error) {
                log_printf("soxr_create failed: %d %s\n", error, soxr_strerror(error));
            } else {
                error = soxr_process(soxr, channelPtrsIn.data(), numSamplesInput, nullptr, channelPtrsOut.data(), numSamplesResampled, &offset);
                log_lf(Log::L_DEBUG, "offset %d, pSamples.size: %d\n", offset, pSamples.size());


                log_lf(Log::L_DEBUG, "soxr_process post %d\n", error);
                if (!!error) {
                    log_printf("soxr_process failed: %d %s\n", error, soxr_strerror(error));
                } else {
                    sample->nSamples   = static_cast<int64_t>(offset);
                    sample->sampleRate = this->samplerate;
                    sample->samples.resize(sample->nChannels);
                    for (channelnum_t i = 0; i < sample->nChannels; i++) {
                        sample->samples[i] = std::move(resampledChannels[i]);
                    }
                }
            }
            soxr_delete(soxr);
        } else {
            sample->nSamples   = numSamplesInput;
            sample->sampleRate = this->samplerate;
            sample->samples.resize(sample->nChannels);
            sample->samples = std::move(loadedSampleChannels);
        }
        int64_t timeBeginDownsample = getTimeMicros();
        log_printf("Downsampling %s...\n", path.c_str());

        uint8_t maxDownS = 1;
        for (uint8_t downsampleStep = 1; downsampleStep < maxDownS; downsampleStep++) {
            samplecount_t lenSamplesDownsampled = sample->nSamples >> downsampleStep;

            if (lenSamplesDownsampled < 10)
                break;

            std::vector<samplechannel_t> downsampledChannels(2);
            for (channelnum_t ch = 0; ch < sample->nChannels; ch++) {
                samplechannel_t chDownSmpld(static_cast<size_t>(lenSamplesDownsampled));
                downsample(sample->sampleRate,
                           sample->samples.at(ch).data(),
                           sample->nSamples,
                           chDownSmpld, downsampleStep);
                downsampledChannels[ch] = std::move(chDownSmpld);
            }
            sample->downsampled.push_back(std::move(downsampledChannels));
        }
        int64_t timeDiffDownsample = getTimeMicros() - timeBeginDownsample;
        log_lf(Log::L_DEBUG, "Downsampling %s took %fsec\n", path.c_str(), timeDiffDownsample / 1000000.0);
        int32_t _id = id;
        if (_id < 0) {
            _id = this->nextIdx++;
        }
        this->nextIdx       = math::max(this->nextIdx.load(), _id + 1);
        auto cachedaudio    = std::make_unique<audiofile_t>();
        cachedaudio->sample = std::move(sample);
        cachedaudio->id     = _id;
        cachedaudio->path   = path;
        String a, b, c, d;
        SplitPath(path, &a, &b, &c, &d);
        cachedaudio->name = b;
        cachedaudio->ext  = c;
        this->mapId[_id]  = cachedaudio.get();
        audiofile_t* audio = cachedaudio.get();
        list.push_back(std::move(cachedaudio));
        dbgassert(mapId[_id] == audio);
        return audio;
    }
    return nullptr;
}

void audiocache::store(samplefile_index_t& v) {
    v.list.reserve(list.size());
    for (auto& w : list) {
        samplefile_index_t index;
        auto* ptr = w.get();
        v.list.push_back({ ptr->id, ptr->path });
    }
}

void audiocache::unloadAll() {
    list.clear();
    mapId.clear();
    this->nextIdx = 0;
}

void audiocache::load(samplefile_index_t& v) {
    unloadAll();
    list.reserve(v.list.size());
    for (auto& w : v.list) {
        loadFile(w.name, w.id);
    }
}

bool audiocache::isEmpty() const {
    return list.empty() && mapId.empty();
}
