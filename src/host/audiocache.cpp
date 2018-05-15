#include "audiocache.h"
#include <unordered_map>
#include <atomic>
#include <assert.h>
#include "str_util.h"
#include "audiosample.h"
#include "../wave/dr_wav.h"
#include "logging.h"
#include "../gui/drawwaveform.h"

namespace
{
	std::unique_ptr<audiocache> g_instance;
}

void audiocache::destroy() {
	g_instance.reset();
}
audiocache* audiocache::getInstance()
{
	return g_instance.get();
}
void audiocache::setInstance(std::unique_ptr<audiocache> host)
{
	g_instance = std::move(host);
}
void audiocache::getLoaded(std::vector<cachedaudio_t*>& v) {
	v.reserve(list.size());
	for (auto& w : list) {
		v.push_back(w.get());
	}
}
cachedaudio_t* audiocache::get(int32_t i) {
	return this->mapId.at(i);
}
cachedaudio_t* audiocache::loadFile(String path) {
	drwav wav;
	if (drwav_init_file(&wav, StringAsCStr(path))) {
		std::vector<float> pSamples(wav.totalSampleCount);
		memset(pSamples.data(), 0, sizeof(float)*pSamples.size());
		size_t nSamples = drwav_read_f32(&wav, wav.totalSampleCount, pSamples.data());

		my_printf("totalSampleCount: %d\n", wav.totalSampleCount);
		my_printf("channels: %d\n", wav.fmt.channels);
		my_printf("sampleRate: %d\n", wav.fmt.sampleRate);
		my_printf("bitsPerSample: %d\n", wav.fmt.bitsPerSample);
		my_printf("samples: %d\n", nSamples);

		std::unique_ptr<audiosample_t> sample = std::make_unique<audiosample_t>();
		sample->bitsPerSample = wav.bitsPerSample;
		sample->sampleRate = wav.sampleRate;
		sample->nChannels = wav.channels;
		sample->nSamples = wav.totalSampleCount/wav.channels;
		int nSamplesAllChannels = 0;
		for (int i = 0; i < wav.channels; i++) {
			samplechannel_t channel(wav.totalSampleCount/wav.channels);
			float* out = channel.data();
			for (int j = 0; j < wav.totalSampleCount; j+= wav.channels) {
				*out = pSamples[j];
				out++;
			}
			sample->samples.push_back(std::move(channel));
			nSamplesAllChannels += sample->samples.back().size();
			assert(sample->samples.back().size()==sample->nSamples);
		}
		my_printf("copy done: %d\n", nSamples);
		int maxDownS = 7;
		for (int step = 1; step < maxDownS; step++) {
			std::vector<samplechannel_t> downsampledChannels(2);
			for (int i = 0; i < wav.channels; i++) {
				size_t len = sample->nSamples>>step;
				samplechannel_t chDownSmpld(len);
				downsample(sample->sampleRate,
						sample->samples.at(i).data(),
						sample->nSamples,
						chDownSmpld, step);
				downsampledChannels[i] = std::move(chDownSmpld);
			}
			sample->downsampled.push_back(std::move(downsampledChannels));
		}
		my_printf("downsample done: %d\n", nSamples);
		int nDownSmplSteps = maxDownS-1;
		assert(sample->downsampled.size() == nDownSmplSteps);
		assert(sample->nSamples*sample->nChannels==nSamplesAllChannels);
		int id = this->nextIdx++;
		std::unique_ptr<cachedaudio_t> cachedaudio = std::make_unique<cachedaudio_t>();
		cachedaudio->sample = std::move(sample);
		cachedaudio->id = id;
		cachedaudio->path = path;
		cachedaudio_t* audio = cachedaudio.get();
		list.push_back(std::move(cachedaudio));
		this->mapId[id] = audio;
		return audio;
	}
	return NULL;
}
