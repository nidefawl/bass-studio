#pragma once
#include "types.hpp"
#include "tls.hpp"
#include "str_util.hpp"
#ifdef __linux__
bool set_thread_priority_realtime() noexcept;
#endif

namespace seqthreads {
    enum class ThreadType {
        Unknown,
        MainThread,
        AudioThread,
        AudioThreadPool,
        WorkerThread,
        ChildProcess,
    };
    ThreadType CurrentThreadType() noexcept;
    int32_t getCurrentThreadId() noexcept;
    void registerThread(const String& threadName, ThreadType threadType = ThreadType::Unknown, bool isInternalThread = true);
    bool isInternalThread() noexcept;
    bool isKnownThread() noexcept;
    void getThreadInfo(bool& isKnown, bool& isInternal) noexcept;
    String getCurrentThreadName();

    void threadSleep(int32_t millis);
    void threadSleepMicros(int32_t microSeconds);


    class thread_base {
    public:
        virtual ~thread_base()= default;
        virtual int32_t getThreadId()                 = 0;
        virtual void setTls(daw_tls::tlsinstance tls) = 0;
    };
}// namespace seqthreads
