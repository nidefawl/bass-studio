#include "ipc.h"
#include "logging.h"
#include "str_util.h"
#include "platform.h"
#include "thread.h"
#include "exceptions.h"

#include <windows.h>
#include <cstdlib>


void printLastError(const char* fn) {
    const DWORD err = GetLastError();
    const auto errorMsg = FormatErrorMessage(err, StringFormat("%s failed (%lu)", fn, err));
    log_lf(Log::L_ERROR, "%s\n", errorMsg.c_str());
}

class ipc_server::Impl {
    HANDLE pipe = nullptr;

public:
    Impl() = default;
    ~Impl() {
        server_close();
    }
    int server_open(const char* path) {
        const String pipeName = StringFormat(R"(\\.\pipe\%s)", path);
        pipe = CreateNamedPipe(
                StringAsCStr(pipeName),    // name of the pipe
                PIPE_ACCESS_DUPLEX,        // bidirectional
                PIPE_TYPE_BYTE | PIPE_WAIT,// send data as a byte stream
                1,                         // only allow 1 instance of this pipe
                0,                         // no outbound buffer
                0,                         // no inbound buffer
                3000,                      // 3000ms timeout
                nullptr                    // use default security attributes
        );
        if (!pipe || pipe == INVALID_HANDLE_VALUE) {
            return IPC_SOCKET_ERROR;
        }
        return IPC_OK;
    }
    int server_accept() {
        bool state = false;
        int nMax   = 30;
        while (!state && nMax > 0) {
            state = ConnectNamedPipe(pipe, nullptr);
            if (state) break;
            //TODO: this sleep should be invoked on the call site
            seqthreads::threadSleep(100);
            nMax--;
        }
        if (!state && GetLastError() != ERROR_PIPE_CONNECTED) {
            printLastError("ConnectNamedPipe");
            DisconnectNamedPipe(pipe);
            return 1;
        }
        return 0;
    }
    int server_read(char* buf, unsigned int buflen) {
        memset(buf, 0, buflen);
        DWORD bytesSent = 0;
        if (ReadFile(pipe, buf, buflen, &bytesSent, nullptr)) {
            return static_cast<int>(bytesSent);
        }
        printLastError("ReadFile");
        return 0;
    }
    int server_peekreadbuf() {
        DWORD bytesAvail = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &bytesAvail, nullptr)) {
            printLastError("PeekNamedPipe");
            return -1;
        }
        return static_cast<int>(bytesAvail);
    }
    int server_send(char* buf, unsigned int buflen) {
        DWORD bytesSent = 0;
        if (WriteFile(pipe, buf, buflen, &bytesSent, nullptr)) {
            return static_cast<int>(bytesSent);
        }
        printLastError("WriteFile");
        return 0;
    }
    void server_close() {
        server_disconnect();
        if (pipe) {
            CloseHandle(pipe);
            pipe = nullptr;
        }
    }
    void server_disconnect() {
        if (pipe) {
            DisconnectNamedPipe(pipe);
        }
    }
};

ipc_server::ipc_server() : m_impl{ new ipc_server::Impl{} } {
}
ipc_server::~ipc_server() {
    delete m_impl;
}
int ipc_server::server_open(const char* path) {
    return m_impl->server_open(path);
}
int ipc_server::server_accept() {
    return m_impl->server_accept();
}
void ipc_server::server_disconnect() {
    m_impl->server_disconnect();
}
void ipc_server::server_close() {
    m_impl->server_close();
}
int ipc_server::sendData(char* buf, unsigned int len) {
    return m_impl->server_send(buf, len);
}
int ipc_server::readData(char* buf, unsigned int len) {
    return m_impl->server_read(buf, len);
}
int ipc_server::peekReadBufferSize() {
    return m_impl->server_peekreadbuf();
}

class ipc_client::Impl {
    HANDLE pipe = nullptr;

public:
    Impl() {
    }
    ~Impl() {
        client_close();
    }
    int client_connect(const char* path) {
        const String pipeName = StringFormat(R"(\\.\pipe\%s)", path);
        if (!WaitNamedPipe(StringAsCStr(pipeName), 5000)) {
            client_close();
            return IPC_CONNECT_FAILED;
        }
        pipe = CreateFile(
                StringAsCStr(pipeName),
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

        if (!pipe || pipe == INVALID_HANDLE_VALUE) {
            return IPC_SOCKET_ERROR;
        }
        return IPC_OK;
    }
    int client_read(char* buf, unsigned int buflen) {
        memset(buf, 0, buflen);
        DWORD bytesSent = 0;
        if (ReadFile(pipe, buf, buflen, &bytesSent, nullptr)) {
            return static_cast<int>(bytesSent);
        }
        printLastError("ReadFile");
        return 0;
    }
    int client_send(char* buf, unsigned int buflen) {
        DWORD bytesSent = 0;
        if (WriteFile(pipe, buf, buflen, &bytesSent, nullptr)) {
            return static_cast<int>(bytesSent);
        }
        printLastError("WriteFile");
        return 0;
    }
    void client_close() {
        if (pipe) {
            CloseHandle(pipe);
            pipe = nullptr;
        }
    }
};

ipc_client::ipc_client() : m_impl{ new ipc_client::Impl{} } {
}
ipc_client::~ipc_client() {
    delete m_impl;
}
int ipc_client::client_connect(const char* path) {
    return m_impl->client_connect(path);
}
void ipc_client::client_close() {
    m_impl->client_close();
}
int ipc_client::sendData(char* buf, unsigned int len) {
    return m_impl->client_send(buf, len);
}
int ipc_client::readData(char* buf, unsigned int len) {
    return m_impl->client_read(buf, len);
}
