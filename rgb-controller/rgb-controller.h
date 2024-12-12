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
#include "assert_dbg.h"
#include "net/network.h"
#include "net/packet.h"
#include "threads/workerthread.h"
#include "logging.h"

#define RGB_PROTOCOL_WRITE_BUF_SIZE (4U*1024U)

struct RGBNetworkController;
class rgbprotocol_net_handler_server;
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
	RGBNetworkController* getController();
	void run() override;
};
