#pragma once
#include <unordered_map>
#include <atomic>
#include <memory>
#include "str_util.h"
#include "audiosample.h"
#include "audiowaveform.h"
#include "samplefileidx.h"

struct NVGcontext;
struct cachedaudio_t {
	int32_t id = 0;
	String path;
	String name;
	String ext;
	std::unique_ptr<audiosample_t> sample;
};
class audiocache {
	int32_t samplerate = 0;
	std::atomic<int32_t> nextIdx{0};
	std::vector<std::unique_ptr<cachedaudio_t>> list;
	std::unordered_map<int, cachedaudio_t*> mapId;
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
	static void setInstance(std::unique_ptr<audiocache> host);
	static void destroy();
	void getLoaded(std::vector<cachedaudio_t*>& v);
	cachedaudio_t* loadFile(String s, int id = -1);
	void setSamplerate(int32_t samplerate);
	cachedaudio_t* get(int32_t i);
	void store(samplefile_index_t& v);
	void load(samplefile_index_t& v);
};
