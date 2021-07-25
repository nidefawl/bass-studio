#pragma once
#include "str_util.h"

typedef enum {
	IPC_OK = 0,
	IPC_UNKNOWN,
	IPC_SOCKET_ERROR,
	IPC_CONNECT_FAILED
} ipc_status;
class ipc_connection {
public:
	virtual ~ipc_connection() { };
	virtual int sendData(char* buf, unsigned int len) = 0;
	virtual int readData(char* buf, unsigned int len) = 0;
};
class ipc_server : public ipc_connection
{
private:
	class Impl;
public:
	ipc_server();
	~ipc_server();
	int server_open(String path);
	int server_accept();
	void server_disconnect();
	void server_close();
	int sendData(char* buf, unsigned int len);
	int readData(char* buf, unsigned int len);
	int peekReadBufferSize();
	ipc_server(const ipc_server&) = delete;
	ipc_server& operator=(const ipc_server&) = delete;
private:
	Impl* _M_impl = NULL;
};

class ipc_client : public ipc_connection
{
public:

private:
	class Impl;
public:
	ipc_client();
	~ipc_client();
    int client_connect(String path);
	void client_close();
	int sendData(char* buf, unsigned int len);
	int readData(char* buf, unsigned int len);
	ipc_client(const ipc_client&) = delete;
	ipc_client& operator=(const ipc_client&) = delete;
private:
	Impl* _M_impl = NULL;
};
