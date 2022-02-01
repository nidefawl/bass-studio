#ifndef _WIN32
#include "ipc.h"
#include "str_util.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "logging.h"


class ipc_server::Impl
{
	unsigned int s = 0, s2 = 0;
	struct sockaddr_un remote{0};
	String pathUnlink;
public:
	Impl() = default;
	~Impl() = default;
    int server_open(String path) {
    	s = socket(AF_UNIX, SOCK_STREAM, 0);
    	if (s == -1) {
    		return IPC_SOCKET_ERROR;
    	}
    	int ret = unlink(StringAsCStr(path));
		log_printf("unlink %s returned %d.\n", StringAsCStr(path), ret);

    	struct sockaddr_un local{0};
    	local.sun_family = AF_UNIX;
    	  strncpy (local.sun_path, StringAsCStr(path), sizeof (local.sun_path));
    	  local.sun_path[sizeof (local.sun_path) - 1] = '\0';
    	  unsigned int len = (offsetof (struct sockaddr_un, sun_path)
    	           + strlen (local.sun_path));
    	 ret = bind(s, (struct sockaddr *)&local, len);
    	if (0 != ret) {
    		log_printf("bind returned %d. Unlinking unix socket %s\n", ret, StringAsCStr(path));
    		ret = unlink(StringAsCStr(path));
    		log_printf("second unlink %s returned %d.\n", StringAsCStr(path), ret);
        	ret = bind(s, (struct sockaddr *)&local, len);
			log_printf("second bind returned %d\n", ret);
        	if (0 != ret) {
        		return 1;
        	}
    	}
    	pathUnlink = path;

    	listen(s, 5);
		return IPC_OK;
    }
    int server_accept() {
    	unsigned int msglen = sizeof(remote);
    	s2 = accept(s, (struct sockaddr *)&(remote), &msglen);
    	return s2 > 0 ? 0 : 1;
    }
    int server_read(char *buf, unsigned int buflen) {
    	return recv(s2, buf, buflen, 0);
    }
    int server_send(char *buf, unsigned int buflen) {
    	return send(s2, buf, buflen, 0);
    }
    int server_peekreadbuf() {
    	char peekBuf[32];
    	return recv(s2, peekBuf, 32, MSG_PEEK);
    }
    void server_close() {
    	server_disconnect();
    	if (s > 0) {
    		close(s);
			s =  0;
    	}
    	if (pathUnlink.length()) {
    		unlink(StringAsCStr(pathUnlink));
    	}

    }
    void server_disconnect() {
    	if (s2 > 0) {
    		close(s2);
			s2 =  0;
    	}
    }
};

ipc_server::ipc_server() :
	m_impl { new ipc_server::Impl {  } } {
}
ipc_server::~ipc_server() {
	delete m_impl;
}
int ipc_server::server_open(String path) {
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


class ipc_client::Impl
{
	int s=0;
public:
	Impl() = default;
	~Impl() = default;
    int client_connect(String path) {
    	s = socket(AF_UNIX, SOCK_STREAM, 0);
    	if (s == -1) {
    		return IPC_SOCKET_ERROR;
    	}


    	struct sockaddr_un remote{0};
    	remote.sun_family = AF_UNIX;
		strncpy (remote.sun_path, StringAsCStr(path), sizeof (remote.sun_path));
		remote.sun_path[sizeof (remote.sun_path) - 1] = '\0';

		unsigned int len = (offsetof (struct sockaddr_un, sun_path) + strlen (remote.sun_path));

    	if (connect(s, (struct sockaddr *)&remote, len) == -1) {
    		return IPC_CONNECT_FAILED;
    	}
		return IPC_OK;
    }
    int client_read(char *buf, unsigned int buflen) {
    	return recv(s, buf, buflen, 0);
    }
    int client_send(char *buf, unsigned int buflen) {
    	return send(s, buf, buflen, 0);
    }
    void client_close() {
    	if (s) {
        	close(s);
    	}
    }
};

ipc_client::ipc_client() :
	m_impl { new ipc_client::Impl {  } } {
}
ipc_client::~ipc_client() {
	delete m_impl;
}
int ipc_client::client_connect(String path) {
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
#endif
