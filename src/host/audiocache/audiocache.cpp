#include "host/audiocache/audiocache.h"
#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <signalsmith-stretch.h>
#include <sys/types.h>
#include <unordered_map>
#include <atomic>
#include "appsettings.h"
#include "assert_dbg.h"
#include "host/audiobuffer/audioblock.h"
#include "host/clip/clip.h"
#include "config.h"
#include "host/daw/daw_async_project_load.h"
#include "math/seq_math.h"
#include "samplerate.h"
#include "seq_util.h"
#include "str_util.h"
#include "host/audiosample.h"
#include "thread.h"
#include "tls.h"
#include "types.h"
#include "wave/downsample.h"
#include "wave/waveform_render_impl.h"
#include "platform.h"
#include "fileio.h"
#include "logging.h"
#include <soxr.h>
#include <utility>
#include <variant>
#include <vector>

/* void audiocache::getLoaded(std::vector<audiofile_t*>& v) {
    v.reserve(list.size());
    for (auto& w : list) {
        v.push_back(w.get());
    }
} */
audiofile_t* audiocache::getSample(int32_t i) {
    auto it = mapId.find(i);
    return it != mapId.end() ? it->second : nullptr;
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
    auto it = std::remove_if(list.begin(), list.end(), [id](std::shared_ptr<audiofile_t>& r) {
        return r->id == id;
    });
    if (it != list.end()) {
        list.erase(it, list.end());
    }
}

