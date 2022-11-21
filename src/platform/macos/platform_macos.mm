#include "types.h"
#include "assert_dbg.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
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
void sendExposeEvent(GLFWwindow* glfw) {
  if (!assert_expr(glfw)) return;
  id handle = glfwGetCocoaWindow(glfw);
  if (!assert_expr(handle)) return;
  [handle display];
}