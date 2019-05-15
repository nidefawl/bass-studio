/**
 * Copyright (c) 2018 Michael Hept
 */
#include <memory>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include "assert_dbg.h"

#ifdef _WIN32
//  #define _WIN32_WINNT 0x501
//  #ifndef _CRT_SECURE_NO_WARNINGS
//    #define _CRT_SECURE_NO_WARNINGS
//  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
//  #include <windows.h>
typedef SOCKET sock_type_t;
#else
  //#define _POSIX_C_SOURCE 200809L
  #ifdef __APPLE__
    #define _DARWIN_UNLIMITED_SELECT
  #endif
  #include <signal.h>
  #include <unistd.h>
  #include <netdb.h>
  #include <fcntl.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/time.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
typedef int sock_type_t;
#endif
#include "network.h"
#include "exceptions.h"

using String = std::string;

std::atomic<int32_t> connId{0};



#ifdef _WIN32
  #define close(a) closesocket(a)
  #define getsockopt(a,b,c,d,e) getsockopt((a),(b),(c),(char*)(d),(e))
  #define setsockopt(a,b,c,d,e) setsockopt((a),(b),(c),(char*)(d),(e))
  #define select(a,b,c,d,e) select((int)(a),(b),(c),(d),(e))
  #define bind(a,b,c) bind((a),(b),(int)(c))
  #define connect(a,b,c) connect((a),(b),(int)(c))

  #undef  errno
  #define errno WSAGetLastError()

  #undef  EWOULDBLOCK
  #define EWOULDBLOCK WSAEWOULDBLOCK
	const char *inet_addrtocstr(int af, const void *src, char *dst, socklen_t size) {
		union {
			struct sockaddr sa;
			struct sockaddr_in sai;
			struct sockaddr_in6 sai6;
		} addr;
		int res;
		memset(&addr, 0, sizeof(addr));
		addr.sa.sa_family = af;
		if (af == AF_INET6) {
			memcpy(&addr.sai6.sin6_addr, src, sizeof(addr.sai6.sin6_addr));
		} else {
			memcpy(&addr.sai.sin_addr, src, sizeof(addr.sai.sin_addr));
		}
		res = WSAAddressToStringA(&addr.sa, sizeof(addr), 0, dst, (LPDWORD) &size);
		if (res != 0)
			return NULL;
		return dst;
	}
	int getImplErrno() {
		return WSAGetLastError();
	}
#else
	int getImplErrno() {
		return errno;
	}
	const char *inet_addrtocstr(int af, const void *src, char *dst, socklen_t size) {
		return inet_ntop(af, src, dst, size);
	}
#endif
#ifndef INVALID_SOCKET
  #define INVALID_SOCKET -1
#endif
#define SOCK_UNSIGNED_BIT (sizeof(unsigned) * CHAR_BIT)

void setTimeout(struct timeval& tv, double seconds) {
	/* Init timeout value and do select */
	#ifdef _MSC_VER
	#pragma warning(push)
		/* Disable double to long implicit conversion warning,
		 * because the type of timeval's fields don't agree across platforms */
	#pragma warning(disable: 4244)
	#endif
		tv.tv_sec = seconds;
		tv.tv_usec = (seconds - tv.tv_sec) * 1e6;
	#ifdef _MSC_VER
	#pragma warning(pop)
	#endif
}
static String errnoToStr(const char *msg, int err) {
	if (0 == err) {
		return msg;
	}
	static const char* szFormat = "%.160s (%.80s)";
	char* errStr = strerror(err);
	int bufSize = snprintf(nullptr, 0, szFormat, msg, errStr);
	if (bufSize > 0 && bufSize < 1024) {
		char* buf = (char*)alloca(bufSize+2);
		snprintf(buf, bufSize+1, szFormat, msg, errStr);
		String s = buf;
		return s;
	}
	return FormatErrorMessage(err, msg);
}

static void network_panic(const char *fmt, ...) {
  char tmp[128];
  va_list args;
  va_start(args, fmt);
  vsprintf(tmp, fmt, args);
  va_end(args);
  printf("network_panic: %s\n", tmp);
  exit(EXIT_FAILURE);
}

