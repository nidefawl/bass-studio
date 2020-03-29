/**
 * Copyright (c) 2020 Michael Hept
 */
#pragma once
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
#include "udp_sync_server.h"
#define RGB_PROTOCOL_WRITE_BUF_SIZE (4U*1024U)

struct RGBNetworkController;
struct rgbprotocol_net_handler_server;
struct NetServerState {

	std::atomic<bool> isRunning{true};
	std::atomic<bool> isConnected{true};
	std::atomic<bool> shouldQuit{false};
};
class RGBMasterController : public WorkerThread::ThreadTask {
	RGBNetworkController* const controller;
	rgbprotocol_net_handler_server* handler;
	uint8_t writeBuf[RGB_PROTOCOL_WRITE_BUF_SIZE];
public:
	NetServerState threadState;
	RGBMasterController();
	~RGBMasterController();
	void run() override;
};
