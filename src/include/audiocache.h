#pragma once
#include <unordered_map>
#include <atomic>
#include <memory>
#include "str_util.h"
#include "audiosample.h"
#include "audiowaveform.h"
#include "samplefileidx.h"

struct NVGcontext;
struct audiofile_path_t  {
	int32_t id = 0;
	String path;
};
struct audiofile_t : public samplesource_t {
	int32_t id = 0;
	String path;
	String name;
	String ext;
	std::unique_ptr<audiosample_t> sample;
	audiosample_t* getSample() override {
		return sample.get();
	}
	audiofile_path_t getPath() {
		return {id, path};
	}
};
class audiocache {
	int32_t samplerate = 0;
	std::atomic<int32_t> nextIdx{0};
	std::vector<std::unique_ptr<audiofile_t>> list;
	std::unordered_map<int, audiofile_t*> mapId;
public:
	audiocache(int32_t _samplerate)
	{
		setSamplerate(_samplerate);
	}
	~audiocache() {
		this->mapId.clear();
		this->list.clear();
	}
	static audiocache* getInstance();
	void getLoaded(std::vector<audiofile_t*>& v);
	audiofile_t* loadFile(String s, int id = -1);
	void setSamplerate(int32_t samplerate);
	void unloadSampleId(int32_t id);
	audiofile_t* get(int32_t i);
	void store(samplefile_index_t& v);
	void load(samplefile_index_t& v);
};
