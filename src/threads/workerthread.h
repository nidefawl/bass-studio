#pragma once
#include <memory>
#include <stdexcept>
#include <functional>
#include "assert_dbg.h"
#include "thread.h"
#include "tls.h"

class WorkerThread final : public seqthreads::thread_base {
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
        void setError() {
            this->status = status_error;
        }
        void setInQueue() {
            this->status = status_accepted;
        }
        void setCompleted() {
            this->status = status_complete;
        }
        std::exception_ptr getException() const {
            return this->eptr;
        }
        bool isInQueue() const {
            return this->status != status_init;
        }
        bool isError() const {
            return this->status >= status_error;
        }
        bool isGood() const {
            return this->status == status_complete;
        }
        void wait();
        void reset();
        bool isCompleted() const;
        virtual void run() = 0;
        virtual void notifyCustom() {};

    private:
        ThreadTaskImpl* m_taskImpl;
        task_status status      = status_init;
        std::exception_ptr eptr = nullptr;
    };

    WorkerThread();
    ~WorkerThread() override;
    void setRealtimePriority(bool isRealtimePriority);
    void startThread(const String& name, seqthreads::ThreadType type);
    void stopThread();
    void joinThread();
    int32_t getThreadId() override;
    void setTls(daw_tls::tlsinstance tls) override;
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
