#pragma once
#include <atomic>
#include <mutex>

class ThreadLock {
public:
    class Impl;

private:
    Impl* _M_impl;

public:
    ThreadLock() = delete;
    explicit ThreadLock(Impl* _M_impl);
    ~ThreadLock();
    ThreadLock(const ThreadLock&)            = delete;
    ThreadLock& operator=(const ThreadLock&) = delete;
    ThreadLock& operator=(ThreadLock&& other) noexcept;
    ThreadLock(ThreadLock&& other) noexcept;
    bool isLocked() const noexcept;
    static ThreadLock MakeThreadLock(std::recursive_mutex& _mutex, std::atomic<int32_t>& _isLocked, const bool bTryLock);
};

//TODO: make hidden implementation to avoid mutex and atomic includes
class ThreadMutex {
    std::recursive_mutex mutex;
    std::atomic<int32_t> mLockCount{ 0 };

public:
    bool isLocked();
    ThreadLock lockThread();
    ThreadLock tryLockThread();
};
