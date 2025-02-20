#include <vector>
#include "types.hpp"
#include <memory>
#include "logging.hpp"
#include "str_util.hpp"
#include "fileio.hpp"
#include "mouse.hpp"
#include "mousecursor.hpp"
#include "exceptions.hpp"
#include <GLFW/glfw3.h>
#include "assert_dbg.h"
#include "renderresources.hpp"


using ImgData = std::shared_ptr<uint8_t>;
namespace MouseCursors {
    MouseCursorIcon* cursors[NUM_CURSORS]{0};
    void initCursors() {
        {
            ImageBuf imgCursors[NUM_CURSORS];
#ifndef NDEBUG
            for (int i = 0; i < NUM_CURSORS; i++) {
                ImageBuf& buf = imgCursors[i];
                dbgassert((int)buf.bytes.size() == buf.w * buf.h * 4);
            }
#endif
            for (int i = 0; i < 6; i++) {
                auto path = StringFormat("cursors/cursor%02d.png", i);
                RenderResources::loadImageResource(StringAsCStr(path), imgCursors[i]);
            }
            cursors[0] = NULL;
            for (int i = 0; i < NUM_CURSORS; i++) {
                ImageBuf& buf = imgCursors[i];
                if (buf.w * buf.h == 0) {
                    continue;
                }
                int posx = buf.w / 2;
                int posy = buf.h / 2;
                if (i + 1 == CURSOR_DUPLICATE) {
                    posx = 0;
                    posy = 0;
                }
                if (i + 1 == CURSOR_CLIP_SIZE_LEFT) {
                    posx = 4;
                }
                if (i + 1 == CURSOR_CLIP_SIZE_RIGHT) {
                    posx = 12;
                }
                GLFWimage image;
                image.width    = buf.w;
                image.height   = buf.h;
                image.pixels   = &buf.bytes[0];
                cursors[i + 1] = glfwCreateCursor(&image, posx, posy);
            }
        }
    }
} // namespace MouseCursors
