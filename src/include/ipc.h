#pragma once
#include "str_util.h"

typedef enum {
	IPC_OK = 0,
	IPC_UNKNOWN,
	IPC_SOCKET_ERROR,
	IPC_CONNECT_FAILED
} ipc_status;
class ipc_server
{
public:

private:
	class Impl;
public:
	ipc_server();
	~ipc_server();
	int server_open(String path);
	void server_close();
	ipc_server(const ipc_server&) = delete;
	ipc_server& operator=(const ipc_server&) = delete;
private:
	Impl* _M_impl = NULL;
};

class ipc_client
{
public:

private:
	class Impl;
public:
	ipc_client();
	~ipc_client();
    int client_connect(String path);
	void client_close();
	ipc_client(const ipc_client&) = delete;
	ipc_client& operator=(const ipc_client&) = delete;
private:
	Impl* _M_impl = NULL;
};
