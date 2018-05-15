#pragma once
#include <unordered_map>
#include <atomic>
#include <memory>
#include "str_util.h"
#include "audiosample.h"
#include "audiowaveform.h"
struct NVGcontext;
struct cachedaudio_t {
	int32_t id = 0;
	String path;
	std::unique_ptr<audiosample_t> sample;
	std::vector<audioclip_texture_t> waveforms;
};
class audiocache {
	std::atomic<int32_t> nextIdx{0};
	std::vector<std::unique_ptr<cachedaudio_t>> list;
	std::unordered_map<int, cachedaudio_t*> mapId;
public:
	audiocache()
	{
	}
	~audiocache() {
		this->mapId.clear();
		this->list.clear();
	}
	static audiocache* getInstance();
	static void setInstance(std::unique_ptr<audiocache> host);
	static void destroy();
	void getLoaded(std::vector<cachedaudio_t*>& v);
	cachedaudio_t* loadFile(String s);
	cachedaudio_t* get(int32_t i);
};
