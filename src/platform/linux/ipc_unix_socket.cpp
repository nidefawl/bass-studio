#include "ipc.h"
#include "str_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

class ipc_server::Impl
{
	unsigned int s = 0, s2 = 0;
	struct sockaddr_un remote{0};
public:
	Impl() {

	}
	~Impl() {

	}
    int server_open(String path) {
    	s = socket(AF_UNIX, SOCK_STREAM, 0);
    	if (s == -1) {
    		return IPC_SOCKET_ERROR;
    	}

    	struct sockaddr_un local{0};
    	local.sun_family = AF_UNIX;
    	strcpy(local.sun_path, StringAsCStr(path));
    	unlink(local.sun_path);
    	unsigned int len = strlen(local.sun_path) + sizeof(local.sun_family);

    	if (0 != bind(s, (struct sockaddr *)&local, len))
    		return 1;

    	listen(s, 5);
		return IPC_OK;
    }
    int server_accept() {
    	unsigned int msglen = sizeof(remote);
    	s2 = accept(s, (struct sockaddr *)&(remote), &msglen);
    	return s2 > 0 ? 0 : 1;
    }
    int server_read(char *buf, int buflen) {
    	return recv(s2, buf, buflen, 0);
    }
    int server_send(char *buf, int buflen) {
    	return send(s2, buf, buflen, 0);
    }
    void server_close() {
    	server_disconnect();
    	if (s > 0) {
    		close(s);
			s =  0;
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
	_M_impl { new ipc_server::Impl {  } } {
}
ipc_server::~ipc_server() {
	delete _M_impl;
}
int ipc_server::server_open(String path) {
	return _M_impl->server_open(path);
}
int ipc_server::server_accept() {
	return _M_impl->server_accept();
}
void ipc_server::server_disconnect() {
	_M_impl->server_disconnect();
}
void ipc_server::server_close() {
	_M_impl->server_close();
}


class ipc_client::Impl
{
	int s=0;
public:
	Impl() {

	}
	~Impl() {

	}
    int client_connect(String path) {
    	s = socket(AF_UNIX, SOCK_STREAM, 0);
    	if (s == -1) {
    		return IPC_SOCKET_ERROR;
    	}
    	struct sockaddr_un remote{0};
    	remote.sun_family = AF_UNIX;
    	strcpy(remote.sun_path, StringAsCStr(path));
    	unsigned int len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    	if (connect(s, (struct sockaddr *)&remote, len) == -1) {
    		return IPC_CONNECT_FAILED;
    	}
		return IPC_OK;
    }
    int client_read(char *buf, int buflen) {
    	return recv(s, buf, buflen, 0);
    }
    int client_send(char *buf, int buflen) {
    	return send(s, buf, buflen, 0);
    }
    void client_close() {
    	if (s) {
        	close(s);
    	}
    }
};

ipc_client::ipc_client() :
	_M_impl { new ipc_client::Impl {  } } {
}
ipc_client::~ipc_client() {
	delete _M_impl;
}
int ipc_client::client_connect(String path) {
	return _M_impl->client_connect(path);
}
void ipc_client::client_close() {
	_M_impl->client_close();
}
