#include "audiotrack.h"
#include "audiosample.h"
#include "audioblock.h"
#include "logging.h"
#include "host/vst_host.h"
#include "mainctrl.h"
#include <stdint.h>
#include <memory>

static constexpr int32_t PER_BLOCK_BYTES = (1024*1024*4);
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
			std::shared_ptr<audiotrack_split_t> split;
			bool present = false;
			if (i >= nSamples || !this->samples[i]) {
				split = std::make_shared<audiotrack_split_t>();
				split->samplePos = samplePos;
				split->sample.sampleRate = host->lSampleRate;
				split->sampleId = host->getNextSampleId(0);
				split->sample.bitsPerSample = 32;
			} else {
				present = true;
				split = this->samples[i];
				assert(split->samplePos == samplePos);
			}
			split->sample.nChannels = block.channels;
			split->sample.nSamples = block.samples;
			newSplits.push_back(split);
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
			for (int j = 0; j < block.channels; j++) {
				float *srcPtr = block.buf[j];
				size_t srcSize = block.samples;
				channels[j].resize(srcSize);
				float* dstPtr = channels[j].data();
				size_t dstSize = channels[j].size();
				memcpy_s(dstPtr, dstSize*sizeof(float), srcPtr, srcSize*sizeof(float));
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

void audiotrack_t::store(AudioBlock* input, int32_t samplePos) {
	int32_t startBlock = (samplePos) / PER_BLOCK_SAMPLES;
	int32_t endBlock = (samplePos + input->samples - 1) / PER_BLOCK_SAMPLES;
	while (data.size() <= endBlock) {
		data.push_back(nullptr);
	}
	auto split = std::make_shared<audiotrack_split_t>();
	if (!data[startBlock]) {
		log_printf("alloc new block #%d\n", startBlock);
		data[startBlock] = std::make_shared<audiotrack_block_t>(OUTPUT_CHANNELS, PER_BLOCK_SAMPLES);
	}
	if (!data[endBlock]) {
		log_printf("alloc new block #%d\n", endBlock);
		data[endBlock] = std::make_shared<audiotrack_block_t>(OUTPUT_CHANNELS, PER_BLOCK_SAMPLES);
	}
	int32_t startOffsetBlock0 = samplePos - (startBlock * PER_BLOCK_SAMPLES);
	int32_t lenBlock0 = math::min(PER_BLOCK_SAMPLES - startOffsetBlock0, (int32_t) (input->samples));
	int32_t lenOver = input->samples - lenBlock0;
	auto* blockStart = data[startBlock].get();
//			log_printf("write %d samples to block #%d{%d:%d}\n", lenBlock0, startBlock, startOffsetBlock0, startOffsetBlock0+lenBlock0);
	blockStart->data.copyFromPosToPos(input->buf, 0, startOffsetBlock0, lenBlock0, input->channels);
	blockStart->version++;
	if (startBlock != endBlock) {
		assert(lenOver > 0);
		auto* blockEnd = data[endBlock].get();
//					log_printf("write %d samples to block #%d{%d:%d}\n", lenOver, endBlock, 0, lenOver);

		blockEnd->data.copyFromPosToPos(input->buf, lenBlock0, 0, lenOver, input->channels);
		blockEnd->version++;
	} else {
		assert(lenOver == 0);
	}
}
