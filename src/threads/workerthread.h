#pragma once
#include <memory>
#include <stdexcept>
#include <functional>
#include "assert_dbg.h"
#include "thread.h"
#include "tls.h"

class WorkerThread : public seqthreads::thread_base {
public:
    enum task_status {
        status_init,
        status_accepted,
        status_complete,
        status_error,
        status_exception
    };

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
            this->status = status_exception;
            this->eptr   = _eptr;
        }
        void clearException() {
            this->eptr = nullptr;
        }
        std::exception_ptr getException() {
            return this->eptr;
        }
        void setError() {
            this->status = status_error;
        }
        bool isInQueue() {
            return this->status != status_init;
        }
        bool isError() {
            return this->status >= status_error;
        }
        bool isGood() {
            return this->status == status_complete;
        }
        void setInQueue() {
            this->status = status_accepted;
        }
        void setCompleted() {
            this->status = status_complete;
        }
        void wait();
        void reset();
        bool isCompleted();
        virtual void run() = 0;

    private:
        ThreadTaskImpl* m_taskImpl;
        task_status status      = status_init;
        std::exception_ptr eptr = nullptr;
    };

    WorkerThread();
    ~WorkerThread() override;
    void setRealtimePriority(bool isRealtimePriority);
    void startThread();
    void stopThread();
    void joinThread();
    int32_t getThreadId() override;
    void setTls(daw_tls::tlsinstance tls) override;
    bool pushTask(ThreadTask* task);
    [[nodiscard]] std::shared_ptr<WorkerThread::ThreadTask> call(std::function<void()>&& fn);

private:
    Impl* m_threadImpl;
};

class ThreadTaskCallStdFn : public WorkerThread::ThreadTask {
    std::function<void()> fn;

public:
    explicit ThreadTaskCallStdFn(std::function<void()> _fn) : ThreadTask(), fn(_fn) {
    }
    void run() override {
        fn();
    }
};
