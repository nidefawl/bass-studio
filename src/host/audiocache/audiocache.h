#pragma once
#include <unordered_map>
#include <atomic>
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include "host/clip/clip.h"
#include "host/project/project.h"
#include "types.h"
#include "str_util.h"
#include "host/audiosample.h"
#include "wave/waveform_render.h"
#include "samplefileidx.h"

#include <dr_libs/dr_wav.h>
#include <dr_libs/dr_mp3.h>
#include <dr_libs/dr_flac.h>

struct drflac_file {
    drflac* pFlac = nullptr;
};

using audiofile_variant = std::variant<std::monostate, drwav, drmp3, drflac_file>;

struct archive;
struct NVGcontext;
class clip_audio_t;

struct audiofile_path_t {
    int32_t id = 0;
    String path;
};

struct audiofile_t final : public samplesource_t {
    enum AudioFileStateFlags : uint8_t {
        AUDIOFILE_FLAGS_NONE = 0,
        AUDIOFILE_FLAG_LOADED = 1 << 0,
        AUDIOFILE_FLAG_MODIFIED = 1 << 1,
        AUDIOFILE_FLAG_MISSING = 1 << 2,
        AUDIOFILE_FLAG_TEMPORARY = 1 << 3,
        AUDIOFILE_FLAG_BUNDLED = 1 << 4,
        AUDIOFILE_FLAG_DERIVED = 1 << 5,
    };
    int32_t id = -1;
    int32_t derivedFromId = -1;
    String path;
    String name;
    String ext;
    String bundlePath;
    uint8_t state = AUDIOFILE_FLAGS_NONE;
    String pathLoaded;
    std::unique_ptr<audiosample_t> sample;
    clip_audio_settings_t settings;
    audiosample_t* getSample() override {
        return sample.get();
    }
    audiofile_path_t getPath() {
        return { id, path };
    }
    audiofile_path_t getPathLoaded() {
        return { id, pathLoaded };
    }
};
struct store_sample_req_t {
    int32_t id = -1;
    sampleformat_t format{};
    samplecount_t offset = 0;
    samplecount_t length = 0;
    samplecount_t preAllocate = 0;
    std::vector<samplechannel_t> channels;
    bool bDownsample = false;
};
struct create_sample_req_t {
    sampleformat_t format{};
    channelnum_t numChannels = 0;
    bool isTemporarySample = false;
    String path = "";
    int32_t id = -1;
    samplecount_t preAllocate = 0;
};
struct create_derived_sample_req_t {
};
class audiocache {
    samplerate_t samplerate = 0;
    std::atomic<int32_t> nextIdx{ 0 };
    std::vector<std::shared_ptr<audiofile_t>> list;
    std::unordered_map<int, audiofile_t*> mapId;

public:
    static constexpr uint8_t maxDownS = 8;
    static void Downsample(audiosample_t* sample);
    class fileloader {
        static const size_t chunkSize = 1024 * 256;
        std::shared_ptr<audiofile_t> file;
        std::optional<audiofile_variant> audiofileVariant;
        String error;
        samplerate_t sourceSamplerate = 0;
        samplerate_t targetSamplerate = 0;
        channelnum_t sourceNumChannels = 0;
        std::vector<float> pSamples;
        samplecount_t numSamplesInput = 0;
        std::vector<uint8_t> heapBuffer;
        bool bReadComplete = false;
        bool bResampleComplete = false;
        samplecount_t resampleInputOffset = 0;
        samplecount_t resampleOutputOffset = 0;
        void* soxrContext = nullptr;
    public:
        fileloader() = default;
        bool resolveFile(const String& pathIn, const String& workingDir, bool remapPath);
        bool preloadFile(struct archive* ar, struct archive_entry* entry);
        bool loadFileIncremental();
        float getProgress() const;
        bool resample();
        const String& getError() const {
            return error;
        }
        std::shared_ptr<audiofile_t>& getSPFile() {
            return file;
        }
        audiofile_t* getFile() {
            dbgassert(file.get());
            return file.get();
        }
        bool isFinished() const;
        bool isOk() const {
            return error.empty();
        }
        samplerate_t getSourceSampleRate() const {
            return sourceSamplerate;
        }
        void setTargetSampleRate(samplerate_t samplerate) {
            targetSamplerate = samplerate;
        }
        samplerate_t getTargetSampleRate() const {
            return targetSamplerate;
        }
        samplecount_t getExpectedNumSamples() const;
    };
    explicit audiocache(samplerate_t _samplerate) {
        setSamplerate(_samplerate);
    }
    ~audiocache() {
        this->mapId.clear();
        this->list.clear();
    }
    static audiocache* getInstance();
    void addFile(std::shared_ptr<audiofile_t>& af);
    void updateSample(const store_sample_req_t& ssr);
    audiofile_t* createSample(const create_sample_req_t& ssr);
    void setSamplerate(samplerate_t samplerate);
    samplerate_t getSampleRate() const {
        return samplerate;
    }
    int32_t getUniqueSampleId() {
        return nextIdx.fetch_add(1);
    }
    void unloadSampleId(int32_t id);
    audiofile_t* getSample(int32_t i);
    audiofile_t* getDerivedSample(clip_audio_t& clipAudio);
    audiofile_t* getDerivedSample(const clip_audio_t& clipAudio) const;
    audiofile_t* getByFilename(const String& pathFile);
    void store(const std::vector<int32_t>& refSampleIds, samplefile_index_t& v);
    void load(samplefile_index_t& v, ProjectFileType projectFileType, const String& bundlePath, const String& workingDir);
    void saveSamples(const std::vector<int32_t>& refSampleIds);
    void unloadAll();
    void unloadUnreferenced(const std::vector<int32_t>& refSampleIds);
    void rellocateSamples(const std::vector<int32_t>& refSampleIds, const String& directory);
    int writeToArchive( const std::vector<int32_t>& refSampleIds,
                        struct archive* ar,
                        std::function<void(const String&, int32_t, int32_t)>& onProgress,
                        std::function<void(const String& msg, const String& file)>& onError);
    bool isEmpty() const;
    bool loadFile(std::shared_ptr<audiofile_t>& outFile, const String& pathIn, const String& workingDir, bool remapPath = true, struct archive* ar = nullptr, struct archive_entry* entry = nullptr);
};

