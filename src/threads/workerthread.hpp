#pragma once
#include <memory>
#include <stdexcept>
#include <functional>
#include "assert_dbg.h"
#include "thread.hpp"
#include "tls.hpp"

class WorkerThread final : public seqthreads::thread_base {
private:
    class ThreadTaskImpl;
    class Impl;

public:
    class ThreadTask {
        friend class ThreadTaskImpl;
        friend class WorkerThread;

    public:
        ThreadTask();
        virtual ~ThreadTask();
        void setException(std::exception_ptr _eptr) {
            dbgassert(_eptr);
            this->eptr   = _eptr;
        }
        void clearException() {
            this->eptr = nullptr;
        }
        std::exception_ptr getException() const {
            return this->eptr;
        }
        bool hasException() const {
            return this->eptr != nullptr;
        }
        void logException();
        void wait();
        void reset();
        bool isCompleted();
        bool isCompletedNoLock();
        virtual void run() = 0;
        virtual void notifyCustom() {};

    private:
        ThreadTaskImpl* m_taskImpl;
        std::exception_ptr eptr = nullptr;
    };

    WorkerThread();
    ~WorkerThread() override;
    void setRealtimePriority(bool isRealtimePriority);
    void startThread(const String& name, seqthreads::ThreadType type);
    void stopThread();
    void joinThread();
    int32_t getThreadId() override;
#if BUILD_DAW_HOST
    void setTls(daw_tls::tlsinstance tls) override;
#endif
    bool pushTask(ThreadTask* task);
    [[nodiscard]] std::shared_ptr<WorkerThread::ThreadTask> call(std::function<void()>&& fn);

private:
    Impl* m_threadImpl;
};

class ThreadTaskCallStdFn final : public WorkerThread::ThreadTask {
    std::function<void()> fn;

public:
    explicit ThreadTaskCallStdFn(std::function<void()> _fn) : ThreadTask(), fn(_fn) {
    }
    void run() override {
        fn();
    }
};
