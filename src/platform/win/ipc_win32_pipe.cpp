#include "ipc.h"
#include "str_util.h"
#include "platform.h"

#include <windows.h>
#include <stdlib.h>


String FormatErrorMessage(int32_t error, String msg);
void printLastError(String fn) {
	DWORD err = GetLastError();
	String desc = StringFormat("%s failed (%d)", StringAsCStr(fn), (int32_t)err);
	printf("%s\n", StringAsCStr(FormatErrorMessage(err, desc)));
	fflush(stdout);
}

class ipc_server::Impl
{
	HANDLE pipe = NULL;
public:
	Impl() {

	}
	~Impl() {
		server_close();
	}
    int server_open(String path) {
    	String pipeName = StringFormat("\\\\.\\pipe\\%s", StringAsCStr(path));
		pipe = CreateNamedPipe(
				StringAsCStr(pipeName), // name of the pipe
				PIPE_ACCESS_DUPLEX,
				PIPE_TYPE_BYTE | PIPE_WAIT, // send data as a byte stream
				1, // only allow 1 instance of this pipe
				0, // no outbound buffer
				0, // no inbound buffer
				3000, // use default wait time
				NULL // use default security attributes
				);
		if (!pipe || pipe == INVALID_HANDLE_VALUE) {
			return IPC_SOCKET_ERROR;
		}
		return IPC_OK;
    }
    int server_accept() {
    	bool state = false;
    	int nMax = 30;
    	while (!state && nMax > 0) {
    		state = ConnectNamedPipe(pipe, NULL);
    		if (state) break;
    		threadSleep(100);
    		nMax--;
    	}
		if (!state && GetLastError() != ERROR_PIPE_CONNECTED) {
	    	printLastError("ConnectNamedPipe");
    		DisconnectNamedPipe(pipe);
	    	return 1;
		}
		return 0;
    }
    int server_read(char *buf, unsigned int buflen) {
    	memset(buf, 0, buflen);
    	DWORD bytesSent;
    	if (ReadFile(pipe, buf, buflen, &bytesSent, NULL)) {
    		return bytesSent;
		}
//    	printf("ReadFile: %d\n", bytesSent);
    	printLastError("ReadFile");
		return 0;
	}
    int server_peakreadbuf() {
    	DWORD bytesAvail = 0;
		if( !PeekNamedPipe(pipe, NULL, 0, NULL, &bytesAvail, NULL) ){
	    	printLastError("PeekNamedPipe");
	    	return -1;
		}
//    	printf("PeekNamedPipe: %d\n", bytesAvail);
		return bytesAvail;
    }
    int server_send(char *buf, unsigned int buflen) {
        DWORD bytesSent;
        if (WriteFile(pipe, buf, buflen, &bytesSent, NULL)) {
    		return bytesSent;
		}
//    	printf("WriteFile: %d\n", bytesSent);
    	printLastError("WriteFile");
		return 0;
    }
    void server_close() {
    	server_disconnect();
		if (pipe) {
			CloseHandle(pipe);
			pipe = NULL;
		}
    }
    void server_disconnect() {
    	if (pipe) {
    		DisconnectNamedPipe(pipe);
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
int ipc_server::sendData(char* buf, unsigned int len) {
	return _M_impl->server_send(buf, len);
}
int ipc_server::readData(char* buf, unsigned int len) {
	return _M_impl->server_read(buf, len);
}
int ipc_server::peakReadBufferSize() {
	return _M_impl->server_peakreadbuf();
}

class ipc_client::Impl
{
	HANDLE pipe = nullptr;
public:
	Impl() {

	}
	~Impl() {
		client_close();
	}
    int client_connect(String path) {
    	String pipeName = StringFormat("\\\\.\\pipe\\%s", StringAsCStr(path));
		if (!WaitNamedPipe(StringAsCStr(pipeName), 5000)) {
			client_close();
			return IPC_CONNECT_FAILED;
		}
		pipe = CreateFile(
			StringAsCStr(pipeName), // name of the pipe
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);
	    if (!pipe || pipe == INVALID_HANDLE_VALUE) {
			return IPC_SOCKET_ERROR;
	    }
		return IPC_OK;
    }
    int client_read(char *buf, unsigned int buflen) {
    	memset(buf, 0, buflen);
    	DWORD bytesSent;
    	if (ReadFile(pipe, buf, buflen, &bytesSent, NULL)) {
    		return bytesSent;
		}
    	printf("ReadFile: %d\n", bytesSent);
    	printLastError("ReadFile");
		return 0;
	}
    int client_send(char *buf, unsigned int buflen) {
        DWORD bytesSent;
        if (WriteFile(pipe, buf, buflen, &bytesSent, NULL)) {
    		return bytesSent;
		}
    	printf("WriteFile: %d\n", bytesSent);
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
int ipc_client::sendData(char* buf, unsigned int len) {
	return _M_impl->client_send(buf, len);
}
int ipc_client::readData(char* buf, unsigned int len) {
	return _M_impl->client_read(buf, len);
}
