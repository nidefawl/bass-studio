#ifndef _WIN32
#include "ipc.hpp"
#include "str_util.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include "logging.hpp"


class ipc_server::Impl {
    int32_t m_fdSockListen = 0;
    int32_t m_fdSockClient = 0;
    struct sockaddr_un remote{};
    String pathUnlink;

public:
    Impl()  = default;
    ~Impl() = default;

    int server_open(const char* path) {
        int newSocket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_fdSockListen == -1) {
            return IPC_SOCKET_ERROR;
        }
        m_fdSockListen = newSocket;
        int ret        = unlink(path);
        log_printf("unlink %s returned %d.\n", path, ret);

        struct sockaddr_un local{};
        local.sun_family = AF_UNIX;
        safe_strcpy(local.sun_path, path);
        local.sun_path[sizeof(local.sun_path) - 1] = '\0';

        socklen_t len = (offsetof(struct sockaddr_un, sun_path) + strlen(local.sun_path));
        ret = bind(m_fdSockListen, (struct sockaddr*) &local, len);
        if (0 != ret) {
            log_printf("bind returned %d. Unlinking unix socket %s\n", ret, path);
            ret = unlink(path);
            log_printf("second unlink %s returned %d.\n", path, ret);
            ret = bind(m_fdSockListen, (struct sockaddr*) &local, len);
            log_printf("second bind returned %d\n", ret);
            if (0 != ret) {
                return 1;
            }
        }
        pathUnlink = path;

        listen(m_fdSockListen, 5);
        return IPC_OK;
    }

    int server_accept() {
        uint32_t msglen = sizeof(remote);
        m_fdSockClient  = accept(m_fdSockListen, (struct sockaddr*) &(remote), &msglen);
        return m_fdSockClient > 0 ? 0 : 1;
    }

    int server_read(char* buf, uint32_t buflen) {
        return recv(m_fdSockClient, buf, buflen, MSG_WAITALL);
    }

    int server_send(char* buf, uint32_t buflen) {
        return send(m_fdSockClient, buf, buflen, 0);
    }

    int server_peekreadbuf() {
        int bytesAv = 0;
        if (ioctl(m_fdSockClient,FIONREAD,&bytesAv) == 0) {
            return bytesAv;
        }
        return 0;
    }

    void server_close() {
        server_disconnect();
        if (m_fdSockListen > 0) {
            close(m_fdSockListen);
            m_fdSockListen = 0;
        }
        if (!pathUnlink.empty()) {
            unlink(StringAsCStr(pathUnlink));
        }
    }

    void server_disconnect() {
        if (m_fdSockClient > 0) {
            close(m_fdSockClient);
            m_fdSockClient = 0;
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

int ipc_server::sendData(char* buf, uint32_t len) {
    return m_impl->server_send(buf, len);
}

int ipc_server::readData(char* buf, uint32_t len) {
    return m_impl->server_read(buf, len);
}

int ipc_server::peekReadBufferSize() {
    return m_impl->server_peekreadbuf();
}


class ipc_client::Impl {
    int m_fdSock = 0;

public:
    Impl()  = default;
    ~Impl() = default;
    int client_connect(const char* path) {
        m_fdSock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_fdSock == -1) {
            return IPC_SOCKET_ERROR;
        }


        struct sockaddr_un remote{};
        remote.sun_family = AF_UNIX;
        safe_strcpy(remote.sun_path, path);
        remote.sun_path[sizeof(remote.sun_path) - 1] = '\0';

        uint32_t len = (offsetof(struct sockaddr_un, sun_path) + strlen(remote.sun_path));

        if (connect(m_fdSock, (struct sockaddr*) &remote, len) == -1) {
            return IPC_CONNECT_FAILED;
        }
        return IPC_OK;
    }
    int client_read(char* buf, uint32_t buflen) {
        return static_cast<int32_t>(recv(m_fdSock, buf, buflen, MSG_WAITALL));
    }
    int client_send(char* buf, uint32_t buflen) {
        return static_cast<int32_t>(send(m_fdSock, buf, buflen, 0));
    }
    void client_close() {
        if (m_fdSock) {
            close(m_fdSock);
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

int ipc_client::sendData(char* buf, uint32_t len) {
    return m_impl->client_send(buf, len);
}

int ipc_client::readData(char* buf, uint32_t len) {
    return m_impl->client_read(buf, len);
}
#endif
