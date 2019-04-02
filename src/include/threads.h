#ifdef __linux__
#include <thread>
#include <condition_variable>
#endif
#ifdef __MINGW32__
#undef _GLIBCXX_HAS_GTHREADS
#include "../platform/mingw/mingw.thread.h"
#include <mutex>
#include "../platform/mingw/mingw.mutex.h"
#include "../platform/mingw/mingw.condition_variable.h"
#else
#include <mutex>
#endif
#include "str_util.h"

void setCurrentThreadName(String s); // util/debug.cpp
String getCurrentThreadName(); // util/debug.cpp