void network_init(void) {
#ifdef _WIN32
  WSADATA dat;
  int err = WSAStartup(MAKEWORD(2, 2), &dat);
  if (err != 0) {
    network_panic("WSAStartup failed (%d)", err);
  }
#else
  /* Stops the SIGPIPE signal being raised when writing to a closed socket */
  signal(SIGPIPE, SIG_IGN);
#endif
}
void network_cleanup(void) {
#ifdef _WIN32
  WSACleanup();
#endif
}

//#define SOCK_FLAG_READY   (1 << 0)
#define SOCK_FLAG_WRITTEN (1 << 1)
#define SOCK_FLAG_ISLISTEN (1 << 2)
#define SOCK_FLAG_ISREMOTE (1 << 4)
#define SOCK_FLAG_ISCLIENT (1 << 8)

static void setNonBlocking(sock_type_t sockfd, int opt) {
#ifdef _WIN32
  u_long mode = opt;
  ioctlsocket(sockfd, FIONBIO, &mode);
#else
  int flags = fcntl(sockfd, F_GETFL);
  fcntl(sockfd, F_SETFL, opt ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
#endif
}


enum SOCK_FD_IDX : int {
	SELECT_READ = 0,
	SELECT_WRITE,
	SELECT_EXCEPT,
	SELECT_MAX
};
namespace {
struct SelectSet {
	int capacity = 0;
	sock_type_t maxfd = 0;
	fd_set fdsRead;
	fd_set fdsWrite;
	fd_set fdsExc;
	SelectSet() {
		memset(&fdsRead, 0, sizeof(fd_set));
		memset(&fdsWrite, 0, sizeof(fd_set));
		memset(&fdsExc, 0, sizeof(fd_set));
	}
	fd_set* fdsSetPtr(int target) {
		switch (target) {
		case SELECT_WRITE:
			return &fdsWrite;
		case SELECT_EXCEPT:
			return &fdsExc;
		}
		return &fdsRead;
	}
	void set(int target, sock_type_t sock) {
		FD_SET(sock, fdsSetPtr(target));
		maxfd = std::max<int>(maxfd, sock);
	}
	void reset() {
		maxfd = 0;
		FD_ZERO(&fdsRead);
		FD_ZERO(&fdsWrite);
		FD_ZERO(&fdsExc);
	}
	bool isset(int target, sock_type_t sock) {
		return (FD_ISSET(sock, fdsSetPtr(target)));
	}
};
}
enum SOCK_STATE : int {
  SOCK_STATE_CLOSED = 0,
  SOCK_STATE_CLOSING,
  SOCK_STATE_CONNECTED,
  SOCK_STATE_LISTENING
};

class network_socket_t : public network_conn_t {
	friend class network_io;
	sock_type_t sockfd;
	int state = SOCK_STATE_CLOSED;
	int flags = 0;
	std::vector<uint8_t> writeBuffer;
//  int bytesSent, bytesReceived;
//  double lastActivity, timeout;
public:
	network_socket_t(network_io* parent, sock_type_t _sockfd);
	~network_socket_t();
	int handleWrite();
	size_t handleRead();
	void initAddress();
	void setSocketOpt(int a, int b, int opt) override;
	void write(void* data, size_t size) override;
	void closeSocket() override;
	void disconnect() override;
	void flush() override;
};
struct network_io::Impl {
	friend class network_socket_t;
	network_io* const io;
	inetwork_handler* const handler;
	std::vector<std::shared_ptr<network_socket_t>> netSockets;
	int lastError = NET_ERROR_UNKNOWN;
	String errorMsg;
	double selectTimeoutSeconds = 5.0;
    Impl(network_io* _io, inetwork_handler* _handler) : io(_io), handler(_handler) {
    }
    ~Impl();
	bool listenAt(const char *host, int port, std::shared_ptr<network_conn_t>& out);
	bool connectTo(const char *host, int port, std::shared_ptr<network_conn_t>& out);
	void update();
	void setSelectTimeout(double dSeconds);
	void disconnectAll();
	bool hasOpenSockets();
private:
	void acceptPendingConnections(network_socket_t* netSocket);
	void setError(int errorType, String msg) {
		handler->onError(errorType, msg);
		this->lastError = errorType;
		this->errorMsg = msg;
	}
protected:
	void onSocketClose(network_socket_t* conn) {
		auto it = netSockets.begin();
		while (it != netSockets.end()) {
			auto& sharedPtr = *it;
			if (conn == sharedPtr.get()) {
				dbgassert(conn->handler);
				dbgassert(handler);
				if (!(conn->flags & SOCK_FLAG_ISLISTEN)) {
					conn->handler->onDisconnect(sharedPtr);
					handler->onDisconnect(sharedPtr);
				}
				it = netSockets.erase(it);
				continue;
			}
			it++;
		}
	}
};

network_socket_t::network_socket_t(network_io* _parent, sock_type_t _sockfd)
	: network_conn_t(_parent), sockfd(_sockfd) {
	setNonBlocking(sockfd, 1);
}
network_socket_t::~network_socket_t() {
}
network_io::network_io(inetwork_handler* handler) :
	_M_impl { new network_io::Impl { this, handler } } {
}
network_io::~network_io() {
	delete _M_impl;
}

bool network_io::listenAt(const char *host, int port, std::shared_ptr<network_conn_t>& out) {
	return _M_impl->listenAt(host, port, out);
}
bool network_io::connectTo(const char *host, int port, std::shared_ptr<network_conn_t>& out) {
	return _M_impl->connectTo(host, port, out);
}
bool network_io::hasOpenSockets() {
	return _M_impl->hasOpenSockets();
}
void network_io::disconnectAll() {
	_M_impl->disconnectAll();
}
void network_io::update() {
	_M_impl->update();
}
void network_io::setSelectTimeout(double dSeconds) {
	_M_impl->setSelectTimeout(dSeconds);
}

network_io::Impl::~Impl() {
	auto netSocketsCopy = this->netSockets;
	for (auto& socket : netSocketsCopy) {
		socket->handleWrite();
		socket->closeSocket();
	}
}
void network_io::Impl::setSelectTimeout(double dSeconds) {
	this->selectTimeoutSeconds = dSeconds;
}
void network_io::Impl::disconnectAll() {
	auto netSocketsCopy = this->netSockets;
	for (auto& socket : netSocketsCopy) {
		socket->disconnect();
	}
}
bool network_io::Impl::hasOpenSockets() {
	for (auto& socket : netSockets) {
		if (socket->state != SOCK_STATE_CLOSED) {
			return true;
		}
	}
	return false;
}
void network_io::Impl::update(void) {
	SelectSet selectSet;
	auto netSocketsCopy = this->netSockets;
	if (std::find_if(netSockets.begin(), netSockets.end(), [](auto& s) {
		return s->state == SOCK_STATE_CLOSED;
	}) != netSockets.end())
	{
		for (auto& socket : netSocketsCopy) {
			if (socket->state == SOCK_STATE_CLOSED) {
				onSocketClose(socket.get()); // removes socket from this->netsockets
			}
		}
		netSocketsCopy = this->netSockets;
	}
	for (auto& socket : netSocketsCopy) {
		switch (socket->state) {
		case SOCK_STATE_CONNECTED:
			selectSet.set(SELECT_READ, socket->sockfd);
			if (socket->flags & SOCK_FLAG_WRITTEN) {
				selectSet.set(SELECT_WRITE, socket->sockfd);
			}
			break;
		case SOCK_STATE_CLOSING:
			selectSet.set(SELECT_WRITE, socket->sockfd);
			break;
		case SOCK_STATE_LISTENING:
			selectSet.set(SELECT_READ, socket->sockfd);
			break;
		}
	}

	struct timeval tv;
	setTimeout(tv, selectTimeoutSeconds);
	int ret = select(selectSet.maxfd + 1, &selectSet.fdsRead, &selectSet.fdsWrite, &selectSet.fdsExc, &tv);
	if (ret == 0) {
		return;
	}
	if (ret < 0) {
		setError(NET_ERROR_UNKNOWN, errnoToStr("select failed", getImplErrno()));
		return;
	}
	for (auto& stream : netSocketsCopy) {
		switch (stream->state) {
		case SOCK_STATE_CONNECTED:
			if (selectSet.isset(SELECT_READ, stream->sockfd)) {
				stream->handleRead();
				if (stream->state == SOCK_STATE_CLOSED) {
					break;
				}
			}
			/* Fall through */
			//no break
		case SOCK_STATE_CLOSING:
			if (selectSet.isset(SELECT_WRITE, stream->sockfd)) {
				stream->handleWrite();
			}
			break;

		case SOCK_STATE_LISTENING:
			if (selectSet.isset(SELECT_READ, stream->sockfd)) {
				acceptPendingConnections(stream.get());
			}
			break;
		}
	}
}
struct addrinfo_deleter {
    template <typename T>
    void operator()(T *p) const {
    	freeaddrinfo(p);
    }
};
void network_io::Impl::acceptPendingConnections(network_socket_t* netSocket) {
	while (1) {
		int err = 0;
		sock_type_t sockfd = accept(netSocket->sockfd, NULL, NULL);
		if (sockfd == INVALID_SOCKET) {
			err = getImplErrno();
			if (err == EWOULDBLOCK) {
				/* No more waiting sockets */
				return;
			} else {
				//handle error accepting new incoming connection here
				return;
			}
		}
		/* Create client stream */
		std::shared_ptr<network_socket_t> remote = std::make_shared<network_socket_t>(io, sockfd);
		remote->state = SOCK_STATE_CONNECTED;
		remote->flags |= SOCK_FLAG_ISREMOTE;
		remote->initAddress();
		dbgassert(handler);
		if (!handler->onAccept(remote)) {
			close(sockfd);
			return;
		}
		dbgassert(remote->handler);
		this->netSockets.push_back(remote);
	}
}
bool network_io::Impl::listenAt(const char *host, int port, std::shared_ptr<network_conn_t>& out) {
	struct addrinfo hints, *ai = NULL;
	std::unique_ptr<addrinfo, addrinfo_deleter> deleter(ai);
	int err, optval;
	char buf[64];

	/* Get addrinfo */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	snprintf(buf, 64, "%d", port);
	err = getaddrinfo(host, buf, &hints, &ai);
	if (err) {
		setError(NET_ERROR_RESOLVE_HOST, errnoToStr("failed resolving host", err));
		return false;
	}
	/* Init socket */
	  printf("ai_family: %d\n", ai->ai_family);
	  printf("ai_socktype: %d\n", ai->ai_socktype);
	  printf("ai_protocol: %d\n", ai->ai_protocol);
	sock_type_t rawSocket = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (rawSocket == INVALID_SOCKET) {
		setError(NET_ERROR_CREATE_SOCKET, errnoToStr("failed resolving host", getImplErrno()));
		return false;
	}
	/* Set SO_REUSEADDR so that the socket can be immediately bound without
	 * having to wait for any closed socket on the same port to timeout */
	optval = 1;
	if (setsockopt(rawSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
		setError(NET_ERROR_CREATE_SOCKET, errnoToStr("setsockopt failed: SOL_SOCKET SO_REUSEADDR 1", getImplErrno()));
		return false;
	}
	optval = 0;

	//ignore return value, as this fails on linux when ipv6only is already 0
	setsockopt(rawSocket, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval));

	/* Bind and listen */
	  printf("ai_addr->sa_family: %d\n", ai->ai_addr->sa_family);
	  if (ai->ai_addr->sa_family == AF_INET6) {
		  struct sockaddr_in6* serveraddr = reinterpret_cast<struct sockaddr_in6*>(ai->ai_addr);
		  printf("ai_addr->sin6_family: %d\n", serveraddr->sin6_family);
	  }
	err = bind(rawSocket, ai->ai_addr, ai->ai_addrlen);
	if (err) {
		setError(NET_ERROR_BIND, errnoToStr("could not bind socket", getImplErrno()));
		return false;
	}
	err = listen(rawSocket, 511);
	if (err) {
		setError(NET_ERROR_LISTEN, errnoToStr("socket failed on listen", getImplErrno()));
		return false;
	}
	std::shared_ptr<network_socket_t> mainSocket = std::make_shared<network_socket_t>(io, rawSocket);
	mainSocket->handler = this->handler;
	mainSocket->flags |= SOCK_FLAG_ISLISTEN;
	mainSocket->port = port;
	mainSocket->initAddress();
	mainSocket->state = SOCK_STATE_LISTENING;
	this->netSockets.push_back(mainSocket);
	out = mainSocket;
	return true;
}

bool network_io::Impl::connectTo(const char *host, int port, std::shared_ptr<network_conn_t>& out) {
	struct addrinfo hints, *ai = nullptr;
	std::unique_ptr<addrinfo, addrinfo_deleter> deleter(ai);
	int err;
	char buf[64];

	printf("connectTo %s:%d\n", host, port);
	/* Resolve host */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(buf, 64, "%d", port);
	err = getaddrinfo(host, buf, &hints, &ai);
	if (err) {
		setError(NET_ERROR_RESOLVE_HOST, errnoToStr("failed resolving host", 0));
		return false;
	}


	/*
	struct addrinfo *it = ai;
	char bufa[INET6_ADDRSTRLEN];
	for (; it;) {
		int pos = (int)(it - ai);
		memset(buf, 0, INET6_ADDRSTRLEN);
		struct sockaddr_in* sock_addr_in = reinterpret_cast<struct sockaddr_in*>(it->ai_addr);
		inet_addrtocstr(ai->ai_family, &sock_addr_in->sin_addr, bufa, INET6_ADDRSTRLEN);
		printf("[%d].ai_family: %d\n", pos, ai->ai_family);
		printf("[%d].ai_socktype: %d\n", pos, ai->ai_socktype);
		printf("[%d].ai_protocol: %d\n", pos, ai->ai_protocol);
		printf("[%d].ai_canonname: %s\n", pos, ai->ai_canonname);
		printf("[%d].ai_addr: %s\n", pos, bufa);
		it = ai->ai_next;
	}
	*/
	sock_type_t rawSocket = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (rawSocket == INVALID_SOCKET) {
		setError(NET_ERROR_CREATE_SOCKET, errnoToStr("INVALID_SOCKET", getImplErrno()));
		close(rawSocket);
		return false;
	}
	std::shared_ptr<network_socket_t> mainSocket = std::make_shared<network_socket_t>(io, rawSocket);
	mainSocket->handler = this->handler;
	mainSocket->flags |= SOCK_FLAG_ISCLIENT;
	mainSocket->initAddress();
	int retVal;
	if ((retVal = connect(mainSocket->sockfd, ai->ai_addr, ai->ai_addrlen)) < 0) {
		int errCn = errno;
#if _WIN32
		if (errCn != WSAEWOULDBLOCK && errCn != WSAEINPROGRESS)
#else
		if (errCn != EINPROGRESS)
#endif
		{
			setError(NET_ERROR_CREATE_SOCKET, errnoToStr("cannot connect to host", errno));
			close(rawSocket);
			return false;
		}
	}
	double connectTimeout = 0.5;
 	fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(rawSocket, &fdset);
	struct timeval tv;
	setTimeout(tv, connectTimeout);
	if (select(rawSocket + 1, nullptr, &fdset, nullptr, &tv) > 0) {
		int optval = 0;
		socklen_t optlen = sizeof(optval);
		getsockopt(rawSocket, SOL_SOCKET, SO_ERROR, &optval, &optlen);
		if (optval != 0) {
			String s = errnoToStr("could not connect to server", optval);
			setError(NET_ERROR_CONNECT, s);
			close(rawSocket);
			return false;
		}
	} else {
		String s = errnoToStr("connect() timeout", 0);
		setError(NET_ERROR_CONNECT, s);
		close(rawSocket);
		return false;
	}

	mainSocket->state = SOCK_STATE_CONNECTED;
	this->netSockets.push_back(mainSocket);
	out = mainSocket;
	this->handler->onConnect(mainSocket);
	return true;
}


void network_socket_t::write(void* data, size_t size) {
	auto& wb = writeBuffer;
	size_t pos = wb.size();
	wb.reserve(pos + size);
	wb.resize(pos + size);
	memcpy(wb.data() + pos, data, size);
	flags |= SOCK_FLAG_WRITTEN;
}
void network_socket_t::flush() {
	handleWrite();
}
void network_socket_t::setSocketOpt(int a, int b, int opt) {
//	setsockopt(sockfd, a, b, &opt, sizeof(opt));
}
void network_socket_t::disconnect() {
	if (this->state != SOCK_STATE_CONNECTED) {
		closeSocket();
		return;
	}
	// close gracefully (flush write buffer)
	this->state = SOCK_STATE_CLOSING;
}
void network_socket_t::closeSocket() {
	if (this->state == SOCK_STATE_CLOSED)
		return;
	this->state = SOCK_STATE_CLOSED;
//	this->parent->_M_impl->onSocketClose(this);
	/* Close socket */
	if (this->sockfd != INVALID_SOCKET) {
		close(this->sockfd);
		this->sockfd = INVALID_SOCKET;
	}
	this->writeBuffer.clear();
}
int network_socket_t::handleWrite() {
	if (this->writeBuffer.size() > 0) {
		/* Send data */
		int size = send(this->sockfd, (const char*) this->writeBuffer.data(), (int) this->writeBuffer.size(), 0);
		if (size <= 0) {
			if (errno != EWOULDBLOCK) {
				this->closeSocket();
			}
			return 0;
		}
		if (size == (int) this->writeBuffer.size()) {
			this->writeBuffer.clear();
		} else {
			this->writeBuffer.erase(this->writeBuffer.begin(), this->writeBuffer.begin() + size);
		}
		/* Update status */
//    this->bytesSent += size;
//    this->lastActivity = dyad_getTime();
	}

	if (this->writeBuffer.size() == 0) {
		this->flags &= ~SOCK_FLAG_WRITTEN;
		if (this->state == SOCK_STATE_CLOSING) {
			this->closeSocket();
			return 0;
		}
	}
	return 1;

}
size_t network_socket_t::handleRead() {
	size_t totalRead = 0;
	for (;;) {
		/* Receive data */
//    dyad_Event e;
		char data[8192];
		int size = recv(this->sockfd, data, sizeof(data) - 1, 0);
		if (size <= 0) {
			if (size == 0 || errno != EWOULDBLOCK) {
				/* Handle disconnect */
				this->closeSocket();
			} else {
				/* No more data */
			}
			break;
		}
		totalRead += size;
		data[size] = 0;
		/* Update status */
//    this->bytesReceived += size;
//    this->lastActivity = dyad_getTime();
		dbgassert(this->handler);
		if (this->handler) {
			if (!this->handler->onReceive(data, size)) {
				//TODO: set error/notitfy
				this->disconnect();
				return 0;
			}
		}

	}
	return totalRead;
}
union sockaddr_ut {
	struct sockaddr sa;
	struct sockaddr_storage sas;
	struct sockaddr_in sai;
	struct sockaddr_in6 sai6;
};
void resolveAddr(sockaddr_ut& addr, String& address, int& port) {
	static_assert(INET_ADDRSTRLEN < INET6_ADDRSTRLEN, "INET_ADDRSTRLEN < INET6_ADDRSTRLEN");
	char buf[INET6_ADDRSTRLEN];
	if (addr.sas.ss_family == AF_INET6) {
		inet_addrtocstr(AF_INET6, &addr.sai6.sin6_addr, buf, INET6_ADDRSTRLEN);
		address = buf;
		port = ntohs(addr.sai6.sin6_port);
	} else {
		inet_addrtocstr(AF_INET, &addr.sai.sin_addr, buf, INET_ADDRSTRLEN);
		address = buf;
		port = ntohs(addr.sai.sin_port);
	}

}

void network_socket_t::initAddress() {
	sockaddr_ut addr;
	socklen_t size;
	memset(&addr, 0, sizeof(addr));
	size = sizeof(addr);
	if (getpeername(sockfd, &addr.sa, &size) == -1) {
		if (getsockname(sockfd, &addr.sa, &size) == -1) {
			return;
		}
	}
	resolveAddr(addr, address, port);
}
