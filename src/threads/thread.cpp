#include <thread>
#include <mutex>
#include "tls.h"
#include "types.h"
#include <unordered_map>
#include <atomic>
#include "str_util.h"
#include "assert_dbg.h"


namespace {
    struct threadlocal_threadinfo_t {
        int32_t threadId = 0;
        bool isKnownThread = false;
        bool isInternalThread = false;
        String threadName;
    };

    int32_t getNextThreadId() noexcept {
        static std::atomic<int32_t> thread_idx(0);
        return thread_idx.fetch_add(1, std::memory_order::memory_order_acquire);
    }

    thread_local threadlocal_threadinfo_t* tlsThreadInfo = nullptr;

    void registerThreadInternal(const String& threadName, bool isKnownThread, bool isInternalThread) {
        static std::mutex gRegisterMutex;
        std::lock_guard<std::mutex> lock(gRegisterMutex);
        dbgassert(!tlsThreadInfo || !tlsThreadInfo->isKnownThread);
        auto* threadInfo          = new threadlocal_threadinfo_t{};
        threadInfo->threadId      = getNextThreadId();
        threadInfo->threadName    = threadName + "-" + std::to_string(threadInfo->threadId);
        threadInfo->isKnownThread = isKnownThread;
        threadInfo->isInternalThread = isInternalThread;
        tlsThreadInfo             = threadInfo;
    }
}// namespace
namespace seqthreads {

    void registerThread(String threadName, bool isInternalThread) {
        registerThreadInternal(threadName, true, isInternalThread);
    }

    int32_t getCurrentThreadId() noexcept {
        threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
        return threadInfo ? threadInfo->threadId : -1;
    }
    bool isInternalThread() noexcept {
        threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
        return threadInfo && threadInfo->isInternalThread;
    }
    bool isKnownThread() noexcept {
        threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
        return threadInfo && threadInfo->isKnownThread;
    }
    void getThreadInfo(bool& isKnown, bool& isInternal) noexcept {
        threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
        if (threadInfo) {
            isKnown = threadInfo->isKnownThread;
            isInternal = threadInfo->isInternalThread;
        } else {
            dbgassert(!daw_tls::isTlsInitialized());
            isKnown = false;
            isInternal = false;
        }
    }

    String getCurrentThreadName() {
        threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
        if (!threadInfo) {
            registerThreadInternal("unknown", false, false);
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

}// namespace seqthreads
