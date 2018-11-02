#include <stdint.h>
#include <string.h>
#include "../packet.h"
#include "audioblock.h"

int64_t timeFix = 0;
const char* timeString(int64_t stamp, char* bufout) {
	stamp += timeFix;
	thread_local char buffer[256] = {};
	const char* prefix = "";
	if (stamp < 0) {
		stamp = -stamp;
		prefix = "-";
	}
	int millisecs = stamp / 1000;
	int secs = millisecs / 1000;
	millisecs -= secs*1000;
	snprintf(bufout ? bufout : buffer, 256, "%s%d.%03d", prefix, secs, millisecs);
	return bufout ? bufout : buffer;
}

void readPacketToBlock(AudioBlock* block, std::vector<uint8_t>& buf, size_t offset) {
	int32_t blockSamples = block->samples;
	int32_t nSamples = block->channels * blockSamples;
	size_t size = nSamples * sizeof(float);
	assert(buf.size()-offset >= size);
	uint8_t* ptr = buf.data()+offset;
	for (uint32_t i = 0; i < block->channels; i++) {
		float* buf = block->buf[i];
		size_t bufSize = sizeof(float) * blockSamples;
		memcpy(buf, ptr, bufSize);
		ptr += bufSize;
	}
}

void writeBlockToPacket(AudioBlock* block, std::vector<uint8_t>& buf, size_t offset) {
	int32_t blockSamples = block->samples;
	int32_t nSamples = block->channels * blockSamples;
	size_t size = nSamples * sizeof(float);
	buf.resize(offset+size);
	uint8_t* ptr = buf.data()+offset;
	for (uint32_t i = 0; i < block->channels; i++) {
		float* buf = block->buf[i];
		size_t bufSize = sizeof(float) * blockSamples;
		memcpy(ptr, buf, bufSize);
		ptr += bufSize;
	}
//	p.hdr.size = ptr-p.buf.data();
//	p.hdr.type = PCKTYPE_DATA;
	assert(buf.size() > 0 && buf.size() < 1<<16);
}
