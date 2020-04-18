
#include <memory>
#include <vector>
#include <string>
#include <iostream>

#include "host/mainctrl.h"
#include "threads.h"
#include "platform.h"
#include "fileio.h"
#include "net/network.h"
#include "net/packet.h"
#include "net/stream/audiostream.h"

#include "commandline_rep.h"
#include "js/scripting.h"
#include "js/interface/duk_daw_interface.h"

#ifdef _WIN32
#include <windows.h>
#endif


bool consumeLineFromVector(std::vector<uint8_t>& buffer, String& input) {
	auto endN = std::find(buffer.begin(), buffer.end(), '\n');
	if (endN != buffer.end()) {
		if (endN != buffer.begin()) {
			auto endPos = endN;
			if (*(endN-1) == '\r') {
				endPos--;
			}
			input.assign(buffer.begin(), endPos);
		}
		auto endPos = endN;
		if (endN+1 != buffer.end()&&*(endN+1) == '\n') {
			endPos++;
		}
		buffer.erase(buffer.begin(), endPos+1);
		return true;
	}
	return false;
}

namespace {

class tcp_handler_cli_client : public inetwork_handler {
public:
	std::shared_ptr<network_conn_t> conn;
	std::vector<uint8_t> buf;
	bool connected = false;
	tcp_handler_cli_client() = delete;
	tcp_handler_cli_client(std::shared_ptr<network_conn_t> _conn) : conn(_conn) {

	}
	void onError(int errorType, String msg) override {
		my_printf("Error %s\n", StringAsCStr(msg));
		connected = false;
	}
	bool onReceive(void* data, size_t size) override {
		buf.insert(buf.end(), reinterpret_cast<uint8_t*>(data), reinterpret_cast<uint8_t*>(data)+size);
		return true;
	}
	void onConnect(std::shared_ptr<network_conn_t> conn) override {
#if defined(IPPROTO_TCP) && defined(TCP_NODELAY)
		conn->setSocketOpt(IPPROTO_TCP, TCP_NODELAY, 1);
#endif
		conn->parent->setSelectTimeout(0.0001);
		my_printf("connected\n", 0);
		connected = true;
	}
	void onDisconnect(std::shared_ptr<network_conn_t> conn) override {
		my_printf("disconnected\n", 0);
		connected = false;
	}

	void writeBuffer(void* buf, size_t size) {
		conn->write(buf, size);
	}
	void handlePackets() {
		if (buf.size())
			my_printf("process client recv buf of size %d\n", buf.size());
		buf.clear();
	}
};
class tcp_handler_cli_server : public inetwork_handler {
public:
	struct cli_server_client_conn_t {
		std::shared_ptr<network_conn_t> conn;
		std::shared_ptr<tcp_handler_cli_client> handler;
	};
	std::vector<std::shared_ptr<cli_server_client_conn_t>> conns;
	bool connected = false;
	tcp_handler_cli_server() {

	}
	void writeBufferToAll(const String& strBuf) {
		writeBufferToAll((void*)strBuf.c_str(), strBuf.length());
	}
	void writeBufferToAll(void* buf, size_t size) {
		for (auto& pc : conns) {
			pc->handler->writeBuffer(buf, size);
		}
	}
	void onError(int errorType, String msg) override {
		my_printf("Error %s\n", StringAsCStr(msg));
		connected = false;
	}
	bool onReceive(void* data, size_t size) override {
		dbgassert(0); // not called
		return 0;
	}
	void onConnect(std::shared_ptr<network_conn_t> conn) override {
#if defined(IPPROTO_TCP) && defined(TCP_NODELAY)
		conn->setSocketOpt(IPPROTO_TCP, TCP_NODELAY, 1);
#endif
		conn->parent->setSelectTimeout(0.0001);
		my_printf("connected\n", 0);
		connected = true;
	}
	void onDisconnect(std::shared_ptr<network_conn_t> conn) override {
		my_printf("disconnected\n", 0);
		connected = false;
	}
	virtual bool onAccept(std::shared_ptr<network_conn_t> clientConn) {
		my_printf("onAccept %s\n", StringAsCStr(clientConn->address));
		auto handler = std::make_shared<tcp_handler_cli_client>(clientConn);
		clientConn->handler = handler.get();
		conns.push_back(std::shared_ptr<cli_server_client_conn_t>(new cli_server_client_conn_t{clientConn, handler}));
		return true;
	}
};
}
namespace NU {
namespace CONSOLE {
class CommandLineREP_TCP::CLIImpl {
	JSContext jsContext;
	std::vector<std::string> commandQueue;
	std::atomic_bool hasCommands{false};
	std::recursive_mutex mutex;
	tcp_handler_cli_server handler;
	rep_running_state& threadState;
public:
	CLIImpl(rep_running_state& _threadState) : threadState(_threadState) {
	}
	~CLIImpl() {
	}
protected:

public:
	void init() {
		network_init();
	}
	int runConsole() {
		threadState.isRunning = true;
		network_io netio(&handler);
		std::shared_ptr<network_conn_t> conn;
		std::shared_ptr<network_conn_t> listenSocket;
		if (!netio.listenAt(nullptr, 2123, protocol_type_i32::TCP, listenSocket)) {
			log_printf("Could not open listening socket on TCP port 2123\n", 0);
			while (!threadState.shouldQuit) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		} else {
			netio.setSelectTimeout(0.0001);
			while (netio.hasOpenSockets() && !threadState.shouldQuit) {
				netio.update();

				threadState.isConnected = handler.conns.size() > 0;
				for (auto& pc : handler.conns) {
					if (!pc->handler->buf.size()) {
						continue;
					}
					std::string strBuf;
					if (consumeLineFromVector(pc->handler->buf, strBuf) && strBuf.length()) {
						std::lock_guard<std::recursive_mutex> lock(mutex);
						commandQueue.push_back(std::move(strBuf));
						hasCommands = true;
					}
				}
			}
		}
		threadState.isRunning = false;
		return 0;
	}
	int executeCommands() {
		if (!hasCommands)
			return 0;
		std::vector<std::string> tmpCommandQueue;
		{
			std::lock_guard<std::recursive_mutex> lock(mutex);
			//TODO: move from
			tmpCommandQueue = commandQueue;
			commandQueue.clear();
			hasCommands= false;
		}
//		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		for (std::string& str : tmpCommandQueue) {
			if (str.length() == 0) {
				continue;
			}
			log_out("js input: '%s'\n", str.c_str());
			call_context_t ctxt;
			String response = jsContext.eval(str, ctxt);
			if (response.length()) {
				handler.writeBufferToAll(response);
				fwrite(response.c_str(), response.length(), 1, stdout);
				fflush(stdout);
			}
		}
		return 0;
	}

};
CommandLineREP_TCP::CommandLineREP_TCP() : m_impl(new CommandLineREP_TCP::CLIImpl{runState}) {
}
CommandLineREP_TCP::~CommandLineREP_TCP() {
	delete m_impl;
}
int CommandLineREP_TCP::runConsole() {
	return m_impl->runConsole();
}
void CommandLineREP_TCP::init() {
	return m_impl->init();
}
int CommandLineREP_TCP::executeCommands() {
	return m_impl->executeCommands();
}

}
}

