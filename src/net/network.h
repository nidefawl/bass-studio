/**
 * Copyright (c) 2018 Michael Hept
 * Based on dyad library
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */
#pragma once

#include <stdint.h>
#include <string>
#include <memory>
#include <atomic>

using String = std::string;
extern std::atomic<int32_t> connId;
class network_conn_t;
class network_io;
class inetwork_handler {
public:
	virtual ~inetwork_handler() { };
	virtual void onError(int errorType, String msg) {

	}
	virtual bool onReceive(void* data, size_t size) {
		return true;
	}
	virtual bool onAccept(std::shared_ptr<network_conn_t> client) {
		return true;
	}
	virtual void onConnect(std::shared_ptr<network_conn_t> conn) {
	}
	virtual void onDisconnect(std::shared_ptr<network_conn_t> conn) {
	}
};
class network_conn_t {
public:
	network_io* const parent;
	inetwork_handler* handler{nullptr};
	String address;
	char szConnId[512] = {0};
	const char* connectionId() {
		if (!szConnId[0]) {
			snprintf(szConnId, 512, "CONN%d %s", ++connId, address.c_str());
		}
		return szConnId;
	}
	int port = 0;
	network_conn_t(network_io* _parent) : parent(_parent) {;
	};
	virtual ~network_conn_t() { };
	virtual void write(void* data, size_t size) = 0;
	virtual void setSocketOpt(int a, int b, int opt) = 0;
	virtual void disconnect() = 0;
	virtual void closeSocket() = 0;
	virtual void flush() = 0;
};
class network_io {
	friend class network_conn_t;
	friend class network_socket_t;
private:
	struct Impl;
public:
	enum NET_ERROR : int {
		NET_ERROR_UNKNOWN = 0,
		NET_ERROR_CREATE_SOCKET = 1,
		NET_ERROR_RESOLVE_HOST = 2,
		NET_ERROR_CONNECT = 3,
		NET_ERROR_BIND = 4,
		NET_ERROR_LISTEN = 5,
	};
	network_io(inetwork_handler* handler);
	~network_io();
	bool listenAt(const char *host, int port, std::shared_ptr<network_conn_t>& out);
	bool connectTo(const char *host, int port, std::shared_ptr<network_conn_t>& out);
	void update();
	void setSelectTimeout(double dSeconds);
	void disconnectAll();
	bool hasOpenSockets();
private:
	Impl* _M_impl;
};


void network_init(void);
void network_cleanup(void);

