#include "threadlock.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "error.hpp"

/*static*/ ThreadLock ThreadLock::MakeThreadLock(std::recursive_mutex& _mutex, std::atomic<int32_t>& _isLocked, const bool bTryLock) noexcept {
    return ThreadLock(ThreadLock::Impl{&_mutex, &_isLocked, bTryLock});
}
/* static */ ThreadLock ThreadLock::MakeVoidLock() noexcept {
    return ThreadLock(ThreadLock::Impl{nullptr, nullptr, false});
}

ThreadLock::ThreadLock(ThreadLock::Impl&& impl) noexcept : lockImpl(std::move(impl)) {
}
bool ThreadLock::isLocked() const noexcept {
    /* impl is null for void locks */
    return lockImpl.isLocked();
}

bool ThreadMutex::isLocked() const noexcept {
    return this->mLockCount > 0;
}

ThreadLock ThreadMutex::lockThread() noexcept {
    return ThreadLock::MakeThreadLock(mutex, this->mLockCount, false);
}

ThreadLock ThreadMutex::tryLockThread() noexcept {
    return ThreadLock::MakeThreadLock(mutex, this->mLockCount, true);
}

ThreadLock& ThreadLock::operator=(ThreadLock&& other) noexcept {
    std::swap(lockImpl, other.lockImpl);
    return *this;
}

ThreadLock::ThreadLock(ThreadLock&& other) noexcept : lockImpl(std::move(other.lockImpl)) {
    other.lockImpl = {nullptr, nullptr, false};
}
