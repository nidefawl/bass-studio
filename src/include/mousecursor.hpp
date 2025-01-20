#pragma once
#define NUM_CURSORS 16
#include <GLFW/glfw3.h>
namespace MouseCursors {
    using MouseCursorIcon = GLFWcursor;
    extern MouseCursorIcon* cursors[NUM_CURSORS];
};// namespace MouseCursors
