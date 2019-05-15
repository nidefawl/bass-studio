#pragma once
#include <stdint.h>
#include <vector>
#include <memory>
#include "assert_dbg.h"
#include <atomic>
#include <algorithm>
#include "logging.h"
#include "platform.h"
#include "str_util.h"

#pragma pack(push, 1)
struct header_t {
	int32_t type = 0;
	int32_t size = 0; // size without header
	int64_t timestamp;
};
struct packet_t {
	header_t hdr;
	std::vector<uint8_t> buf;
	packet_t(int32_t _type = 0) : hdr{_type, 0, 0} {
	}
};
#pragma pack(pop)

extern int64_t timeFix;
template<typename T>
void sendPacket(T& conn, packet_t& p) {
	dbgassert ((size_t)p.hdr.size == p.buf.size());
	p.hdr.timestamp = getTimeHPint64()+timeFix;
	conn->write(&p.hdr, sizeof(header_t));
	conn->write(p.buf.data(), p.hdr.size);
}
template<typename T>
void serializeStruct(T& data, packet_t& p, size_t offset) {
	auto& buf = p.buf;
	if (buf.size() < sizeof(T)+offset) {
		buf.resize(sizeof(T)+offset-buf.size());
	}
	void* ptr = buf.data()+offset;
	memcpy(ptr, &data, sizeof(T));
	p.hdr.size = buf.size();
}
template<typename T>
void deserializeStruct(T& data, packet_t& p, size_t offset) {
	auto& buf = p.buf;
	dbgassert(buf.size() >= sizeof(T)+offset);
	void* ptr = buf.data()+offset;
	memcpy(&data, ptr, sizeof(T));
}

struct packetreader_t {
	std::vector<uint8_t> buf;
	bool hasHeader = false;
	packet_t readPacket;
	std::vector<packet_t> packets;
	packetreader_t() {
	}
	~packetreader_t() {
	}
	int maxStateSeen = 0;
	int recvData(const void* data, size_t size) {
		buf.resize(buf.size()+size);
		dbgassert(buf.data() != nullptr);
		uint8_t* ptrData = buf.data()+buf.size()-size;
		memcpy(ptrData, data, size);
		while (1) {
			if (!hasHeader) {
				if (buf.size() < sizeof(header_t)) {
					break;
				}
				memcpy(&readPacket.hdr, buf.data(), sizeof(header_t));
				buf.erase(buf.begin(), buf.begin()+sizeof(header_t));
				hasHeader = true;
//				dbgassert(readPacket.hdr.size >= 0 && readPacket.hdr.size < (1<<16));
				if (readPacket.hdr.size < 0 || readPacket.hdr.size >= (1<<16)) {
					printf("received invalid packet header\n");
					return -1;
				}
				readPacket.hdr.timestamp-=timeFix;
				readPacket.buf.clear();
				maxStateSeen = std::max<int>(readPacket.hdr.type, maxStateSeen);
			}
			if (hasHeader) {
				size_t dataSize = readPacket.hdr.size;
				if (dataSize) {
					if (buf.size() < dataSize) {
						break;
					}
					readPacket.buf.resize(dataSize);
					memcpy(readPacket.buf.data(), buf.data(), dataSize);
					buf.erase(buf.begin(), buf.begin() + dataSize);
				}
				packets.push_back(readPacket);
				hasHeader = false;
			}
		}
		return 0;
	}
};
