
#include <memory>
#include <stdexcept>
#include <functional>
#include "assert_dbg.h"
#include "thread.h"
#include "tls.h"
#include "console_thread.h"
#include "threads.h"
#include "logging.h"

#include "cli/console/commandline_rep.h"

namespace NU {
namespace CONSOLE {

class ConsoleThread::Impl {
	NU::CONSOLE::CommandLineREP& commandREP;
	std::thread t;
	int32_t threadid = 0;
    daw_tls::tlsinstance threadTLS;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_running{false};
public:
    Impl(NU::CONSOLE::CommandLineREP& _cli) : commandREP(_cli) {
//		std::atomic_init(&m_stop, false);
	}
    ~Impl() {
        dbgassert(!m_running || m_stop); // joining of thread must be handled by caller
    }
    void setTls(daw_tls::tlsinstance tls) {
    	dbgassert(!t.joinable());
    	threadTLS = tls;
    }
	void start() {
		t = std::thread([this]() {
            m_running = true;
			daw_tls::setTls(threadTLS);
			setCurrentThreadName("CommandLineThread");
			this->run();
		});
	#ifdef _WIN32
			this->threadid = static_cast<int32_t>(get_thread_id());
//			HANDLE h = reinterpret_cast<HANDLE*>(t.native_handle());
	#endif
	}
	void join() {
		t.join();
	}
    int32_t getThreadId() {
    	return threadid;
    }

	void stop() {
		commandREP.getRunningState().shouldQuit = true;
		m_stop = true;
    }
	void init() {
		commandREP.init();
	}

    bool isStarted() {
        return m_running;
	}

private:
	void run() {
		daw_tls::setTls(threadTLS);
        while (!m_stop){
        	try {
        		commandREP.runConsole();
        	} catch(...) {
        		log_printf("exception on commandline thread\n", 0);
        	}
        }
        m_running = false;
	}
};
ConsoleThread::~ConsoleThread() {
	delete _M_impl;
}

ConsoleThread::ConsoleThread(NU::CONSOLE::CommandLineREP& cli) :
	_M_impl { new ConsoleThread::Impl { cli } } {
}

void ConsoleThread::startThread() {
	_M_impl->start();
}
void ConsoleThread::stopThread() {
	_M_impl->stop();
}
void ConsoleThread::joinThread() {
	_M_impl->join();
}
void ConsoleThread::init() {
	_M_impl->init();
}
int32_t ConsoleThread::getThreadId() {
	return _M_impl->getThreadId();
}
void ConsoleThread::setTls(daw_tls::tlsinstance tls)
{
    _M_impl->setTls(tls);
}
bool ConsoleThread::isStarted()
{
    return _M_impl->isStarted();
}

}
}
