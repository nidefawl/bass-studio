#pragma once
#include <unordered_map>
#include <atomic>
#include <memory>
#include "types.h"
#include "str_util.h"
#include "audiosample.h"
#include "wave/waveform_render.h"
#include "samplefileidx.h"

struct NVGcontext;
struct audiofile_path_t {
    int32_t id = 0;
    String path;
};
struct audiofile_t : public samplesource_t {
    enum class filestate {
        UNLOADED,
        UNLOADED_MISSING,
        LOADED_MODIFIED,
        LOADED
    };
    int32_t id = 0;
    String path;
    String name;
    String ext;
    filestate state = filestate::UNLOADED;
    std::unique_ptr<audiosample_t> sample;
    audiosample_t* getSample() override {
        return sample.get();
    }
    audiofile_path_t getPath() {
        return { id, path };
    }
};
struct store_sample_req_t {
    int32_t id = -1;
    sampleformat_t format{};
    samplecount_t offset = 0;
    samplecount_t length = 0;
    std::vector<samplechannel_t> channels;
};
struct create_sample_req_t {
    sampleformat_t format{};
    channelnum_t numChannels = 0;
    bool isTemporarySample = false;
    String path = "";
    int32_t id = -1;
};
class audiocache {
    samplerate_t samplerate = 0;
    std::atomic<int32_t> nextIdx{ 0 };
    std::vector<std::unique_ptr<audiofile_t>> list;
    std::unordered_map<int, audiofile_t*> mapId;

public:
    explicit audiocache(samplerate_t _samplerate) {
        setSamplerate(_samplerate);
    }
    ~audiocache() {
        this->mapId.clear();
        this->list.clear();
    }
    static audiocache* getInstance();
    void getLoaded(std::vector<audiofile_t*>& v);
    audiofile_t* loadFile(const String& s, int32_t id = -1);
    void updateSample(const store_sample_req_t& ssr);
    audiofile_t* createSample(const create_sample_req_t& ssr);
    void setSamplerate(samplerate_t samplerate);
    void unloadSampleId(int32_t id);
    audiofile_t* get(int32_t i);
    audiofile_t* getByFilename(const String& pathFile);
    void store(const std::vector<int32_t>& refSampleIds, samplefile_index_t& v);
    void load(samplefile_index_t& v);
    void saveSamples(const std::vector<int32_t>& refSampleIds);
    void unloadAll();
    void unloadUnreferenced(const std::vector<int32_t>& refSampleIds);
    bool isEmpty() const;
};
