#pragma once
#include <atomic>
#include "threads.h"

class ThreadLock {
public:
	class Impl;
private:
	Impl* _M_impl;
public:
	ThreadLock() = delete;
	ThreadLock(Impl* _M_impl);
	~ThreadLock();
	ThreadLock (const ThreadLock&) = delete;
	ThreadLock& operator= (const ThreadLock&) = delete;
	ThreadLock& operator=(ThreadLock&& other) noexcept;
	ThreadLock(ThreadLock&& other) noexcept;
	bool isLocked() const noexcept;
	static ThreadLock MakeThreadLock(std::recursive_mutex& _mutex, std::atomic<int32_t>& _isLocked, const bool bTryLock);
};

