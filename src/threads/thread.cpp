#include "types.hpp"
#include "compiler.hpp"
#include "thread.hpp"
#include "str_util.hpp"
#include "assert_dbg.h"
#include <mutex>
#include <atomic>
#include <thread>


namespace {
    struct threadlocal_threadinfo_t {
        int32_t threadId = 0;
        seqthreads::ThreadType threadType = seqthreads::ThreadType::Unknown;
        bool isKnownThread = false;
        bool isInternalThread = false;
        char threadName[32] = {0};
    };

    int32_t getNextThreadId() noexcept {
        static std::atomic<int32_t> thread_idx(0);
        return thread_idx.fetch_add(1, std::memory_order_acquire);
    }

    DAW_CXX_CONSTINIT thread_local threadlocal_threadinfo_t threadPrivateTls;
    DAW_CXX_CONSTINIT thread_local threadlocal_threadinfo_t* tlsThreadInfo = nullptr;

    void registerThreadInternal(const String& threadName, bool isKnownThread, bool isInternalThread, seqthreads::ThreadType threadType) {
        static std::mutex gRegisterMutex;
        std::lock_guard<std::mutex> lock(gRegisterMutex);
        dbgassert(!tlsThreadInfo || !tlsThreadInfo->isKnownThread);
        auto* threadInfo          = &threadPrivateTls;
        threadInfo->threadId      = getNextThreadId();
        threadInfo->threadType    = threadType;
        const auto threadNameWithId = threadName + "-" + std::to_string(threadInfo->threadId);
        safe_strcpy(threadInfo->threadName, threadNameWithId);
        threadInfo->isKnownThread = isKnownThread;
        threadInfo->isInternalThread = isInternalThread;
        tlsThreadInfo             = threadInfo;
    }
}// namespace
namespace seqthreads {

    void registerThread(const String& threadName, ThreadType threadType, bool isInternalThread) {
        registerThreadInternal(threadName, true, isInternalThread, threadType);
    }

    int32_t getCurrentThreadId() noexcept {
        threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
        return threadInfo ? threadInfo->threadId : -1;
    }
    ThreadType CurrentThreadType() noexcept {
        threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
        return threadInfo ? threadInfo->threadType : ThreadType::Unknown;
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
            isKnown = false;
            isInternal = false;
        }
    }

    String getCurrentThreadName() {
        threadlocal_threadinfo_t* threadInfo = tlsThreadInfo;
        if (!threadInfo) {
            registerThreadInternal("unknown", false, false, seqthreads::ThreadType::Unknown);
            threadInfo = tlsThreadInfo;
        }
        return threadInfo && threadInfo->threadName[0] ? threadInfo->threadName : "unknown";
    }

    void threadSleep(int32_t millis) {
        std::this_thread::sleep_for(std::chrono::milliseconds(millis));
    }

    void threadSleepMicros(int32_t microSeconds) {
        std::this_thread::sleep_for(std::chrono::microseconds(microSeconds));
    }

}// namespace seqthreads
