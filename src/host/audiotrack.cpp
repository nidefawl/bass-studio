#include "audiotrack.h"
#include "audiosample.h"
#include "audioblock.h"
#include "logging.h"
#include "host/vst_host.h"
#include "mainctrl.h"
#include <stdint.h>
#include <memory>

static constexpr int32_t PER_BLOCK_BYTES = (1024*512);
static constexpr int32_t PER_BLOCK_SAMPLES = (PER_BLOCK_BYTES/(sizeof(float)));

std::shared_ptr<audiotrack_split_t> audiotrack_t::getSampleById(int32_t sampleId) {
	//TODO: this lock could be narrowed
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	for (const std::shared_ptr<audiotrack_split_t>& sample : samples) {
		if (sample && sample->sampleId == sampleId)
			return sample;
	}
	return nullptr;
}
std::shared_ptr<audiotrack_split_t> audiotrack_t::getSample(int32_t samplePos) {
	//TODO: this lock could be narrowed
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	int32_t startBlock = (samplePos) / PER_BLOCK_SAMPLES;
	if (samples.size() > startBlock && samples[startBlock]) {
		return samples[startBlock];
	}
	return nullptr;
}
int32_t audiotrack_t::convertToSamples(vsthost* host) {
	auto isInSync = [this]() {
		if (data.size() != this->samples.size()) {
			return false;
		}
		for (int32_t i = 0;i < data.size();i++) {
			if (data[i]) {
				if (!this->samples[i] || this->samples[i]->version != data[i]->version) {
					return false;
				}
			}
		}
		return true;
	};
	if (isInSync())
		return 0;

	std::vector<std::shared_ptr<audiotrack_split_t> > newSplits;
	newSplits.reserve(data.size());
	int32_t nSamples = samples.size();
	int32_t bytesCopied = 0;
	for (int32_t i = 0; i < data.size(); i++) {
		int32_t samplePos = i * PER_BLOCK_SAMPLES;
		if (data[i]) {
			auto& block = data[i]->data;
			audiotrack_split_t* split;
			bool present = false;
			if (i >= nSamples || !this->samples[i]) {
				auto sharedSplit = std::make_shared<audiotrack_split_t>();
				split = sharedSplit.get();
				newSplits.push_back(std::move(sharedSplit));
				split->samplePos = samplePos;
				split->sample.sampleRate = vsthost::getInstance()->sampleFormat.sampleRate;
				split->sampleId = host->getNextSampleId(0);
				split->sample.bitsPerSample = 32;
			} else {
				present = true;
				split = this->samples[i].get();
				newSplits.push_back(this->samples[i]);
				dbgassert(split->samplePos == samplePos);
			}
			split->sample.nChannels = block.channels;
			split->sample.nSamples = block.samples;
			if (split->version == data[i]->version) {
				continue;
			}
			int preVersion = split->version;
			split->version = data[i]->version;
			std::vector<samplechannel_t>& channels = split->sample.samples;
			bool resized = false;
			if (block.channels != channels.size()) {
				channels.resize(block.channels);
				resized = true;
			}
			bool reused = false;
			for (uint32_t j = 0; j < block.channels; j++) {
				float *srcPtr = block.buf[j];
				size_t srcSize = block.samples;
				channels[j].resize(srcSize);
				float* dstPtr = channels[j].data();
				size_t dstSize = channels[j].size();
#ifndef _WIN32
				memcpy(dstPtr, srcPtr, math::min(dstSize*sizeof(float), srcSize*sizeof(float)));
#else
				memcpy_s(dstPtr, dstSize*sizeof(float), srcPtr, srcSize*sizeof(float));
#endif
				bytesCopied += sizeof(float)*srcSize;
			}
//			log_printf("block #%d copy %d bytes, present %d, resized %d, reused %d, version %d/%d\n", i, bytesCopied, present, resized, reused, preVersion, data[i]->version);
		} else {
			newSplits.push_back(nullptr);
		}

	}
	this->samples = std::move(newSplits);
	return bytesCopied;
}

