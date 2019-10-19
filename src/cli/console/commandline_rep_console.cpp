#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <conio.h>
#include "js/interface/duk_daw_interface.h"
#include "commandline_rep.h"
#include "fileio.h"
#include "host/mainctrl.h"
#include "threads.h"
#include "platform.h"
#include "js/scripting.h"
#include <windows.h>
#define  LINEBUF_SIZE       65536

namespace NU {
namespace CONSOLE {
class CommandLineREP_Console::CLIImpl {
	std::vector<std::string> commandQueue;
	std::atomic_bool hasCommands{false};
	std::recursive_mutex mutex;
	std::array<char, LINEBUF_SIZE> inputBuffer;
	int bufferIdx = 0;
	rep_running_state& threadState;

	JSContext jsContext;
public:
	CLIImpl(rep_running_state& _threadState) : threadState(_threadState) {

	}
	~CLIImpl() {
	}
protected:
public:
	void init() {
		String srcJS;
		int64_t ret = ReadFileText("daw_context_init.js", srcJS);
		if (ret <= 0) {
			my_printf("failed loading %s\n", StringAsCStr(srcJS));

			call_context_t ctxt;
			String response = jsContext.eval(srcJS, ctxt);
			if (response.length()) {
				fwrite(response.c_str(), response.length(), 1, stdout);
				fflush(stdout);
			}
		}
	}
	int runConsole() {
		bool got_eof = false;
		while (_kbhit()&&!threadState.shouldQuit) {
			char c = _getch();
			if (c == EOF) {
				got_eof = 1;
				return 0;
			} else if (c == '\n'||c == '\r') {
				if (bufferIdx > 0) {
					std::string strcommand;
					strcommand.assign(inputBuffer.data(), bufferIdx);
					memset(inputBuffer.data(), 0, bufferIdx);
					bufferIdx = 0;
					std::lock_guard<std::recursive_mutex> lock(mutex);
					commandQueue.push_back(std::move(strcommand));
					hasCommands = true;
				}
				return 1;
			} else if (bufferIdx >= inputBuffer.size()) {
				log_printf("Input exceeds buffer size!\n", 0);
				return -1;
			} else {
				inputBuffer[bufferIdx++] = (char) c;
			}
		}
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
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		for (std::string& str : tmpCommandQueue) {
			if (str.length() == 0) {
				continue;
			}
			log_out("js input: '%s'\n", str.c_str());
			call_context_t ctxt;
			String response = jsContext.eval(str, ctxt);
			if (response.length()) {
				fwrite(response.c_str(), response.length(), 1, stdout);
				fflush(stdout);
			}
		}
		return 0;
	}

};
CommandLineREP_Console::CommandLineREP_Console() : m_impl(new CommandLineREP_Console::CLIImpl{runState}) {
}
CommandLineREP_Console::~CommandLineREP_Console() {
	delete m_impl;
}
int CommandLineREP_Console::runConsole() {
	return m_impl->runConsole();
}
void CommandLineREP_Console::init() {
	return m_impl->init();
}
int CommandLineREP_Console::executeCommands() {
	return m_impl->executeCommands();
}

}
}

