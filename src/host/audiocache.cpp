#include "audiocache.h"
#include <cstring>
#include <unordered_map>
#include <atomic>
#include "appsettings.h"
#include "assert_dbg.h"
#include "audioblock.h"
#include "math/seq_math.h"
#include "samplerate.h"
#include "str_util.h"
#include "audiosample.h"
#include <dr_libs/dr_wav.h>
#include "tls.h"
#include "types.h"
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
audiofile_t* audiocache::getByFilename(const String& pathFile) {
    for (auto& w : list) {
        if (w->path == pathFile) {
            return w.get();
        }
    }
    return nullptr;
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
        log_printf("reloading file %s with new samplerate %u\n", StringAsCStr(f.path), samplerate);
        loadFile(f.path, f.id);
    }
}
void audiocache::updateSample(const store_sample_req_t& ssr) {
    dbgassert(ssr.id >= 0);
    auto it = mapId.find(ssr.id);
    dbgassert(it != mapId.end());
    auto file = it->second;
    auto sample = file->sample.get();
    auto& loadedSampleChannels = ssr.channels;
    channelnum_t numChannels = math::clamp<size_t>(loadedSampleChannels.size(), 0, 255);
    dbgassert(ssr.format.sampleRate == sample->sampleRate);
    dbgassert(loadedSampleChannels.size() == sample->nChannels);
    if ((samplerate_t) ssr.format.sampleRate != this->samplerate) {
        dbgassert(0);
        //TODO: convert in temp buffer then append to this sample
    }
    sample->nSamples   = math::max(sample->nSamples, ssr.offset+ssr.length);
    sample->sampleRate = this->samplerate;
    if (ssr.offset == 0 && ssr.length == 0) {
        sample->samples = loadedSampleChannels;
    } else {
        if (sample->samples.size() != numChannels) {
            sample->samples.resize(numChannels);
        }
        for (channelnum_t ch = 0; ch < numChannels; ch++) {
            if (static_cast<samplecount_t>(sample->samples[ch].size()) < sample->nSamples) {
                samplecount_t newSize = sample->nSamples;
                if (ssr.preAllocate) {
                    newSize = static_cast<samplecount_t>(ssr.preAllocate * (((newSize) + ssr.preAllocate - 1) / (ssr.preAllocate)));
                }
                /* resize to fit new samples */
                /* Note: this is a bit wasteful, but it's better than reallocating every time */
                sample->samples[ch].resize(newSize);
            }
        }
        for (channelnum_t ch = 0; ch < numChannels; ch++) {
            auto& srcChannel = loadedSampleChannels[ch];
            auto& dstChannel = sample->samples[ch];
            memcpy(dstChannel.data() + ssr.offset, srcChannel.data(), ssr.length * sizeof(float));
        }
    }
    int64_t timeBeginDownsample = getTimeMicros();

    uint8_t maxDownS = 4;
    int numDownS = 0;
    for (uint8_t downsampleStep = 1; downsampleStep < maxDownS; downsampleStep++) {
        samplecount_t lenSamplesDownsampled = sample->nSamples >> downsampleStep;

        if (lenSamplesDownsampled < 10)
            break;
        if (CtrSize(sample->downsampled) <= numDownS) {
            sample->downsampled.emplace_back();
        }
        auto& downsamplesChannels = sample->downsampled[numDownS];
        if (downsamplesChannels.size() != numChannels) {
            downsamplesChannels.resize(numChannels);
        }
        for (channelnum_t ch = 0; ch < numChannels; ch++) {
            samplechannel_t& chDownSmpld = downsamplesChannels[ch];
            if (static_cast<samplecount_t>(chDownSmpld.size()) < lenSamplesDownsampled) {
                chDownSmpld.resize(lenSamplesDownsampled);
            }
            downsample(sample->sampleRate,
                        sample->samples.at(ch).data(),
                        ssr.offset,
                        ssr.length,
                        chDownSmpld, downsampleStep);
        }
        numDownS++;
    }
    while (CtrSize(sample->downsampled) > numDownS) {
        sample->downsampled.resize(numDownS);
    }
    auto timeDiffDownsample = (getTimeMicros() - timeBeginDownsample) / 1000000.0;
    if (timeDiffDownsample > 0.001) {
        log_lf(Log::L_DEBUG, "Downsampling %s took %fsec\n", StringAsCStr(file->path), timeDiffDownsample);
    }
}
audiofile_t* audiocache::createSample(const create_sample_req_t& ssr) {
    std::unique_ptr<audiosample_t> sample = std::make_unique<audiosample_t>();

    sample->bitsPerSample = ssr.format.sampleformat == sampleformat_bits_t::FLOAT_32 ? 32 : 64;
    sample->nChannels     = math::clamp<size_t>(ssr.numChannels, 0, 255);
    sample->sampleRate    = ssr.format.sampleRate;
    sample->nSamples      = 0;
    if (ssr.preAllocate) {
        sample->samples.resize(sample->nChannels);
        for (channelnum_t ch = 0; ch < sample->nChannels; ch++) {
            sample->samples[ch].resize(ssr.preAllocate);
        }
    }
    log_printf("createSample %s...\n", StringAsCStr(ssr.path));
    log_lf(Log::L_DEBUG, "channels: %u\n", sample->nChannels);
    log_lf(Log::L_DEBUG, "sampleRate: %u\n", sample->sampleRate);

    int32_t _id = ssr.id;
    if (_id < 0) {
        _id = this->nextIdx++;
    }
    this->nextIdx       = math::max(this->nextIdx.load(), _id + 1);
    auto spFile    = std::make_unique<audiofile_t>();
    spFile->sample = std::move(sample);
    spFile->state = audiofile_t::filestate::LOADED_MODIFIED;
    spFile->id     = _id;
    spFile->path   = ssr.path;
    String a, b, c, d;
    SplitPath(ssr.path, &a, &b, &c, &d);
    spFile->name = b;
    spFile->ext  = c;
    this->mapId[_id]  = spFile.get();
    audiofile_t* pFile = spFile.get();
    list.push_back(std::move(spFile));
    dbgassert(mapId[_id] == pFile);
    return pFile;
}
static bool LoadAudioSample(const String& path, audiosample_t* sample, samplerate_t samplerate) {
    drwav wav{};
    if (drwav_init_file(&wav, StringAsCStr(path))) {
        struct close_wave_file {
            drwav* wav;
            ~close_wave_file() { drwav_uninit(wav); }
        } closeWaveFile{&wav};
        std::vector<float> pSamples(wav.totalSampleCount);
        memset(pSamples.data(), 0, sizeof(float) * pSamples.size());
        samplecount_t numSamplesInterleaved = drwav_read_f32(&wav, wav.totalSampleCount, pSamples.data());
        pSamples.resize(numSamplesInterleaved);
        if (wav.channels <= 0) {
            log_lf(Log::L_WARN, "File %s has 0 channels\n", StringAsCStr(path));
            return false;
        }
        // log_lf(Log::L_TRACE, "totalSampleCount: %zu\n", wav.totalSampleCount);
        // log_lf(Log::L_TRACE, "numSamplesRead: %zu\n", numSamplesRead);
        // log_lf(Log::L_TRACE, "channels: %d\n", wav.fmt.channels);
        // log_lf(Log::L_TRACE, "sampleRate: %d\n", wav.fmt.sampleRate);
        // log_lf(Log::L_TRACE, "bitsPerSample: %d\n", wav.fmt.bitsPerSample);
        // log_lf(Log::L_TRACE, "samples: %zu\n", nSamples);


        sample->bitsPerSample = wav.bitsPerSample;
        sample->nChannels     = math::clamp<size_t>(wav.channels, 0, 255);
        sample->sampleRate    = samplerate;
        sample->nSamples      = 0;

        std::vector<samplechannel_t> loadedSampleChannels;
        samplecount_t numSamplesInput = numSamplesInterleaved / wav.channels;
        // deinterleave
        for (channelnum_t i = 0; i < sample->nChannels; i++) {
            samplechannel_t channel(numSamplesInput);
            auto out = channel.begin();
            // interleaved sample is at samples[ chIdx + sampleIdx * chCount ]
            for (samplecount_t j = i; j < numSamplesInterleaved; j += wav.channels) {
                *out++ = pSamples[j];
            }
            loadedSampleChannels.push_back(std::move(channel));
        }
        if (samplerate != wav.sampleRate) {

            std::vector<samplechannel_t> resampledChannels;
            std::vector<float*> channelPtrsOut(sample->nChannels);
            std::vector<float*> channelPtrsIn(sample->nChannels);
            auto numSamplesResampled = static_cast<samplecount_t>(numSamplesInput * samplerate / (double) wav.sampleRate + .5); /* Assay output len. */

            for (channelnum_t ch = 0; ch < sample->nChannels; ch++) {
                channelPtrsIn[ch] = loadedSampleChannels[ch].data();
                samplechannel_t channel(numSamplesResampled);
                resampledChannels.push_back(std::move(channel));
                channelPtrsOut[ch] = resampledChannels[ch].data();
            }

            soxr_quality_spec_t q_spec             = soxr_quality_spec(0, 0);
            soxr_io_spec_t io_spec                 = soxr_io_spec(SOXR_FLOAT32_S, SOXR_FLOAT32_S);
            soxr_runtime_spec_t const runtime_spec = soxr_runtime_spec(0);

            soxr_error_t error = 0;
            size_t offset = 0;

            soxr_t soxr = soxr_create(wav.sampleRate, samplerate, sample->nChannels, &error, &io_spec, &q_spec, &runtime_spec);
            if (!!error) {
                log_lf(Log::L_ERROR, "soxr_create failed: %s\n", soxr_strerror(error));
            } else {
                error = soxr_process(soxr, channelPtrsIn.data(), numSamplesInput, nullptr, channelPtrsOut.data(), numSamplesResampled, &offset);
                if (!!error) {
                    log_lf(Log::L_ERROR, "soxr_process failed: %s\n", soxr_strerror(error));
                } else {
                    sample->nSamples   = static_cast<int64_t>(offset);
                    sample->samples.resize(sample->nChannels);
                    for (channelnum_t i = 0; i < sample->nChannels; i++) {
                        sample->samples[i] = std::move(resampledChannels[i]);
                    }
                }
            }
            soxr_delete(soxr);
        } else {
            sample->nSamples = numSamplesInput;
            sample->samples.resize(sample->nChannels);
            sample->samples = std::move(loadedSampleChannels);
        }
        int64_t timeBeginDownsample = getTimeMicros();

        uint8_t maxDownS = 4;
        for (uint8_t downsampleStep = 1; downsampleStep < maxDownS; downsampleStep++) {
            samplecount_t lenSamplesDownsampled = sample->nSamples >> downsampleStep;

            if (lenSamplesDownsampled < 10)
                break;

            std::vector<samplechannel_t> downsampledChannels(2);
            for (channelnum_t ch = 0; ch < sample->nChannels; ch++) {
                samplechannel_t chDownSmpld(static_cast<size_t>(lenSamplesDownsampled));
                downsample(sample->sampleRate,
                           sample->samples.at(ch).data(),
                           0,
                           sample->nSamples,
                           chDownSmpld, downsampleStep);
                downsampledChannels[ch] = std::move(chDownSmpld);
            }
            sample->downsampled.push_back(std::move(downsampledChannels));
        }
        int64_t timeDiffDownsample = getTimeMicros() - timeBeginDownsample;
        double timeDiffInSeconds = timeDiffDownsample / 1000000.0;
        if (timeDiffInSeconds > 1.0) {
            log_lf(Log::L_WARN, "Downsampling %s took %fsec\n", path.c_str(), timeDiffInSeconds);
        }
        return true;
    }
    return false;
}
audiofile_t* audiocache::loadFile(const String& pathIn, int32_t id) {
    String path = pathIn;
    auto mappings = daw_tls::getSettings().pathmapping;
    bool replacedPath = false;
    // split path using platform specific path separator
    // then check if any of the path parents are mapped in the hashmap and if so,
    // replace the path with the mapped path
    for (auto& mapping : mappings.pathRemapping) {
        if (path.find(mapping.first) == 0) {
            String newPath = path;
            newPath.replace(0, mapping.first.size(), mapping.second);
            App::Platform::sanitizePathToFile(newPath);
            if (FileExists(newPath)) {
                path = newPath;
                replacedPath = true;
                break;
            }
        }
    }
    if (!replacedPath) {
        App::Platform::sanitizePathToFile(path);
    }

    //TODO: sanitize path so comparison matches, or ask os if path equals a file we already loaded before
    for (auto& w : list) {
        if (w->path == path) {
            auto pFile = w.get();
            mapId[w->id] = pFile;
            log_printf("skipping file %s (requested id %d), already loaded (id %d)\n", StringAsCStr(path), id, pFile->id);
            return pFile;
        }
    }

    std::unique_ptr<audiosample_t> sample = std::make_unique<audiosample_t>();
    int32_t _id = id;
    if (_id < 0) {
        _id = this->nextIdx++;
    }
    this->nextIdx = math::max(this->nextIdx.load(), _id + 1);
    auto file = std::make_unique<audiofile_t>();
    file->state  = audiofile_t::filestate::UNLOADED;
    file->sample = std::move(sample);
    file->id     = _id;
    file->path   = path;
    String a, b, c, d;
    SplitPath(path, &a, &b, &c, &d);
    file->name = b;
    file->ext  = c;
    auto pFile  = file.get();
    this->mapId[_id] = pFile;
    list.push_back(std::move(file));
    if (LoadAudioSample(path, pFile->sample.get(), samplerate)) {
        pFile->state = audiofile_t::filestate::LOADED;
        log_printf("Loaded %s\n", path.c_str());
    } else {
        pFile->state = audiofile_t::filestate::UNLOADED_MISSING;
        log_printf("Failed to load %s\n", path.c_str());
    }
    return pFile;
}

