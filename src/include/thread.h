#pragma once
#include <stdint.h>

namespace seqthreads {
	int32_t currentThreadsId();
	class thread_base {
	public:
		virtual ~thread_base() { };
		virtual int32_t getThreadId() = 0;
	};
}

