#include <thread>
#include <mutex>
#include <stdint.h>
#include <unordered_map>
#include <atomic>
#include "str_util.h"
#include "assert_dbg.h"


namespace
{
	struct threadlocal_threadinfo_t {
		int32_t threadId = 0;
		String threadName = "";
		bool isKnownThread = false;
	};

	int32_t getNextThreadId() noexcept {
	    static std::atomic<int32_t> thread_idx(0);
	    return thread_idx.fetch_add(1, std::memory_order::memory_order_acquire);
	}

	thread_local threadlocal_threadinfo_t* tlsThreadInfo = nullptr;

	void registerThreadInternal(String threadName, bool isKnownThread) {
		static std::mutex gRegisterMutex;
		std::lock_guard<std::mutex> lock(gRegisterMutex);
		dbgassert(!tlsThreadInfo || !tlsThreadInfo->isKnownThread);
		threadlocal_threadinfo_t* threadInfo = new threadlocal_threadinfo_t{};
		threadInfo->threadId = getNextThreadId();
		threadInfo->threadName = threadName + StringFormat("-%d", threadInfo->threadId);
		threadInfo->isKnownThread = isKnownThread;
		tlsThreadInfo = threadInfo;
	}
}
namespace seqthreads {

void registerThread(String threadName) {
	registerThreadInternal(threadName, true);
}

int32_t getCurrentThreadId() noexcept {
	threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
	return threadInfo ? threadInfo->threadId : -1;
}
bool isInternalThread() noexcept {
	threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
	return threadInfo && threadInfo->isKnownThread;
}

String getCurrentThreadName() {
	threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
	if (!threadInfo) {
		registerThreadInternal("unknown", false);
		threadInfo = tlsThreadInfo;
	}
	return threadInfo ? threadInfo->threadName : "unknown";
}

void threadSleep(int32_t millis) {
	std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

void threadSleepMicros(int32_t microSeconds) {
	std::this_thread::sleep_for(std::chrono::microseconds(microSeconds));
}

}

