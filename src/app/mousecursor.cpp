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


using ImgData = std::shared_ptr<uint8_t>;
namespace MouseCursors {
    MouseCursorIcon* cursors[NUM_CURSORS]{0};
    namespace {
        void load(const String& path, ImageBuf& out) {
            try {
                if (ReadImage(path, out) < 0) {
                    log_lf(Log::L_ERROR, "Error loading image %s\n", StringAsCStr(path));
                }
            } catch (std::exception& e) {
                log_lf(Log::L_ERROR, "Failed loading cursor %s: %s\n", StringAsCStr(path), e.what());
            }
        }
    } // namespace
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
                load(StringFormat("cursors/cursor%02d.png", i), imgCursors[i]);
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
