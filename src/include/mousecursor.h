#pragma once
#include "fileio.h"
#define NUM_CURSORS 16

#ifdef NO_GLFW_LIB
namespace MouseCursors {
    struct AppMouseCursor;
    using MouseCursorIcon = AppMouseCursor;
    extern MouseCursorIcon* cursors[NUM_CURSORS];
};// namespace MouseCursors
#else
#include <GLFW/glfw3.h>
namespace MouseCursors {
    using MouseCursorIcon = GLFWcursor;
    extern MouseCursorIcon* cursors[NUM_CURSORS];
};// namespace MouseCursors
#endif
