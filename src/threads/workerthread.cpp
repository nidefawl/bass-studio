#include "workerthread.h"
#include "threads.h"

#include <atomic>
#include <queue>
#include "assert_dbg.h"
#include <exception>
#include <functional>
#include <memory>


#include "logging.h"
#define LOG(fmtString,...) printf(fmtString "\n", ##__VA_ARGS__); fflush(stdout)

class WorkerThread::ThreadTaskImpl {
    ThreadTask* task;
    std::mutex m_mtx;
    std::condition_variable m_cond;
    std::atomic<bool> m_finished{false};
public:
    ThreadTaskImpl(ThreadTask* _task)
	: task(_task)
	{
	}
    virtual ~ThreadTaskImpl() {
    }
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
            m_cond.wait(lock, [&](){ return m_finished == true; });
        }
	}
	void notify() {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_finished = true;
        m_cond.notify_all();
	}
};
WorkerThread::ThreadTask::~ThreadTask() {
	delete _M_impl;
}
WorkerThread::ThreadTask::ThreadTask()
: _M_impl( new ThreadTaskImpl(this) ) {
}
void WorkerThread::ThreadTask::wait() {
	this->_M_impl->wait();
}
void WorkerThread::ThreadTask::reset() {
	this->_M_impl->reset();
}
bool WorkerThread::ThreadTask::isCompleted() {
	return this->_M_impl->isCompleted();
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
			daw_tls::setTls(threadTLS);
			setCurrentThreadName("workerthread");
			this->run();
		});
	#ifdef _WIN32
			this->threadid = get_thread_id();
//			HANDLE h = reinterpret_cast<HANDLE*>(t.native_handle());
	#endif
	}
	void join() {
		t.join();
	}
    int32_t getThreadId() {
    	return threadid;
    }
    bool push(ThreadTaskImpl* task){
        std::unique_lock<std::mutex> lock(m_mtx);
        if (!m_stop) {
        	task->setInQueue();
            m_q.push(task);
            m_cond.notify_one();
        	return true;
        }
    	return false;
    }

    void stop(){
        std::unique_lock<std::mutex> lock(m_mtx);
    	m_stop = true;
        m_cond.notify_all();
    }
private:
	void run() {
        while (true){
        	ThreadTaskImpl* task = pop();
			if (!task) {
				break;
			}
			try {
				task->run();
				task->setCompleted();
			}
		    catch(...)
		    {
		        task->setException(std::current_exception());
		    }
			task->notify();
        }
	}
	ThreadTaskImpl* pop() {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cond.wait(lock, [&](){ return !m_q.empty() || m_stop; });

        if (m_stop && m_q.empty()){
        	return NULL;
        }

        ThreadTaskImpl* task = m_q.front();
        m_q.pop();
        return task;
    }
};
WorkerThread::~WorkerThread() {
	delete _M_impl;
}

WorkerThread::WorkerThread() :
	_M_impl { new WorkerThread::Impl {  } } {
}

void WorkerThread::startThread() {
	_M_impl->start();
}
void WorkerThread::stopThread() {
	_M_impl->stop();
}
void WorkerThread::joinThread() {
	_M_impl->join();
}
int32_t WorkerThread::getThreadId() {
	return _M_impl->getThreadId();
}
bool WorkerThread::pushTask(ThreadTask* task) {
	return this->_M_impl->push(task->_M_impl);
}
void WorkerThread::setTls(daw_tls::tlsinstance tls) {
	_M_impl->setTls(tls);
}
std::shared_ptr<WorkerThread::ThreadTask> WorkerThread::call(std::function<void()>&& fn) {
	std::shared_ptr<WorkerThread::ThreadTask> task = std::make_shared<ThreadTaskCallStdFn>(fn);
	_M_impl->push(task->_M_impl);
	return task;
}
