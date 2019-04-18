#pragma once
#include <stdint.h>
#include "tls.h"

namespace seqthreads {
	int32_t currentThreadsId();
	class thread_base {
	public:
		virtual ~thread_base() { };
		virtual int32_t getThreadId() = 0;
		virtual void setTls(daw_tls::tlsinstance tls) = 0;
	};
}

