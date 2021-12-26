#include <atomic>
#include <queue>
#include <exception>
#include <functional>
#include <memory>
#include <thread>
#include <condition_variable>
#include <mutex>
#include "thread.h"
#include "workerthread.h"
#include "assert_dbg.h"

class WorkerThread::ThreadTaskImpl {
    ThreadTask* task;
    std::mutex m_mtx;
    std::condition_variable m_cond;
    std::atomic<bool> m_finished{ false };

public:
    explicit ThreadTaskImpl(ThreadTask* _task)
        : task(_task) {
    }
    virtual ~ThreadTaskImpl() = default;
    void reset() {
        m_finished = false;
        task->clearException();
        task->status = status_init;
    }
    void run() {
        task->run();
    }
    void setException(std::exception_ptr eptr) {
        task->setException(eptr);
    }
    void setError() {
        task->status = status_error;
    }
    void setInQueue() {
        task->status = status_accepted;
    }
    void setCompleted() {
        task->status = status_complete;
    }
    bool isCompleted() {
        return m_finished;
    }
    void wait() {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (!m_finished) {
            m_cond.wait(lock, [&]() { return m_finished.load(); });
        }
    }
    void notify() {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_finished = true;
        m_cond.notify_all();
    }
};
WorkerThread::ThreadTask::~ThreadTask() {
    delete m_taskImpl;
}
WorkerThread::ThreadTask::ThreadTask()
    : m_taskImpl(new ThreadTaskImpl(this)) {
}
void WorkerThread::ThreadTask::wait() {
    this->m_taskImpl->wait();
}
void WorkerThread::ThreadTask::reset() {
    this->m_taskImpl->reset();
}
bool WorkerThread::ThreadTask::isCompleted() {
    return this->m_taskImpl->isCompleted();
}
class WorkerThread::Impl {
    std::thread t;
    std::queue<ThreadTaskImpl*> m_q;
    std::mutex m_mtx;
    std::condition_variable m_cond;
    std::atomic<bool> m_stop;
    int32_t threadid = 0;
    daw_tls::tlsinstance threadTLS;

public:
    Impl() {
        std::atomic_init(&m_stop, false);
    }
    void setTls(daw_tls::tlsinstance tls) {
        dbgassert(!t.joinable());
        threadTLS = tls;
    }
    void start() {
        t = std::thread([this]() {
            seqthreads::registerThread("workerthread");
#ifdef _WIN32
            this->threadid = seqthreads::getCurrentThreadId();
#endif
            dbgassert(threadTLS.tlsInitialized);
            daw_tls::setTls(threadTLS);
            this->run();
        });
    }
    void join() {
        t.join();
    }
    int32_t getThreadId() const {
        return threadid;
    }
    bool push(ThreadTaskImpl* task) {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (!m_stop) {
            task->setInQueue();
            m_q.push(task);
            m_cond.notify_one();
            return true;
        }
        return false;
    }

    void stop() {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_stop = true;
        m_cond.notify_all();
    }

private:
    void run() {
        while (true) {
            ThreadTaskImpl* task = pop();
            if (!task) {
                break;
            }
            try {
                task->run();
                task->setCompleted();
            } catch (...) {
                task->setException(std::current_exception());
            }
            task->notify();
        }
    }
    ThreadTaskImpl* pop() {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cond.wait(lock, [&]() { return !m_q.empty() || m_stop; });

        if (m_stop && m_q.empty()) {
            return nullptr;
        }

        ThreadTaskImpl* task = m_q.front();
        m_q.pop();
        return task;
    }
};
WorkerThread::~WorkerThread() {
    delete m_threadImpl;
}

WorkerThread::WorkerThread() : m_threadImpl{ new WorkerThread::Impl{} } {
}

void WorkerThread::startThread() {
    m_threadImpl->start();
}
void WorkerThread::stopThread() {
    m_threadImpl->stop();
}
void WorkerThread::joinThread() {
    m_threadImpl->join();
}
int32_t WorkerThread::getThreadId() {
    return m_threadImpl->getThreadId();
}
bool WorkerThread::pushTask(ThreadTask* task) {
    return this->m_threadImpl->push(task->m_taskImpl);
}
void WorkerThread::setTls(daw_tls::tlsinstance tls) {
    m_threadImpl->setTls(tls);
}
std::shared_ptr<WorkerThread::ThreadTask> WorkerThread::call(std::function<void()>&& fn) {
    std::shared_ptr<WorkerThread::ThreadTask> task = std::make_shared<ThreadTaskCallStdFn>(fn);
    m_threadImpl->push(task->m_taskImpl);
    return task;
}
