#pragma once
#include <cstdint>
#include "tls.h"
#include "str_util.h"

namespace seqthreads {
    int32_t getCurrentThreadId() noexcept;
    void registerThread(String threadName);
    bool isInternalThread() noexcept;
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
