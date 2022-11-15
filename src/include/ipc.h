#pragma once
#include "str_util.h"

enum ipc_status : int32_t {
    IPC_UNKNOWN = -1,
    IPC_OK = 0,
    IPC_CONNECT_FAILED = 1,
    IPC_SOCKET_ERROR = 2,
};

class ipc_connection {
public:
    virtual ~ipc_connection() = default;
    virtual int sendData(char* buf, uint32_t len) = 0;
    virtual int readData(char* buf, uint32_t len) = 0;
};

class ipc_server final : public ipc_connection {
    class Impl;
    Impl* m_impl = nullptr;
public:
    ipc_server();
    ~ipc_server() override;
    int server_open(const char* path);
    int server_accept();
    void server_disconnect();
    void server_close();
    int sendData(char* buf, uint32_t len) override;
    int readData(char* buf, uint32_t len) override;
    int peekReadBufferSize();
    ipc_server(const ipc_server&)            = delete;
    ipc_server& operator=(const ipc_server&) = delete;
};

class ipc_client final : public ipc_connection {
    class Impl;
    Impl* m_impl = nullptr;
public:
    ipc_client();
    ~ipc_client() override;
    int client_connect(const char* path);
    void client_close();
    int sendData(char* buf, uint32_t len) override;
    int readData(char* buf, uint32_t len) override;
    ipc_client(const ipc_client&)            = delete;
    ipc_client& operator=(const ipc_client&) = delete;
};
