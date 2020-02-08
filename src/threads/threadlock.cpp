#include "threadlock.h"
#include <chrono>
#include <atomic>
#include <queue>
#include "threads.h"

#include "error.h"

#ifndef _MSC_VER
#pragma GCC diagnostic push
#endif
#ifdef __clang__
#pragma clang diagnostic ignored "-Wpessimizing-move"
#endif

class ThreadLock::Impl {
	std::recursive_mutex* mutex;
	std::atomic<bool>* isLocked;
public:
	Impl() = delete;
	Impl(std::recursive_mutex& _mutex, std::atomic<bool>& _isLocked)
		: mutex(&_mutex), isLocked(&_isLocked)
	{
		*isLocked = true;
		mutex->lock();
	}
	~Impl() {
		mutex->unlock();
		*isLocked = false;
	}
    Impl ( Impl && ) = default;
    Impl &  operator= ( Impl && ) = default;
    Impl ( const Impl & ) = delete;
    Impl & operator= ( const Impl & ) = delete;
};
/*static*/ ThreadLock ThreadLock::MakeThreadLock(std::recursive_mutex& _mutex, std::atomic<bool>& _isLocked) {
	return ThreadLock(new Impl(_mutex, _isLocked));
//	return ThreadLock(nullptr);
}


ThreadLock::ThreadLock(ThreadLock::Impl* impl) :
	_M_impl(impl) {
}
ThreadLock::~ThreadLock() {
	if (_M_impl)
		delete _M_impl;
}

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
