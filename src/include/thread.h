#pragma once
#include "types.h"
#include "tls.h"
#include "str_util.h"
#ifdef __linux__
bool set_thread_priority_realtime() noexcept;
#endif

namespace seqthreads {
    int32_t getCurrentThreadId() noexcept;
    void registerThread(String threadName, bool isInternalThread = true);
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