void audiocache::setSamplerate(samplerate_t _newSampleRate) {
    if (samplerate != _newSampleRate) {
        samplerate = _newSampleRate;
        if (samplerate == 0) {
            return;
        }
        soxr_io_spec_t iospec;
        iospec.flags = 0;
        iospec.scale = 1;
        iospec.e     = 0;
        iospec.itype = SOXR_FLOAT32_I;
        iospec.otype = SOXR_FLOAT32_I;

        for (auto& f : list) {
            auto* file = f.get();
            if (!(file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADED)) {
                continue;
            }
            auto* sample = f->sample.get();
            if (sample->sampleRate == _newSampleRate) {
                continue;
            }
            sample->downsampled = {};

            auto numSamplesInput = static_cast<samplecount_t>(sample->nSamples);
            auto numSamplesResampled = static_cast<samplecount_t>(sample->nSamples * double(_newSampleRate) / static_cast<double>(sample->sampleRate) + .5); /* Assay output len. */
            std::vector<samplechannel_t> resampledChannels(sample->nChannels);
            std::vector<float*> channelPtrsOut(sample->nChannels);
            std::vector<float*> channelPtrsIn(sample->nChannels);
            for (channelnum_t ch = 0; ch < sample->nChannels; ch++) {
                channelPtrsIn[ch] = sample->samples[ch].data();
                resampledChannels[ch].resize(numSamplesResampled);
                channelPtrsOut[ch] = resampledChannels[ch].data();
            }

            soxr_quality_spec_t q_spec             = soxr_quality_spec(0, 0);
            soxr_io_spec_t io_spec                 = soxr_io_spec(SOXR_FLOAT32_S, SOXR_FLOAT32_S);
            soxr_runtime_spec_t const runtime_spec = soxr_runtime_spec(0);

            soxr_error_t error = 0;
            size_t offset = 0;

            soxr_t soxr = soxr_create(sample->sampleRate, _newSampleRate, sample->nChannels, &error, &io_spec, &q_spec, &runtime_spec);
            if (!!error) {
                log_lf(Log::L_ERROR, "soxr_create failed: %s\n", soxr_strerror(error));
            } else {
                error = soxr_process(soxr, channelPtrsIn.data(), numSamplesInput, nullptr, channelPtrsOut.data(), numSamplesResampled, &offset);
                soxr_delete(soxr);
                if (!!error) {
                    log_lf(Log::L_ERROR, "soxr_process failed: %s\n", soxr_strerror(error));
                } else {
                    sample->nSamples = static_cast<int64_t>(offset);
                    sample->samples = std::move(resampledChannels);
                    sample->sampleRate = _newSampleRate;
                    audiocache::Downsample(sample);
                }
            }
        }
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
    if (ssr.bDownsample) {
        audiocache::Downsample(sample);
    }
}

audiofile_t* audiocache::createSample(const create_sample_req_t& ssr) {
    auto sample = std::make_unique<audiosample_t>();

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
    auto spFile    = std::make_shared<audiofile_t>();
    spFile->sample = std::move(sample);
    spFile->state = audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADED | audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_MODIFIED;
    if (ssr.isTemporarySample) {
        spFile->state |= audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_TEMPORARY;
    }
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

void audiocache::Downsample(audiosample_t* sample, std::atomic<bool>* abortFlag, uint8_t numSteps) {
    int64_t timeBeginDownsample = getTimeMicros();
    for (uint8_t downsampleStep = 1; downsampleStep <= numSteps && (!abortFlag || !abortFlag->load()); downsampleStep++) {
        samplecount_t lenSamplesDownsampled = sample->nSamples >> downsampleStep;

        if (lenSamplesDownsampled < 10)
            break;
        sample->downsampleSteps = downsampleStep - 1;
        auto& chDown = sample->downsampled.at(downsampleStep - 1);
        chDown.resize(sample->nChannels);
        for (channelnum_t ch = 0; ch < sample->nChannels && (!abortFlag || !abortFlag->load()); ch++) {
            auto& chIn = sample->samples.at(ch);
            auto& chOut = chDown.at(ch);
            chOut.resize(lenSamplesDownsampled);
            downsample(sample->sampleRate,
                        chIn.data(),
                        0,
                        sample->nSamples,
                        chOut,
                        downsampleStep
                    );
        }
        sample->downsampleSteps = downsampleStep;
    }
    int64_t timeDiffDownsample = getTimeMicros() - timeBeginDownsample;
    double timeDiffInSeconds = timeDiffDownsample / 1000000.0;
    if (timeDiffInSeconds > 1.0) {
        log_lf(Log::L_WARN, "Downsampling %zu samples took %fsec\n", sample->nSamples, timeDiffInSeconds);
    }
}

void audiocache::addFile(std::shared_ptr<audiofile_t>& af) {
    dbgassert(af.get());
    if (af->id < 0) {
        af->id = this->nextIdx++;
    }
    this->nextIdx = math::max(this->nextIdx.load(), af->id + 1);
    dbgassert(af->id >= 0);
    dbgassert(this->mapId.find(af->id) == this->mapId.end());
    this->mapId[af->id] = af.get();
    this->list.push_back(af);
}

bool audiocache::fileloader::resolveFile(const String& pathIn, const String& workingDir, bool remapPath, int32_t id) {
    String path = pathIn;
    if (remapPath) {
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

        if (!workingDir.empty() && !FileExists(path)) {
            bool bIsAbsolute = (!path.empty() && path[0] == '/') || (path.size() > 1 && path[1] == ':');
            if (!bIsAbsolute) {
                String path2 = workingDir + FILE_PATHSEP_STR + path;
                App::Platform::sanitizePathToFile(path2);
                if (FileExists(path2)) {
                    path = path2;
                }
            }
            
        }
    }

    file = std::make_shared<audiofile_t>();
    file->state  = audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADING;
    file->sample = std::make_unique<audiosample_t>();
    file->path   = pathIn;
    file->pathLoaded = path;
    file->id     = id;
    String pathOnly, nameOnly, extOnly, nameExt;
    SplitPath(path, &pathOnly, &nameOnly, &extOnly, &nameExt);
    file->name = nameOnly;
    file->ext  = extOnly;
    return true;
}

bool audiocache::fileloader::preloadFile(struct archive* ar, struct archive_entry* entry) {
    auto pFile  = file.get();
    audiofile_variant drAudiofile = std::monostate{};
    bool bCanRead = false;
    const auto& extOnly = file->ext;
    const auto& path = file->pathLoaded;
    if (!entry) {
        if (extOnly == "wav") {
            drAudiofile = drwav{};
            bCanRead = drwav_init_file(&std::get<drwav>(drAudiofile), StringAsCStr(path), nullptr);
        } else if (extOnly == "mp3") {
            drAudiofile = drmp3{};
            bCanRead = drmp3_init_file(&std::get<drmp3>(drAudiofile), StringAsCStr(path), nullptr);
        } else if (extOnly == "flac" || extOnly == "ogg") {
            auto pDrFlac = drflac_open_file(StringAsCStr(path), nullptr);
            if (pDrFlac) {
                bCanRead = true;
                drAudiofile = drflac_file{pDrFlac};
            }
        } else {
            error = StringFormat("Unsupported file type %s", StringAsCStr(path));
            return false;
        }

        if (!bCanRead) {
            error = StringFormat("Failed to read file %s", StringAsCStr(path));
            return false;
        }
    } else {
        dbgassert(entry && ar);
        pFile->state |= audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_BUNDLED;
        auto sizeToRead = archive_entry_size(entry);
        heapBuffer.resize(sizeToRead);
        auto sizeRead = archive_read_data(ar, heapBuffer.data(), heapBuffer.size());
        if (sizeRead != sizeToRead) {
            error = StringFormat("Failed to read file %s: read %zd bytes, expected %zu bytes", StringAsCStr(path), sizeRead, sizeToRead);
            return false;
        }

        if (extOnly == "wav") {
            drAudiofile = drwav{};
            bCanRead = drwav_init_memory(&std::get<drwav>(drAudiofile), heapBuffer.data(), heapBuffer.size(), nullptr);
        } else if (extOnly == "mp3") {
            drAudiofile = drmp3{};
            bCanRead = drmp3_init_memory(&std::get<drmp3>(drAudiofile), heapBuffer.data(), heapBuffer.size(), nullptr);
        } else if (extOnly == "flac" || extOnly == "ogg") {
            drflac* pDrFlac = drflac_open_memory(heapBuffer.data(), heapBuffer.size(), nullptr);
            if (pDrFlac) {
                bCanRead = true;
                drAudiofile = drflac_file{pDrFlac};
            }
        } else {
            error = StringFormat("Unsupported file type %s", StringAsCStr(path));
            return false;
        }

        if (!bCanRead) {
            error = StringFormat("Failed to read file %s", StringAsCStr(path));
            return false;
        }
    }

    if (std::holds_alternative<drwav>(drAudiofile)) {
        auto& wav = std::get<drwav>(drAudiofile);
        sourceSamplerate = wav.sampleRate;
        sourceNumChannels = wav.channels;
        if (sourceNumChannels <= 0) {
            drwav_uninit(&wav);
            error = StringFormat("File %s has 0 channels", StringAsCStr(path));
            return false;
        }
        fileReadSamples = wav.totalPCMFrameCount * wav.channels;
    } else if (std::holds_alternative<drmp3>(drAudiofile)) {
        auto& mp3 = std::get<drmp3>(drAudiofile);
        sourceSamplerate = mp3.sampleRate;
        sourceNumChannels = mp3.channels;
        if (sourceNumChannels <= 0) {
            error = StringFormat("File %s has 0 channels", StringAsCStr(path));
            drmp3_uninit(&mp3);
            return false;
        }
        fileReadSamples = samplecount_t(drmp3_get_pcm_frame_count(&mp3) * sourceNumChannels);
    } else if (std::holds_alternative<drflac_file>(drAudiofile)) {
        auto& flac = *std::get<drflac_file>(drAudiofile).pFlac;
        sourceSamplerate = flac.sampleRate;
        sourceNumChannels = flac.channels;
        if (sourceNumChannels <= 0) {
            error = StringFormat("File %s has 0 channels", StringAsCStr(path));
            drflac_close(&flac);
            return false;
        }
        fileReadSamples = flac.totalPCMFrameCount * flac.channels;
    } else {
        error = "Unknown audio file type";
        return false;
    }
    pSamples.resize(fileReadSamples + chunkSize);
    std::memset(pSamples.data(), 0, sizeof(float) * pSamples.size());
    auto* sample = file->sample.get();
    sample->bitsPerSample = 32;
    sample->nChannels     = math::clamp<size_t>(sourceNumChannels, 0, 255);
    sample->sampleRate    = sourceSamplerate;
    sample->sampleRate    = targetSamplerate;

    auto numSamples = getExpectedNumSamples();
    sample->samples.resize(sample->nChannels);
    for (channelnum_t ch = 0; ch < sample->nChannels; ch++) {
        sample->samples[ch].resize(numSamples);
    }
    sample->nSamples = numSamples;

    sample->downsampleSteps = 0;
    for (uint8_t downsampleStep = 1; downsampleStep <= audiosample_t::MAX_DOWNSAMPLE; downsampleStep++) {
        samplecount_t lenSamplesDownsampled = sample->nSamples >> downsampleStep;
        auto& chDown = sample->downsampled.at(downsampleStep - 1);
        chDown.resize(sample->nChannels);
        if (lenSamplesDownsampled >= 10) {
            for (channelnum_t ch = 0; ch < sample->nChannels; ch++) {
                auto& chOut = chDown.at(ch);
                chOut.resize(lenSamplesDownsampled);
            }
            sample->downsampleSteps = downsampleStep;
        }
    }

    this->audiofileVariant = std::move(drAudiofile);
    return true;
}

bool audiocache::fileloader::loadFileIncremental() {
    dbgassert(file);
    dbgassert(file->sample);
    dbgassert(audiofileVariant.has_value());
    dbgassert(sourceSamplerate > 0);
    dbgassert(sourceNumChannels > 0);
    dbgassert(!pSamples.empty());
    // dbgassert(!isFinished());
    if (bReadComplete) {
        auto sample = file->sample.get();
        if (!bResampleComplete) {
            if (!resample()) {
                return false;
            }
            return true;
        }
        bool bFinished = false;
        if (downsampleStep != audiosample_t::MAX_DOWNSAMPLE) {
            downsampleStep++;
            auto lenSamplesDownsampled = sample->downsampled.at(downsampleStep - 1).at(0).size();
            if (lenSamplesDownsampled < 10) {
                downsampleStep = audiosample_t::MAX_DOWNSAMPLE;
            } else {
                for (channelnum_t ch = 0; ch < sample->nChannels; ch++) {
                    auto& chIn = sample->samples.at(ch);
                    auto& chOut = sample->downsampled.at(downsampleStep - 1).at(ch);
                    dbgassert(chOut.size() >= lenSamplesDownsampled);
                    if (chOut.size() < lenSamplesDownsampled) {
                        error = StringFormat("Downsampled buffer too small");
                        return false;
                    }
                    downsample(sample->sampleRate,
                                chIn.data(),
                                0,
                                sample->nSamples,
                                chOut,
                                downsampleStep
                            );
                }
            }
            bFinished = downsampleStep == audiosample_t::MAX_DOWNSAMPLE;
        }
        if (bFinished) {
            file->state &= ~audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADING;
            file->state |= audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADED;
        }
        return true;
    }
    audiofile_variant& audiofileVariant = this->audiofileVariant.value();
    if (std::holds_alternative<drwav>(audiofileVariant)) {
        auto& wav = std::get<drwav>(audiofileVariant);
        auto outputBuffer = pSamples.data() + (numSamplesInput * sourceNumChannels);
        auto numRead = drwav_read_pcm_frames_f32(&wav, chunkSize / sourceNumChannels, outputBuffer);
        numSamplesInput += numRead;
        if (numRead < chunkSize / sourceNumChannels) {
            if (size_t(numSamplesInput * sourceNumChannels) < pSamples.size()) {
                pSamples.resize(size_t(numSamplesInput * sourceNumChannels));
            }
            drwav_uninit(&wav);
            audiofileVariant = std::monostate{};
            bReadComplete = true;
        }
    } else if (std::holds_alternative<drmp3>(audiofileVariant)) {
        auto& mp3 = std::get<drmp3>(audiofileVariant);
        auto outputBuffer = pSamples.data() + (numSamplesInput * sourceNumChannels);
        auto numSamplesDecoded = drmp3_read_pcm_frames_f32(&mp3, chunkSize / sourceNumChannels, outputBuffer);
        numSamplesInput += numSamplesDecoded;
        if (numSamplesDecoded < chunkSize / sourceNumChannels) {
            if (size_t(numSamplesInput * sourceNumChannels) < pSamples.size()) {
                pSamples.resize(size_t(numSamplesInput * sourceNumChannels));
            }
            drmp3_uninit(&mp3);
            bReadComplete = true;
        }
    } else if (std::holds_alternative<drflac_file>(audiofileVariant)) {
        auto& flac = *std::get<drflac_file>(audiofileVariant).pFlac;
        if (numSamplesInput == 0) {
            if (drflac_seek_to_pcm_frame(&flac, 0) != DRFLAC_TRUE) {
                error = StringFormat("Failed to seek to PCM frame");
                drflac_close(&flac);
                return false;
            }
        }
        auto outputBuffer = pSamples.data() + (numSamplesInput * sourceNumChannels);
        auto numRead = drflac_read_pcm_frames_f32(&flac, chunkSize / sourceNumChannels, outputBuffer);
        numSamplesInput += numRead;
        if (numRead < chunkSize / sourceNumChannels) {
            if (size_t(numSamplesInput * sourceNumChannels) < pSamples.size()) {
                pSamples.resize(size_t(numSamplesInput * sourceNumChannels));
            }
            drflac_close(&flac);
            bReadComplete = true;
        }
    } else {
        error = "Unknown audio file type";
        audiofileVariant = std::monostate{};
        return false;
    }
    if (bReadComplete) {
        this->heapBuffer.resize(0);
        audiofileVariant = std::monostate{};
    }
    return true;
}
bool audiocache::fileloader::resample() {
    auto* sample = file->sample.get();

    if (targetSamplerate != sourceSamplerate) {

        if (!soxrContext) {
            soxr_quality_spec_t q_spec             = soxr_quality_spec(0, 0);
            soxr_io_spec_t io_spec                 = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_S);
            soxr_runtime_spec_t const runtime_spec = soxr_runtime_spec(0);

            soxr_error_t soxrError = 0;

            soxr_t soxr = soxr_create(sourceSamplerate, targetSamplerate, sourceNumChannels, &soxrError, &io_spec, &q_spec, &runtime_spec);
            if (!!soxrError) {
                error = StringFormat("soxr_create failed: %s", soxr_strerror(soxrError));
                return false;
            }
            soxrContext = soxr;
        }
        if (soxrContext) {
            auto& loadedSampleChannels = sample->samples;
    #ifndef NDEBUG
            auto numSamplesResampled = static_cast<samplecount_t>(numSamplesInput * targetSamplerate / (double) sourceSamplerate + .5);
            dbgassert(loadedSampleChannels.size() == sample->nChannels);
            dbgassert(samplecount_t(loadedSampleChannels[0].size()) == numSamplesResampled);
    #endif
            std::vector<float*> channelPtrsOut(sample->nChannels);
            for (channelnum_t ch = 0; ch < sample->nChannels; ch++) {
                dbgassert(sample->samples[ch].size() >= size_t(this->resampleOutputOffset));
                channelPtrsOut[ch] = sample->samples[ch].data() + this->resampleOutputOffset;
            }
            auto outputBufferLength = samplecount_t(loadedSampleChannels[0].size()) - this->resampleOutputOffset;
            dbgassert(outputBufferLength >= 0);
            auto inputAvailable = (samplecount_t(pSamples.size()) - this->resampleInputOffset) / sourceNumChannels;
            dbgassert(inputAvailable >= 0);
            auto input = pSamples.data() + this->resampleInputOffset;
            if (inputAvailable <= 0) {
                input = NULL;
            }
            size_t usedInputSamples = 0;
            size_t writtenOutputSamples = 0;
            soxr_error_t soxrError = soxr_process(static_cast<soxr_t>(soxrContext), input, math::min<size_t>(inputAvailable, chunkSize), &usedInputSamples, channelPtrsOut.data(), outputBufferLength, &writtenOutputSamples);
            if (!!soxrError) {
                error = StringFormat("soxr_process failed: %s", soxr_strerror(soxrError));
                return false;
            }
            dbgassert(usedInputSamples <= chunkSize);
            
            this->resampleInputOffset += usedInputSamples * sourceNumChannels;
            dbgassert(samplecount_t(pSamples.size()) >= this->resampleInputOffset);
            this->resampleOutputOffset += writtenOutputSamples;
            dbgassert(sample->nSamples >= this->resampleOutputOffset);
            if (input == NULL) {
                soxr_delete(static_cast<soxr_t>(soxrContext));
                soxrContext = nullptr;
                bResampleComplete = true;
            }
        }
    } else {
        auto& loadedSampleChannels = sample->samples;
        dbgassert(loadedSampleChannels.size() == sample->nChannels);
        // deinterleave
        for (channelnum_t i = 0; i < sample->nChannels; i++) {
            dbgassert(samplecount_t(loadedSampleChannels[i].size()) >= numSamplesInput);
            auto out = loadedSampleChannels[i].begin();
            // interleaved sample is at samples[ chIdx + sampleIdx * chCount ]
            for (samplecount_t j = i; j < numSamplesInput * sourceNumChannels; j += sourceNumChannels) {
                *out++ = pSamples[j];
            }
        }
        sample->nSamples = numSamplesInput;
        this->resampleOutputOffset = numSamplesInput;
        bResampleComplete = true;
    }
    return true;
}

bool audiocache::loadFile(std::shared_ptr<audiofile_t>& outFile, const String& pathIn, const String& workingDir, bool remapPath, struct archive* ar, struct archive_entry* entry) {
    fileloader loader;
    loader.setTargetSampleRate(getSampleRate());
    if (!loader.resolveFile(pathIn, workingDir, remapPath)) {
        log_lf(Log::L_ERROR, "Failed to resolve file %s: %s\n", pathIn.c_str(), loader.getError().c_str());
        return false;
    } else if (!loader.preloadFile(ar, entry)) {
        log_lf(Log::L_ERROR, "Failed to preload file %s: %s\n", loader.getFile()->pathLoaded.c_str(), loader.getError().c_str());
        return false;
    } else {
        while (!loader.isFinished()) {
            if (!loader.loadFileIncremental()) {
                break;
            }
        }
        if (!loader.isOk()) {
            log_lf(Log::L_ERROR, "Failed to load file %s: %s\n", loader.getFile()->pathLoaded.c_str(), loader.getError().c_str());
            return false;
        }
        outFile = loader.getSPFile();
        outFile->state |= audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADED;
        addFile(outFile);
        return true;
    }
}


int saveSampleToArchive(audiofile_t& file, struct archive_entry* entry, struct archive* ar,
                                std::function<void(const String&, int32_t, int32_t)>& onProgress,
                                std::function<void(const String& msg, const String& file)>& onError) {
    auto sample = file.getSample();
    dbgassert(sample->nSamples == 0 || sample->nChannels == sample->samples.size());
    log_printf("saveSampleToBuffer %s id %d len %zd\n", StringAsCStr(file.name), file.id, sample->nSamples);

    drwav_data_format format;
    format.container = drwav_container_riff;    // drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;  // Any of the DR_WAVE_FORMAT_* codes.
    format.channels = sample->nChannels;
    format.sampleRate = sample->sampleRate;
    format.bitsPerSample = 32;

    size_t dataSize = 0;
    void* pData = nullptr;
    drwav wav{};/* , &format */
    if (!drwav_init_memory_write_sequential_pcm_frames(&wav, &pData, &dataSize, &format, sample->nSamples, nullptr)) {
        onError("drwav_init_memory returned NULL", file.path);
        return ARCHIVE_FATAL;
    }

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
    static_assert(sizeof(drwav_uint64) == 8, "drwav_uint64 is not 64 bits");
    auto toWrite = sample->nSamples;
    auto samplesWritten = samplecount_t(drwav_write_pcm_frames(&wav, toWrite, blockFull.buf[0]));
    struct free_wave_buffer {
        void* pData;
        ~free_wave_buffer() { drwav_free(pData, nullptr); }
    } freeWaveBuffer{pData};
    if (samplesWritten != toWrite || !pData) {
        onError(StringFormat("drwav_write: Only %zu/%zu samples written\n", samplesWritten, toWrite), StringAsCStr(file.name));
        return ARCHIVE_FATAL;
    }

    archive_entry_set_size(entry, dataSize);
    auto ret = archive_write_header(ar, entry);
    if (ARCHIVE_OK != ret) {
        onError("Failed to write archive header", file.name);
        return ret;
    }
    if (pData && dataSize) {
        auto bytesWritten = archive_write_data(ar, pData, dataSize);
        if (bytesWritten != int64_t(dataSize)) {
            onError(StringFormat("dataSize: Only %zd/%zu bytes written", bytesWritten, dataSize), file.name);
            return ARCHIVE_FATAL;
        }
    }
    return ARCHIVE_OK;
}
samplecount_t saveSampleToFile(audiofile_t& file, const String& fOutWave) {
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
    log_printf("saveSampleToFile %d %s len %zd\n", file.id, StringAsCStr(fOutWave), sample->nSamples);

    drwav_data_format format;
    format.container = drwav_container_riff;    // drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;  // Any of the DR_WAVE_FORMAT_* codes.
    format.channels = sample->nChannels;
    format.sampleRate = sample->sampleRate;
    format.bitsPerSample = 32;
    drwav wav;
    if (!drwav_init_file_write_sequential_pcm_frames(&wav, StringAsCStr(fOutWave), &format, sample->nSamples, nullptr)) {
        log_lf(Log::L_WARN, "drwav_init_file_write_sequential_pcm_frames failed\n");
        return 0;
    }
    struct close_wave_file_write {
        drwav* wav;
        ~close_wave_file_write() { drwav_uninit(wav); }
    } closeWaveFile{&wav};

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

    auto toWrite = sample->nSamples;
    auto samplesWritten = samplecount_t(drwav_write_pcm_frames(&wav, toWrite, blockFull.buf[0]));
    if (samplesWritten != toWrite) {
        log_lf(Log::L_WARN, "Failed writing %s. Only %zd/%zd samples written\n", StringAsCStr(fOutWave), samplesWritten, toWrite);
    }

    return samplecount_t(samplesWritten);
}
void audiocache::saveSamples(const std::vector<int32_t>& refSampleIds) {
    for (auto& w : list) {
        auto* ptr = w.get();
        if (std::binary_search(refSampleIds.cbegin(), refSampleIds.cend(), ptr->id)
            && (ptr->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADED) 
            && (ptr->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_MODIFIED)
            && !(ptr->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_TEMPORARY)) {
            if (FileExists(ptr->path)) {
                log_lf(Log::L_WARN, "Overwriting sample %s\n", StringAsCStr(ptr->path));
            }
            saveSampleToFile(*ptr, ptr->path);
            ptr->pathLoaded = ptr->path;
            ptr->state &= ~audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_MODIFIED;
        }
    }
}
void audiocache::rellocateSamples(const std::vector<int32_t>& refSampleIds, const String& directory) {
    auto& targetDirectory = directory;
    for (auto& f : list) {
        auto* file = f.get();
        if (file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_TEMPORARY) {
            continue;
        }
        if (std::binary_search(refSampleIds.cbegin(), refSampleIds.cend(), file->id)) {
            String oldPath,name,fileExt,nameExt;
            SplitPath(file->path, &oldPath, &name, &fileExt, &nameExt);
                    
            String uniqueName = name;
            String uniquePath = targetDirectory;
            uniquePath += FILE_PATHSEP_CHAR;
            uniquePath += "samples";
            uniquePath += FILE_PATHSEP_CHAR;
            uniquePath += name;
            uniquePath += ".";
            uniquePath += fileExt;
            int32_t idx = 0;
            while ((FileExists(uniquePath) || getByFilename(uniquePath) != nullptr) && ++idx < 10000) {
                idx++;
                uniqueName = name;
                uniqueName += "_";
                uniqueName += std::to_string(idx);

                uniquePath = targetDirectory;
                uniquePath += FILE_PATHSEP_CHAR;
                uniquePath += "samples";
                uniquePath += FILE_PATHSEP_CHAR;
                uniquePath += uniqueName;
                uniquePath += ".";
                uniquePath += fileExt;
            }
            const String newPath = String("samples") + FILE_PATHSEP_STR + uniqueName + "." + fileExt;
            if (file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADED) {
                saveSampleToFile(*file, uniquePath);
                file->name = uniqueName;
                file->path = newPath;
                file->pathLoaded = file->path;
            } else if (file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_BUNDLED) {
                //TODO: copy bundled sample
            } else if (FileExists(file->pathLoaded)) {
                String pathOld = file->path;
                try {
                    std::vector<uint8_t> data;
                    ReadFileVector(pathOld, data);
                    WriteFileVector(uniquePath, data);
                    file->name = uniqueName;
                    file->path = newPath;
                    file->pathLoaded = file->path;
                } catch (const std::exception& e) {
                    log_printf("failed copying sample %s to %s: exception: %s\n", StringAsCStr(pathOld), StringAsCStr(file->path), e.what());
                }
            } else {
                log_lf(Log::L_WARN, "Sample %s not found\n", StringAsCStr(file->path));
                continue;
            }
        }
    }
}

int audiocache::writeToArchive( const std::vector<int32_t>& refSampleIds,
                                struct archive* ar,
                                std::function<void(const String&, int32_t, int32_t)>& onProgress,
                                std::function<void(const String& msg, const String& file)>& onError) {
    String targetDirectory = "";
    auto countTotal = CtrSize(refSampleIds);
    auto nWritten = countTotal * 0;
    for (auto& f : list) {
        auto* file = f.get();
        if (file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_TEMPORARY) {
            continue;
        }
        if (file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_DERIVED) {
            continue;
        }
        if (std::binary_search(refSampleIds.cbegin(), refSampleIds.cend(), file->id)) {
            String origPath = file->pathLoaded;
            if (!(file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_BUNDLED)) {
                String oldPath,name,fileExt,nameExt;
                SplitPath(file->path, &oldPath, &name, &fileExt, &nameExt);
                        
                String uniqueName = name;
                String uniquePath = targetDirectory;
                uniquePath += FILE_PATHSEP_FORWARD_STR;
                uniquePath += "samples";
                uniquePath += FILE_PATHSEP_FORWARD_STR;
                uniquePath += name;
                uniquePath += ".";
                uniquePath += fileExt;
                int32_t idx = 0;
                while ((getByFilename(uniquePath) != nullptr) && ++idx < 10000) {
                    idx++;
                    uniqueName = name;
                    uniqueName += "_";
                    uniqueName += std::to_string(idx);

                    uniquePath = targetDirectory;
                    uniquePath += FILE_PATHSEP_FORWARD_STR;
                    uniquePath += "samples";
                    uniquePath += FILE_PATHSEP_FORWARD_STR;
                    uniquePath += uniqueName;
                    uniquePath += ".";
                    uniquePath += fileExt;
                }
                file->name = uniqueName;
                file->path = String("samples") + FILE_PATHSEP_FORWARD_STR + uniqueName + "." + fileExt;
                file->state |= audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_BUNDLED;
            }

            struct archive_entry* entry = archive_entry_new();
            if (!entry) {
                onError("Failed to create archive entry", file->path);
                return ARCHIVE_FAILED;
            }
            archive_entry_set_pathname(entry, StringAsCStr(file->path));
            archive_entry_set_mtime(entry, time(nullptr), 0);
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            if (file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADED) {
                //TODO: load sample if not loaded!
                auto ret = saveSampleToArchive(*file, entry, ar, onProgress, onError);
                if (ARCHIVE_OK != ret) {
                    return ret;
                }
            } else if (!origPath.empty() && FileExists(origPath)) {
                try {
                    std::vector<uint8_t> buffer;
                    ReadFileVector(origPath, buffer);
                    int64_t signedSize = static_cast<int64_t>(buffer.size());
                    archive_entry_set_size(entry, signedSize);
                    auto ret = archive_write_header(ar, entry);
                    if (ARCHIVE_OK != ret) {
                        onError("Failed to write archive header", file->path);
                        return ret;
                    }
                    auto sizeWritten = archive_write_data(ar, buffer.data(), signedSize);
                    if (sizeWritten != signedSize) {
                        onError("Failed to write archive data", file->path);
                        return ret;
                    }
                } catch (const std::exception& e) {
                    log_printf("failed copying sample %s to %s: exception: %s\n", StringAsCStr(origPath), StringAsCStr(file->path), e.what());
                }
            } else {
                log_lf(Log::L_WARN, "Sample %s not found\n", StringAsCStr(file->path));
            }
            archive_entry_free(entry);
            onProgress(file->path, nWritten, countTotal);
            nWritten++;
        }
    }
    return ARCHIVE_OK;
}
void audiocache::store(const std::vector<int32_t>& refSampleIds, samplefile_index_t& v) {
    v.list.reserve(list.size());
    for (auto& file : list) {
        if (file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_TEMPORARY) {
            continue;
        }
        if (file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_DERIVED) {
            continue;
        }
        if (std::binary_search(refSampleIds.cbegin(), refSampleIds.cend(), file->id)) {
            v.list.push_back({ file->id, file->path });
        }
    }
}
void audiocache::unloadUnreferenced(const std::vector<int32_t>& refSampleIds) {
    auto it = list.begin();
    while (it != list.end()) {
        auto* ptr = it->get();
        if (!std::binary_search(refSampleIds.cbegin(), refSampleIds.cend(), ptr->id)) {
            auto derivedSample = std::find_if(mapId.cbegin(), mapId.cend(), [ptr](const auto& p) {
                const auto* f = p.second;
                return f->derivedFromId == ptr->id;
            });
            if (derivedSample != mapId.cend()) {
                ++it;
                continue;
            }
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

void audiocache::load(samplefile_index_t& sampleIndex, ProjectFileType projectFileType, const String& bundlePath, const String& workingDir) {
    unloadAll();
    struct archive* ar = nullptr;
    if (projectFileType == PROJECT_FILETYPE_BUNDLE && !bundlePath.empty()) {
        ar = archive_read_new();
        archive_read_support_filter_all(ar);
        archive_read_support_format_all(ar);
        archive_read_open_filename(ar, StringAsCStr(bundlePath), 10240);
    }
    DAW::samplefile_index_incremental_loader_t loader(this, ar, sampleIndex, workingDir);
    while (!loader.isFinished()) {
        loader.loadSingleStep();
    }
}

bool audiocache::isEmpty() const {
    return list.empty() && mapId.empty();
}


audiofile_t* audiocache::getDerivedSample(clip_audio_t& clipAudio, std::atomic<bool>* abortFlag) {
    if (clipAudio.isEmpty()) {
        return nullptr;
    }
    if (clipAudio.settings.pitch == 0.0f && clipAudio.settings.stretch == 1.0f) {
        return getSample(clipAudio.id);
    }
    if (clipAudio.idDerived == -1) {
        for (auto& w : list) {
            if (w->derivedFromId == clipAudio.id && w->settings == clipAudio.settings) {
                clipAudio.idDerived = w->id;
                return w.get();
            }
        }
        auto it = mapId.find(clipAudio.id);
        if (it == mapId.end()) {
            log_lf(Log::L_WARN, "getDerivedSample: sample %d not found\n", clipAudio.id);
            clipAudio.idDerived = -2;
            return nullptr;
        }
        auto audiofile = it->second;
        if (!(audiofile->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADED)) {
            log_lf(Log::L_WARN, "getDerivedSample: sample %s not loaded\n", audiofile->path.c_str());
            clipAudio.idDerived = -2;
            return nullptr;
        }
        const auto& sourceSample = audiofile->sample;
        const auto MAX_SAMPLES_INPUT = 1024 * 1024 * 4;
        auto stretchFactor = math::max(double(clipAudio.settings.stretch), 0.05);
        if (sourceSample->nSamples > MAX_SAMPLES_INPUT || sourceSample->nSamples * stretchFactor > MAX_SAMPLES_INPUT) {
            log_lf(Log::L_WARN, "getDerivedSample: sample %s too large\n", audiofile->path.c_str());
            clipAudio.idDerived = -3;
            return nullptr;
        }
        samplecount_t numSamplesStretched = math::ceildS64(double(sourceSample->nSamples) * stretchFactor);
        create_sample_req_t ssr {
            .format = sampleformat_t{samplerate, 512, sampleformat_bits_t::FLOAT_32},
            .numChannels = sourceSample->nChannels,
            .isTemporarySample = true,
            .path = audiofile->path,
            .id = -1,
            .preAllocate = numSamplesStretched,
        };
        auto derivedFile = createSample(ssr);
        if (!derivedFile) {
            log_lf(Log::L_WARN, "Failed to create derived sample for %s\n", audiofile->path.c_str());
            return nullptr;
        }
        derivedFile->derivedFromId = audiofile->id;
        derivedFile->settings = clipAudio.settings;
        derivedFile->state |= audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_DERIVED;
        derivedFile->state |= audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADING;
        auto derivedSample = derivedFile->getSample();
        derivedSample->nSamples = numSamplesStretched;
        derivedSample->samples.resize(derivedSample->nChannels);
        for (channelnum_t c = 0; c < derivedSample->nChannels; c++) {
            auto& ch = derivedSample->samples[c];
            ch.resize(derivedSample->nSamples);
        }


        signalsmith::stretch::SignalsmithStretch<float> stretch;
        stretch.presetDefault(sourceSample->nChannels, sourceSample->sampleRate);
        auto pitchSemitones = clipAudio.settings.pitch;
        auto pitchLinear = std::pow(2.0, pitchSemitones / 12.0);
        pitchLinear = math::clamp(pitchLinear, 1.0/32.0, 32.0);
        stretch.setTransposeFactor(pitchLinear);
        samplecount_t preRoll = math::ceildS64(stretch.outputLatency() + stretch.inputLatency() * stretchFactor);
        auto blockStretched = AudioBlock(sourceSample->nChannels, numSamplesStretched + preRoll);
        blockStretched.clear();

        samplecount_t chunkSize = 10000;
        samplecount_t outputOffset = 0;
        std::vector<float*> bufIn;
        for (samplecount_t i = 0; i < sourceSample->nSamples; i += chunkSize) {
            auto blockOutOffset = blockStretched.getOffsetBlock(outputOffset);
            bufIn.resize(sourceSample->nChannels);
            for (channelnum_t c = 0; c < sourceSample->nChannels; c++) {
                bufIn[c] = sourceSample->samples[c].data() + i;
            }
            auto chSizeIn = math::min<samplecount_t>(chunkSize, sourceSample->nSamples - i);
            auto chSizeStr = chSizeIn * stretchFactor;
            auto chunkSizeStretched = math::min<samplecount_t>(math::ceildS64(chSizeStr), blockOutOffset.samples);
            stretch.process(bufIn.data(), chSizeIn, blockOutOffset.buf, chunkSizeStretched);
            outputOffset += chunkSizeStretched;
            if (abortFlag && abortFlag->load()) {
                return nullptr;
            }
        }
        if (abortFlag && abortFlag->load()) {
            return nullptr;
        }
        // stretch.process(sourceSample->samples, sourceSample->nSamples, blockStretched.buf, numSamplesStretched);

        // feed preroll num silent samples
        // auto inputSizePreRoll = math::ceildS64(preRoll * (1.0 / stretchFactor));
        // auto blockSilent = AudioBlock(sourceSample->nChannels, inputSizePreRoll);
        // blockSilent.clear();
        // auto blockOutOffset = blockStretched.getOffsetBlock(numSamplesStretched);
        // stretch.process(blockSilent.buf, blockSilent.samples, blockOutOffset.buf, preRoll);

        auto blockOutOffset = blockStretched.getOffsetBlock(numSamplesStretched);
        stretch.flush(blockOutOffset.buf, blockOutOffset.samples);

        auto blockNoPreRoll = blockStretched.getOffsetBlock(preRoll);
        for (channelnum_t c = 0; c < derivedSample->nChannels; c++) {
            auto& ch = derivedSample->samples[c];
            // ch.resize(sample->nSamples);
            std::memcpy(ch.data(), blockNoPreRoll.buf[c], blockNoPreRoll.samples * sizeof(float));
        }

        derivedSample->sampleVersion++;
        Downsample(derivedSample, abortFlag);
        derivedFile->state &= ~audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADING;
        clipAudio.idDerived = derivedFile->id;
    }
    return getSample(clipAudio.idDerived);
}

audiofile_t* audiocache::getDerivedSample(const clip_audio_t& clipAudio) const {
    auto it = mapId.find(clipAudio.idDerived);
    return it != mapId.end() ? it->second : nullptr;
}

bool audiocache::fileloader::isFinished() const {
    if (!error.empty())
        return true;
    if (!file || !file->getSample())
        return true;
    return file->state & audiofile_t::AudioFileStateFlags::AUDIOFILE_FLAG_LOADED;
}

float audiocache::fileloader::getProgress() const {
    auto pSamples = samplecount_t(this->pSamples.size());
    auto samplesTotal = samplecount_t(file->getSample()->nSamples);
    float progressResample = 1.0f;
    float progressSampleRead = 1.0f;
    if (samplesTotal > 0) {
        progressResample = resampleOutputOffset / float(samplesTotal);
    }
    if (pSamples > 0) {
        progressSampleRead = (numSamplesInput * sourceNumChannels) / float(pSamples);
    }
    float progressDownsample = downsampleStep / float(audiosample_t::MAX_DOWNSAMPLE);
    auto total = progressSampleRead * 0.33f + progressResample * 0.33f + progressDownsample * 0.33f + 0.01f;
    return total;
}

samplecount_t audiocache::fileloader::getExpectedNumSamples() const {
    auto perChannel = fileReadSamples / sourceNumChannels;
    auto numSamples = targetSamplerate == sourceSamplerate ? perChannel : static_cast<samplecount_t>(perChannel * targetSamplerate / (double) sourceSamplerate + .5);
    return numSamples;
}
