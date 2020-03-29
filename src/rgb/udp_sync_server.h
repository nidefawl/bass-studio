/**
 * Copyright (c) 2018 Michael Hept
 */
#pragma once

#include <memory>
#include "threads/workerthread.h"

std::shared_ptr<WorkerThread::ThreadTask> createUDPServer();
void GlobalUDP_sendSyncTo(String host, int port);
