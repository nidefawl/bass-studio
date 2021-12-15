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
	std::recursive_mutex* m_mutex;
	std::atomic<int32_t>* m_extLockCount;
	bool m_isLocked;
public:
	Impl() = delete;
	Impl(std::recursive_mutex& _mutex, std::atomic<int32_t>& _extLockCount, const bool bTryLock)
		: m_mutex(&_mutex), m_extLockCount(&_extLockCount), m_isLocked(false)
	{
		m_isLocked = !bTryLock || m_mutex->try_lock();
		if (!bTryLock) {
			m_mutex->lock();
		}
		if (m_isLocked) {
			(*m_extLockCount)++;
		}
	}
	~Impl() {
		if (m_isLocked) {
			m_mutex->unlock();
			(*m_extLockCount)--;
		}
		//m_isLocked = false;
	}
    Impl ( Impl && ) = default;
    Impl &  operator= ( Impl && ) = default;
    Impl ( const Impl & ) = delete;
    Impl & operator= ( const Impl & ) = delete;
	bool isLocked() const {
		return this->m_isLocked;
	}
};
/*static*/ ThreadLock ThreadLock::MakeThreadLock(std::recursive_mutex& _mutex, std::atomic<int32_t>& _isLocked, const bool bTryLock) {
	return ThreadLock(new Impl(_mutex, _isLocked, bTryLock));
//	return ThreadLock(nullptr);
}

ThreadLock::ThreadLock(ThreadLock::Impl* impl) :
	_M_impl(impl) {
}
ThreadLock::~ThreadLock() {
	if (_M_impl)
		delete _M_impl;
}
bool ThreadLock::isLocked() const noexcept {
	return this->_M_impl->isLocked();
}

bool ThreadMutex::isLocked() {
	return this->mLockCount > 0;
}

ThreadLock ThreadMutex::lockThread() {
	ThreadLock t = ThreadLock::MakeThreadLock(mutex, this->mLockCount, false);
	return std::move(t); //CANNOT RELY ON RVO
}

ThreadLock ThreadMutex::tryLockThread() {
	ThreadLock t = ThreadLock::MakeThreadLock(mutex, this->mLockCount, true);
	return std::move(t); //CANNOT RELY ON RVO
}

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
