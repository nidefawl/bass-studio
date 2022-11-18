#include <stdint.h>
#include <thread>

namespace seqthreads {
int32_t currentThreadsId() {
#ifndef __APPLE__
  return static_cast<int32_t>(std::this_thread::get_id().get());
#else
  return static_cast<int32_t>(pthread_mach_thread_np(pthread_self()));
#endif
}
}
