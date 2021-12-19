#pragma once
#include <stdint.h>
#include "tls.h"
#include "str_util.h"

namespace seqthreads {
	int32_t get_thread_id() noexcept;
	void threadSleep(int32_t millis);
	void threadSleepMicros(int32_t microSeconds);

	void setCurrentThreadName(String s);
	String getCurrentThreadName();

	class thread_base {
	public:
		virtual ~thread_base() { };
		virtual int32_t getThreadId() = 0;
		virtual void setTls(daw_tls::tlsinstance tls) = 0;
	};
}

