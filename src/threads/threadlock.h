#pragma once
#include <atomic>
#include <mutex>

class ThreadLock {
public:

    class Impl {
        std::recursive_mutex* m_mutex;
        std::atomic<int32_t>* m_extLockCount;
        bool m_isLocked = false;
    public:
        Impl(std::recursive_mutex* _mutex, std::atomic<int32_t>* _extLockCount, const bool bTryLock) noexcept
            : m_mutex(_mutex), m_extLockCount(_extLockCount) {
            m_isLocked = !m_mutex || !bTryLock || m_mutex->try_lock();
            if (m_mutex && !bTryLock) {
                m_mutex->lock();
            }
            if (m_extLockCount && m_isLocked) {
                (*m_extLockCount)++;
            }
        }
        ~Impl() {
            if (m_isLocked) {
                if (m_mutex)
                    m_mutex->unlock();
                if (m_extLockCount)
                    (*m_extLockCount)--;
            }
            //m_isLocked = false;
        }
        Impl(Impl&& other) noexcept : m_mutex(other.m_mutex), m_extLockCount(other.m_extLockCount), m_isLocked(other.m_isLocked) {
            other.m_isLocked = false;
            other.m_mutex = nullptr;
            other.m_extLockCount = nullptr;
        }
        Impl& operator=(Impl&& other) noexcept {
            if (this != &other) {
                m_mutex = other.m_mutex;
                m_extLockCount = other.m_extLockCount;
                m_isLocked = other.m_isLocked;
                other.m_isLocked = false;
                other.m_mutex = nullptr;
                other.m_extLockCount = nullptr;
            }
            return *this;
        }
        Impl(const Impl&)            = delete;
        Impl& operator=(const Impl&) = delete;
        bool isLocked() const noexcept {
            return this->m_isLocked;
        }
    };

private:
    Impl lockImpl;

public:
    ThreadLock() = delete;
    explicit ThreadLock(Impl&& lockImpl) noexcept;
    ThreadLock(const ThreadLock&)            = delete;
    ThreadLock& operator=(const ThreadLock&) = delete;
    ThreadLock& operator=(ThreadLock&& other) noexcept;
    ThreadLock(ThreadLock&& other) noexcept;
    bool isLocked() const noexcept;
    static ThreadLock MakeThreadLock(std::recursive_mutex& _mutex, std::atomic<int32_t>& _isLocked, const bool bTryLock) noexcept;
    static ThreadLock MakeVoidLock() noexcept;
};

class ThreadMutex {
    std::recursive_mutex mutex;
    std::atomic<int32_t> mLockCount{ 0 };

public:
    bool isLocked() const noexcept;
    ThreadLock lockThread() noexcept;
    ThreadLock tryLockThread() noexcept;
    void lockDirect() noexcept {
        mutex.lock();
        mLockCount++;
    }
    void unlockDirect() noexcept {
        mutex.unlock();
        mLockCount--;
    }
};