void copyFromToSample(audiosample_t *dstSample, float** srcBuf, uint32_t offsetIn, uint32_t offsetOut, uint32_t srcSamples, uint32_t srcChannels) {
//		dbgassert(srcSamples == samples);
	uint32_t nChannels = math::max(srcChannels, (uint32_t)dstSample->nChannels);
	uint32_t nSamples = math::min(srcSamples, (uint32_t)dstSample->nSamples);
	for (uint32_t i = 0; i < nChannels; i++) {
		uint32_t srcChannelIdx = math::min(srcChannels-1, i);
		uint32_t dstChannelIdx = math::min((uint32_t)dstSample->nChannels-1, i);
		float* srcBufChannel = srcBuf[srcChannelIdx];
		float* dstBufChannel = dstSample->samples[dstChannelIdx].data();
		//TODO: this does 2 copys to the same destination when going from stereo to mono (MIX FIRST)
		memcpy(dstBufChannel+offsetOut, srcBufChannel+offsetIn, nSamples * sizeof(float));
	}
}
void copyBlockChannelsToSample(audiosample_t *dstSample, AudioBlock* input, uint32_t offsetIn, uint32_t offsetOut, uint32_t len) {
	dbgassert(input->channels >= dstSample->nChannels);
	for (uint32_t i = 0; i < dstSample->nChannels; i++) {
		float* srcBufChannel = input->buf[i];
		float* dstBufChannel = dstSample->samples[i].data();
		//TODO: this does 2 copys to the same destination when going from stereo to mono (MIX FIRST)
		memcpy(dstBufChannel+offsetOut, srcBufChannel+offsetIn, len * sizeof(float));
	}
}
void audiotrack_t::store(AudioBlock* input, int32_t samplePos) {
	int32_t startBlock = math::max(0, (samplePos) / PER_BLOCK_SAMPLES);
	int32_t endBlock = math::max(0, (samplePos + (int32_t)input->samples - 1) / PER_BLOCK_SAMPLES);
	while (data.size() <= endBlock) {
		data.push_back(nullptr);
	}
	auto split = std::make_shared<audiotrack_split_t>();
	if (!data[startBlock]) {
//		log_printf("alloc new block #%d\n", startBlock);
		data[startBlock] = std::make_shared<audiotrack_block_t>(OUTPUT_CHANNELS, PER_BLOCK_SAMPLES);
	}
	if (!data[endBlock]) {
//		log_printf("alloc new block #%d\n", endBlock);
		data[endBlock] = std::make_shared<audiotrack_block_t>(OUTPUT_CHANNELS, PER_BLOCK_SAMPLES);
	}
	int32_t readLen = input->samples;
	int32_t readOffset = 0;
	if (samplePos < 0) {
		if (samplePos < -PER_BLOCK_SAMPLES) {
			return;
		}
		readLen = samplePos+input->samples;
		readOffset = -samplePos;
		samplePos = 0;
	}
	int32_t startOffsetBlock0 = samplePos - (startBlock * PER_BLOCK_SAMPLES);
	int32_t lenBlock0 = math::min(PER_BLOCK_SAMPLES - startOffsetBlock0, readLen);
	int32_t lenOver = readLen - lenBlock0;
	auto* blockStart = data[startBlock].get();
//			log_printf("write %d samples to block #%d{%d:%d}\n", lenBlock0, startBlock, startOffsetBlock0, startOffsetBlock0+lenBlock0);
	blockStart->version++;
	blockStart->data.copyFromPosToPos(input->buf, readOffset, startOffsetBlock0, lenBlock0, input->channels);
//	if (samples.size() > startBlock && samples[startBlock]) {
//		samples[startBlock]->version++;
//		copyBlockChannelsToSample(samples[startBlock]->getSample(), input, 0, startOffsetBlock0, lenBlock0);
//	}
	if (startBlock != endBlock) {
		dbgassert(lenOver > 0);
		auto* blockEnd = data[endBlock].get();
//					log_printf("write %d samples to block #%d{%d:%d}\n", lenOver, endBlock, 0, lenOver);
		blockEnd->version++;
		blockEnd->data.copyFromPosToPos(input->buf, lenBlock0, 0, lenOver, input->channels);
//		if (samples.size() > endBlock && samples[endBlock]) {
//			samples[endBlock]->version++;
//			copyBlockChannelsToSample(samples[endBlock]->getSample(), input, lenBlock0, 0, lenOver);
//		}
	} else {
		dbgassert(lenOver == 0);
	}
}
