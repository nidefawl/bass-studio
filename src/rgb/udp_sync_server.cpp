/**
 * Copyright (c) 2018 Michael Hept
 */
#include <memory>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include "basectrl.h"
#include "color_util.h"
#include "platform.h"
#include "threads.h"
#include "assert_dbg.h"
#include "net/network.h"
#include "net/packet.h"
#include "net/stream/audiostream.h"
#include "threads/workerthread.h"
#include "logging.h"
#ifdef _WIN32
#include <io.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include "rgb_network_types.h"
#include "udp_sync_server.h"


#define RGB_PROTOCOL_WRITE_BUF_SIZE (32U*1024U)

struct lamp_udp_sync_context_t {
	uint64_t frameId = 0;
	uint64_t frameStep = 0;
	uint64_t packetId = 0;
};

class rgbprotocol_server_nethandler_server : public inetwork_handler {
public:
	std::vector<uint8_t> dataBuf;

	bool connected = false;
	rgbprotocol_server_nethandler_server() {

	}
	void onError(int errorType, String msg) override {
		log_printf("Error %s\n", StringAsCStr(msg));
		connected = false;
	}
	bool onReceive(void* data, size_t size) override {
		log_printf("onReceive %u\n", size);

		dataBuf.assign((uint8_t*)data, ((uint8_t*)data) + size);
		return true;
	}
	void onConnect(std::shared_ptr<network_conn_t> conn) override {
		conn->parent->setSelectTimeout(0.0001);
		log_printf("connected\n", 0);
		connected = true;
	}
	void onDisconnect(std::shared_ptr<network_conn_t> conn) override {
		log_printf("disconnected\n", 0);
		connected = false;
	}
	virtual bool onAccept(std::shared_ptr<network_conn_t> clientConn) {
		dbgassert(0); // not called
		return false;
	}
};
struct NetServerState {

	std::atomic<bool> isRunning{true};
	std::atomic<bool> isConnected{true};
	std::atomic<bool> shouldQuit{false};
};
struct UDPSyncController {
	struct Impl;
	Impl* const impl;
	UDPSyncController();
	~UDPSyncController();
	bool processPacket(network_conn_t* conn, std::vector<uint8_t>& dataBuf);
	void sendSyncTo(network_conn_t* conn, String host, int port);
};
struct UDPSyncController::Impl {
	std::map<int32_t, lamp_udp_sync_context_t> ctxts;
	uint8_t writeBuf[RGB_PROTOCOL_WRITE_BUF_SIZE];
	int masterFrameStep = 0;
	heartbeat_message pktHeartbeat;
	uint32_t tick;
	uint32_t getCurrentTick() {
		return (uint32_t)(((uint64_t)getTimeHPint64())>>4);
	}
	void sendSyncTo(network_conn_t* conn, String host, int port) {

		sync_message pktSync;
		pktSync.frame = 0;
		packet_hdr_t hdr;
		hdr.len = sizeof(pktSync);
		hdr.packetType = PKT_TYPE_SYNC;

		uint8_t* bufPos = writeBuf;
		memcpy(bufPos, (void*) &hdr, sizeof(packet_hdr_t));
		bufPos += sizeof(packet_hdr_t);
		memcpy(bufPos, (void*) &pktSync, sizeof(pktSync));
		bufPos += sizeof(pktSync);
//		log_printf("send %s:%d sync with size %d\n", StringAsCStr(host), port, bufPos-writeBuf);
//		conn->sendTo(host, port, writeBuf, bufPos-writeBuf);
	}
	bool processPacket(network_conn_t* conn, std::vector<uint8_t>& dataBuf) {
		if (dataBuf.size() < sizeof(heartbeat_message)) {
			return false;
		}
		uint8_t* const readBuf = dataBuf.data();
		uint8_t* readBufPos = readBuf;
		memcpy(&pktHeartbeat, readBufPos, sizeof(heartbeat_message));
		readBufPos += sizeof(heartbeat_message);
		const int16_t lampId = pktHeartbeat.lampId;
		dataBuf.erase(dataBuf.begin(), dataBuf.begin()+(sizeof(heartbeat_message)));
		return true;
	}

};

UDPSyncController::UDPSyncController() : impl(new UDPSyncController::Impl{}) {

}

UDPSyncController::~UDPSyncController() {
	delete impl;
}
bool UDPSyncController::processPacket(network_conn_t* conn, std::vector<uint8_t>& dataBuf)
{
	return impl->processPacket(conn, dataBuf);
}
void UDPSyncController::sendSyncTo(network_conn_t* conn, String host, int port)
{
	impl->sendSyncTo(conn, host, port);
}

bool quitUDPServer = false;
class NetworkServer_UDP_ThreadTask : public WorkerThread::ThreadTask {
	UDPSyncController controller;
	rgbprotocol_server_nethandler_server handler;
	NetServerState threadState;
	std::shared_ptr<network_conn_t> listenSocket;
	std::recursive_mutex mutex;
public:
	NetworkServer_UDP_ThreadTask() : WorkerThread::ThreadTask(), threadState() {
	}
	struct last_seen_host {
		int port;
		int64_t time = 0;
		bool needsUpdate = false;
	};
	std::map<String, last_seen_host> hosts;
	void sendSyncTo(String host, int port) {
		std::lock_guard<std::recursive_mutex> lockguard(mutex);
		if (hosts.count(host) == 0 && hosts.size() >= 100) {
			log_printf("too many hosts!\n", 0);
			return;
		}
		hosts[host] = {port, getTimeHPint64(), true};
	}

	void run() {
		network_io netio(&handler);
		if (netio.listenAt("192.168.0.228", 7171, protocol_type_i32::UDP, listenSocket)) {
			log_printf("bound to 192.168.0.228:7171\n", 0);
			netio.setSelectTimeout(0.01);
			if (quitUDPServer) {
				threadState.shouldQuit = true;
			}
			long tmLastSync = getTimeHPint64();
			while (netio.hasOpenSockets() && !threadState.shouldQuit) {
//				log_printf("netio.update\n", 0);

				netio.setSelectTimeout(0.0001);
				netio.update();
				dbgassert(listenSocket);
				dbgassert(!threadState.shouldQuit);

				threadState.isConnected = true;
				while (controller.processPacket(listenSocket.get(), handler.dataBuf));

				std::lock_guard<std::recursive_mutex> lockguard(mutex);
				for (auto& host : hosts) {
					if (host.second.needsUpdate) {
						host.second.needsUpdate = false;
						controller.sendSyncTo(listenSocket.get(), host.first, host.second.port);
					}
				}
//					std::vector<uint8_t>& dataBuf = pc->handler->buf;
//					if (!dataBuf.size()) {
//						continue;
//					}
//					log_printf("processPacket\n", 0);
//
//					while (controller.processPacket(pc.get(), dataBuf));
//				}
			}
		} else {
			log_printf("Failed binding\n", 0);
		}

		threadState.isRunning=false;
	}
};
static std::shared_ptr<NetworkServer_UDP_ThreadTask> server;
void GlobalUDP_sendSyncTo(String host, int port) {
	if (server && !server->isCompleted()) {
		server->sendSyncTo(host, port);
	}
}
std::shared_ptr<WorkerThread::ThreadTask> createUDPServer() {
	server = std::make_shared<NetworkServer_UDP_ThreadTask>();
	return server;
}
