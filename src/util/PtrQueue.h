/*
Copyright (c) 2017 Erik Rigtorp <erik@rigtorp.se>
Copyright (c) 2018 Michael Hept

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#pragma once

#include <atomic>
#include <cassert>
#include <stdexcept>
#include <type_traits>

namespace rigtorp {

template <typename T> class alignas(128) PointerQueueLockFree {
    static_assert(std::is_pointer<T>::value, "T must be pointer type");
    static_assert(std::is_nothrow_copy_constructible<T>::value, "T must be nothrow_copy_constructible");
    static_assert(std::is_nothrow_destructible<T>::value, "T must be is_nothrow_destructible");
public:
  explicit PointerQueueLockFree(const size_t capacity = 1<<4)
      : capacity_(capacity),
        slots_(capacity_ < 2 ? nullptr
                             : static_cast<T *>(operator new[](
                                   sizeof(T) * (capacity_ + 2 * kPadding)))),
        head_(0), tail_(0) {
    if (capacity_ < 2) {
      throw std::invalid_argument("size < 2");
    }
    assert(alignof(PointerQueueLockFree<T>) >= kCacheLineSize);
    assert((size_t)(reinterpret_cast<char *>(&tail_) -
               reinterpret_cast<char *>(&head_)) >=
           kCacheLineSize);
    memset(slots_, 0, sizeof(T) * (capacity_ + 2 * kPadding));
  }

  ~PointerQueueLockFree() {
    while (pop());
    operator delete[](slots_);
  }

  // non-copyable and non-movable
  PointerQueueLockFree(const PointerQueueLockFree &) = delete;
  PointerQueueLockFree &operator=(const PointerQueueLockFree &) = delete;

  bool push(T v) noexcept {
    auto const head = head_.load(std::memory_order_relaxed);
    auto nextHead = head + 1;
    if (nextHead == capacity_) {
      nextHead = 0;
    }
    if (nextHead == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    std::swap(slots_[head + kPadding], v);
    assert(v == nullptr);
    head_.store(nextHead, std::memory_order_release);
    return true;
  }

  T *front() noexcept {
    auto const tail = tail_.load(std::memory_order_relaxed);
    if (head_.load(std::memory_order_acquire) == tail) {
      return nullptr;
    }
    return &slots_[tail + kPadding];
  }

  T *back() noexcept {
    auto const tail = tail_.load(std::memory_order_relaxed);
    auto const head = head_.load(std::memory_order_acquire);
    if (head == tail) {
      return nullptr;
    }
    return &slots_[head + kPadding];
  }

  T pop() noexcept {
    auto const tail = tail_.load(std::memory_order_relaxed);
    if (head_.load(std::memory_order_acquire) == tail) {
      return nullptr;
    }
    T ptr = nullptr;
    std::swap(ptr, slots_[tail + kPadding]);
    auto nextTail = tail + 1;
    if (nextTail == capacity_) {
      nextTail = 0;
    }
    tail_.store(nextTail, std::memory_order_release);
    return ptr;
  }

  size_t size() const noexcept {
    ssize_t diff = head_.load(std::memory_order_acquire) -
                   tail_.load(std::memory_order_acquire);
    if (diff < 0) {
      diff += capacity_;
    }
    return diff;
  }

  bool empty() const noexcept { return size() == 0; }

  size_t capacity() const noexcept { return capacity_; }

private:
  static constexpr size_t kCacheLineSize = 128;

  // Padding to avoid false sharing between slots_ and adjacent allocations
  static constexpr size_t kPadding = (kCacheLineSize - 1) / sizeof(T) + 1;

private:
  const size_t capacity_;
  T *const slots_;

  // Align to avoid false sharing between head_ and tail_
  alignas(kCacheLineSize) std::atomic<size_t> head_;
  alignas(kCacheLineSize) std::atomic<size_t> tail_;

  // Padding to avoid adjacent allocations to share cache line with tail_
  char padding_[kCacheLineSize - sizeof(tail_)];
};
}
