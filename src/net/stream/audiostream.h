#pragma once
#include <stdint.h>
#include <vector>

#define STREAM_MODE_IDLE 0
#define STREAM_MODE_SEND 1
#define STREAM_MODE_INSERT 2

#define PCKTYPE_SYNC_CLOCK 1
#define PCKTYPE_AUDIO_CONFIG 2
#define PCKTYPE_START_STREAM 3
#define PCKTYPE_TEST 4
#define PCKTYPE_DATA 5

#define STATE_IDLE 1
#define STATE_STREAMING 2
#define NUM_TEST_PINGS 12

#pragma pack(push, 1)
struct AudioBlockHeader {
	int32_t nonce;
	int64_t timestamp;
};
struct clock_timeinfo_t {
	int64_t hpcTime;
	int64_t hpcDiff;
};
struct stream_timeinfo_t {
	int64_t sampleRate;
	int64_t blockSize;
	int64_t nChannels;
	int64_t hpcTime;
	double streamTime;
	double latencyIn;
	double latencyOut;
};
struct stream_startinfo_t {
	int32_t streamMode;
};
#pragma pack(pop)

struct AudioBlock;
void writeBlockToPacket(AudioBlock* block, std::vector<uint8_t>& p, size_t offset);
void readPacketToBlock(AudioBlock* block, std::vector<uint8_t>& p, size_t offset);
const char* timeString(int64_t stamp, char* bufout = nullptr);
