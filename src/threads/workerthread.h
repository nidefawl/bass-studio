#pragma once
#include <memory>
#include <stdexcept>

class WorkerThread
{
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
	    	this->status = status_exception;
			this->eptr = _eptr;
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
	    virtual void run() = 0;

	private:

	    //		std::unique_ptr<ThreadTaskImpl> _M_impl; // smart pointers do not work well with eclipse cdt
	    ThreadTaskImpl* _M_impl;
	    task_status status = status_init;
	    std::exception_ptr eptr = nullptr;
	};

	WorkerThread();
	~WorkerThread();
    void startThread();
    void stopThread();
	void joinThread();
    bool pushTask(ThreadTask* task);

private:
	Impl* _M_impl;
};

class ThreadTaskTest : public WorkerThread::ThreadTask {
public:
	ThreadTaskTest() : ThreadTask() {
	}
	int id = 0;
	int a = 0;
	int b = 0;
	int result = 0;
	void run() {
		result = a*b;
		//std::this_thread::sleep_for(std::chrono::milliseconds{ 120 });
//        LOG("work on ThreadTask %d", result);
        if (id == 3)
        	throw std::runtime_error("little error hihi");
	}
};