int64_t saveSample(audiofile_t& file, const String& fOutWave) {
    if (fOutWave.empty()) {
        dbgassert(0);
        return 0;
    }
    String path;
    SplitPath(fOutWave, &path, nullptr, nullptr, nullptr);
    if (!path.empty()) {
        CreateDirectoryIfNotExists(path);
    }
    auto sample = file.getSample();
    dbgassert(sample->nSamples == 0 || sample->nChannels == sample->samples.size());
    log_printf("saveSample %d %s len %zd\n", file.id, StringAsCStr(fOutWave), sample->nSamples);

    drwav_data_format format;
    format.container = drwav_container_riff;    // drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;  // Any of the DR_WAVE_FORMAT_* codes.
    format.channels = sample->nChannels;
    format.sampleRate = sample->sampleRate;
    format.bitsPerSample = 32;

    drwav* pWav = drwav_open_file_write(StringAsCStr(fOutWave), &format);
    struct close_wave_file_write {
        drwav* wav;
        ~close_wave_file_write() { drwav_close(wav); }
    } closeWaveFile{pWav};

    AudioBlock blockFull(1, sample->nSamples*format.channels);

    /* Interleave data */
    for (channelnum_t ch = 0; sample->nSamples && ch < sample->nChannels; ch++) {
        float* in = sample->samples[ch].data();
        float* out0 = blockFull.buf[0] + ch;
        for (samplecount_t i = 0; i < sample->nSamples; i++) {
            *out0 = *in++;
            out0 += format.channels;
        }
    }

    auto samplesWritten = drwav_write(pWav, drwav_uint64(sample->nSamples*format.channels), blockFull.buf[0]);

    return static_cast<samplecount_t>(samplesWritten);
}
void audiocache::saveSamples(const std::vector<int32_t>& refSampleIds) {
    for (auto& w : list) {
        auto* ptr = w.get();
        if (std::binary_search(refSampleIds.cbegin(), refSampleIds.cend(), ptr->id)
            && ptr->state == audiofile_t::filestate::LOADED_MODIFIED) {
            if (FileExists(ptr->path)) {
                log_lf(Log::L_WARN, "Overwriting sample %s\n", ptr->path.c_str());
            }
            saveSample(*ptr, ptr->path);
            ptr->state = audiofile_t::filestate::LOADED;
        }
    }
}
void audiocache::store(const std::vector<int32_t>& refSampleIds, samplefile_index_t& v) {
    v.list.reserve(list.size());
    for (auto& w : list) {
        auto* ptr = w.get();
        if (std::binary_search(refSampleIds.cbegin(), refSampleIds.cend(), ptr->id)) {
            v.list.push_back({ ptr->id, ptr->path });
        }
    }
}
void audiocache::unloadUnreferenced(const std::vector<int32_t>& refSampleIds) {
    auto it = list.begin();
    while (it != list.end()) {
        auto* ptr = it->get();
        if (!std::binary_search(refSampleIds.cbegin(), refSampleIds.cend(), ptr->id)) {
            log_lf(Log::L_DEBUG, "Unloading unreferenced sample %s (ID %d)\n", StringAsCStr(ptr->path), ptr->id);
            auto itMap = mapId.find(ptr->id);
            if (itMap != mapId.end()) {
                mapId.erase(itMap);
            }
            it = list.erase(it);
        } else {
            log_lf(Log::L_DEBUG, "Keeping referenced sample %s (ID %d)\n", StringAsCStr(ptr->path), ptr->id);
            ++it;
        }
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
