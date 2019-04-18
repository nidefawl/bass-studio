#include "audiocache.h"
#include <unordered_map>
#include <atomic>
#include <assert.h>
#include "math/seq_math.h"
#include "str_util.h"
#include "audiosample.h"
#include "../wave/dr_wav.h"
#include "../gui/drawwaveform.h"
#include "fileio.h"
#include "logging.h"
#include <soxr.h>

void audiocache::getLoaded(std::vector<cachedaudio_t*>& v) {
	v.reserve(list.size());
	for (auto& w : list) {
		v.push_back(w.get());
	}
}
cachedaudio_t* audiocache::get(int32_t i) {
	size_t count = this->mapId.count(i);
	return count ? this->mapId.at(i) : nullptr;
}
void audiocache::setSamplerate(int32_t samplerate) {
	this->samplerate = samplerate;
}
cachedaudio_t* audiocache::loadFile(String path, int id) {
	drwav wav;
	//satinize path so comparison matches, or ask os if path equals a file we already loaded before

	for (auto& w : list) {
		if (w.get()->path == path)
			return w.get();
	}
	if (drwav_init_file(&wav, StringAsCStr(path))) {
		my_printf("%s\n", StringAsCStr(path));
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
		sample->nChannels = wav.channels;
		sample->sampleRate = wav.sampleRate;
		sample->nSamples = 0;
		std::vector<samplechannel_t> loadedSampleChannels;
		uint64_t numSamplesInput = 0;
		for (int i = 0; i < wav.channels; i++) {
			samplechannel_t channel(wav.totalSampleCount/wav.channels);
			float* out = channel.data();
			for (unsigned j = i; j < wav.totalSampleCount; j+= wav.channels) {
				*out = pSamples[j];
				out++;
			}
			numSamplesInput = i == 0 ? channel.size() : math::min(numSamplesInput, channel.size());
			loadedSampleChannels.push_back(std::move(channel));
		}
		if ((int32_t)wav.sampleRate != this->samplerate) {

			std::vector<samplechannel_t> resampledChannels;
			std::vector<float*> channelPtrsOut(wav.channels);
			std::vector<float*> channelPtrsIn(wav.channels);
			size_t numSamplesResampled = (size_t) (numSamplesInput * this->samplerate / (double)wav.sampleRate + .5); /* Assay output len. */

			for (int i = 0; i < wav.channels; i++) {
				channelPtrsIn[i] = loadedSampleChannels[i].data();
				samplechannel_t channel(numSamplesResampled);
				resampledChannels.push_back(std::move(channel));
				channelPtrsOut[i] = resampledChannels[i].data();
			}


//			my_printf("soxr_oneshot from %d to %d, samples %d -> %d, channels %d\n", wav.sampleRate, this->samplerate, wav.totalSampleCount, olen, wav.channels);
//			my_printf("pSamples.size %d\n", pSamples.size());
//			my_printf("pSamples2.size %d\n", pSamples2.size());

			soxr_quality_spec_t q_spec = soxr_quality_spec(0, 0);
			soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_S, SOXR_FLOAT32_S);
			soxr_runtime_spec_t const runtime_spec = soxr_runtime_spec(0);

			soxr_error_t error = 0;
			soxr_t soxr;
			size_t offset = 0;
			soxr = soxr_create(wav.sampleRate, this->samplerate, wav.channels, &error, &io_spec, &q_spec, &runtime_spec);
			if (!!error) {
				my_printf("soxr_create failed: %d %s\n", error, soxr_strerror(error));
			} else {
				error = soxr_process(soxr,
						channelPtrsIn.data(),numSamplesInput, NULL, channelPtrsOut.data(), numSamplesResampled, &offset);
				my_printf("offset %d, pSamples.size: %d\n", offset, pSamples.size());


				my_printf("soxr_process post %d\n", error);
				if (!!error) {
					my_printf("soxr_process failed: %d %s\n", error, soxr_strerror(error));
				} else {
					sample->nSamples = offset;
					sample->sampleRate = this->samplerate;
					sample->samples.resize(sample->nChannels);
					for (int i = 0; i < sample->nChannels; i++) {
						sample->samples[i] = std::move(resampledChannels[i]);
					}
				}
			}
			my_printf("%-26s\n", soxr_strerror(error));
			soxr_delete(soxr);
		} else {
			sample->nSamples = numSamplesInput;
			sample->sampleRate = this->samplerate;
			sample->samples.resize(sample->nChannels);
			for (int i = 0; i < sample->nChannels; i++) {
				sample->samples[i] = std::move(loadedSampleChannels[i]);
			}
		}
		my_printf("copy done: %d\n", nSamples);
		int maxDownS = 1;
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
//		int nDownSmplSteps = maxDownS-1;
//		assert(sample->downsampled.size() == nDownSmplSteps);
		int _id = id < 0 ? this->nextIdx++ : id;
		std::unique_ptr<cachedaudio_t> cachedaudio = std::make_unique<cachedaudio_t>();
		cachedaudio->sample = std::move(sample);
		cachedaudio->id = _id;
		cachedaudio->path = path;
		String a,b,c, d;
		SplitPath(path, &a, &b, &c, &d);
		cachedaudio->name = b;
		cachedaudio->ext = c;
		this->mapId[_id] = cachedaudio.get();
		my_printf("%d\n", cachedaudio->id);
		cachedaudio_t* audio = cachedaudio.get();
		list.push_back(std::move(cachedaudio));
		assert(mapId[_id] == audio);
		return audio;
	}
	return NULL;
}
void audiocache::store(samplefile_index_t& v) {
	v.list.reserve(list.size());
	for (auto& w : list) {
		samplefile_index_t index;
		auto* ptr = w.get();
		v.list.push_back({ptr->id, ptr->path});
	}
}
void audiocache::load(samplefile_index_t& v) {
	list.reserve(v.list.size());
	for (auto& w : v.list) {
		loadFile(w.name, w.id);
	}
}
