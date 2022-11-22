#include "types.h"
#include "assert_dbg.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#include <stdint.h>
#include <thread>
#include "platform/linux/windowsize.h"

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

bool restoreWindowPos(GLFWwindow* glfw, appwindow_size_t* placement) {
    if (!placement->valid) {
        return false;
    }

    id cocoaWindow = glfwGetCocoaWindow(glfw);
    if (!cocoaWindow) {
        return false;
    }

    NSRect frame = [cocoaWindow frame];
    frame.origin.x = placement->x;
    frame.origin.y = placement->y;
    frame.size.width = placement->w;
    frame.size.height = placement->h;
    [cocoaWindow setFrame:frame display:YES];
    return true;
}

bool saveWindowPos(GLFWwindow* glfw, appwindow_size_t* placement) {
    *placement = appwindow_size_t{};

    id cocoaWindow = glfwGetCocoaWindow(glfw);
    if (!cocoaWindow) {
        return false;
    }

    NSRect frame = [cocoaWindow frame];
    placement->valid = true;
    placement->x = frame.origin.x;
    placement->y = frame.origin.y;
    placement->w = frame.size.width;
    placement->h = frame.size.height;
    return true;
}
