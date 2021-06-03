#pragma once
#include <memory>
#include <stdexcept>
#include <functional>
#include "assert_dbg.h"
#include "thread.h"
#include "tls.h"
#include "commandline_rep.h"
namespace NU {
namespace CONSOLE {
class ConsoleThread : public seqthreads::thread_base
{
    enum thread_status {
    	status_init,
    	status_running,
		status_ended,
    };
	thread_status status = status_init;
public:
	ConsoleThread() = delete;
	explicit ConsoleThread(NU::CONSOLE::CommandLineREP&);
	~ConsoleThread();
    void startThread();
    void stopThread();
	void joinThread();
	void init();
	int32_t getThreadId() override;
	void setTls(daw_tls::tlsinstance tls) override;
    bool isStarted();
private:
	class Impl;
	Impl* _M_impl;
};
}
}
