#if defined(__linux__) || defined(_MSC_VER) || defined(_LIBCPP_VERSION) || (defined(_GLIBCXX_HAS_GTHREADS) && _GLIBCXX_HAS_GTHREADS)
#include <thread>
#include <condition_variable>
#include <mutex>
#else
#ifndef __MINGW32__
#error threads on this platform not supported
#endif
#include "../platform/mingw/mingw.thread.h"
#include <mutex>
#include "../platform/mingw/mingw.mutex.h"
#include "../platform/mingw/mingw.condition_variable.h"
#endif

#include "str_util.h"

void setCurrentThreadName(String s); // util/debug.cpp
String getCurrentThreadName(); // util/debug.cpp#include <mutex>
int32_t get_thread_id() noexcept;
