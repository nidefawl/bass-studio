#include "types.h"
#include <memory>
#include <vector>
#include <nanovg.h>

#include "str_util.h"
#include "fileio.h"
#include "platform.h"
#include "renderresources.h"
#include "guifonts.h"
#include "exceptions.h"

#include <GLFW/glfw3.h>

#include "assert_dbg.h"


using ImgData = std::shared_ptr<uint8_t>;

namespace {
    void setColor(uint8_t* b, int i) {
        b[0] = i & 0xFF;
        i    = i >> 8;
        b[1] = i & 0xFF;
        i    = i >> 8;
        b[2] = i & 0xFF;
        i    = i >> 8;
        b[3] = i & 0xFF;
        i    = i >> 8;
    };
    ImgData createDashedLineTexture(int w) {
        int size = w * w * 4;
        std::shared_ptr<uint8_t> imageData(new uint8_t[size], std::default_delete<uint8_t[]>());

        uint8_t* dataBuf = imageData.get();
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < w; y++) {
                int idx   = (x * w + y) * 4;
                int scale = 16;
                int ix    = x % scale;
                setColor(dataBuf + idx, ix < 10 ? 0xffffffff : 0x00ffffff);
            }
        }
        return imageData;
    }
} // namespace

namespace RenderResources {
    NvgImageTexture imgDashedLine;
    NvgImageTexture imgIcons[NUM_IMGS];
    std::unordered_map<NVGcontext*, NvgFonts> perContextFonts;
    std::vector<FontDesc> fontsInstalled;
    namespace {
        void load(NVGcontext* vg, String path, ImageBuf& out) {
            try {
                if (ReadImage(path, out) < 0) {
                    log_lf(Log::L_ERROR, "Error loading image %s\n", StringAsCStr(path));
                }
            } catch (appexception& e) {
                log_lf(Log::L_ERROR, "Failed loading image %s: %s\n", StringAsCStr(path), e.what());
            }
        }
    } // namespace
    void initResources(NVGcontext* vg) {
        {
            ImageBuf imgIconsBuf[NUM_IMGS];
            for (int i = 0; i < NUM_IMGS; i++) {
                ImageBuf& buf = imgIconsBuf[i];
                dbgassert((int)buf.bytes.size() == buf.w * buf.h * 4);
            }
            load(vg, StringFormat("icons/synth.png"), imgIconsBuf[ICON_SYNTH]);
            load(vg, StringFormat("icons/effect.png"), imgIconsBuf[ICON_EFFECT]);
            load(vg, StringFormat("icons/folder.png"), imgIconsBuf[ICON_FOLDER]);
            load(vg, StringFormat("icons/folder_open.png"), imgIconsBuf[ICON_FOLDER_OPEN]);
            load(vg, StringFormat("icons/file.png"), imgIconsBuf[ICON_FILE]);
            load(vg, StringFormat("icons/save.png"), imgIconsBuf[ICON_SAVE]);
            load(vg, StringFormat("icons/copy.png"), imgIconsBuf[ICON_COPY]);
            load(vg, StringFormat("icons/paste.png"), imgIconsBuf[ICON_PASTE]);
            load(vg, StringFormat("icons/cut.png"), imgIconsBuf[ICON_CUT]);
            load(vg, StringFormat("icons/adjust.png"), imgIconsBuf[ICON_ADJUST]);
            load(vg, StringFormat("icons/close.png"), imgIconsBuf[ICON_CLOSE]);
            load(vg, StringFormat("icons/bypass.png"), imgIconsBuf[ICON_BYPASS]);
            load(vg, StringFormat("icons/loop.png"), imgIconsBuf[ICON_LOOP]);
            load(vg, StringFormat("icons/arr_down.png"), imgIconsBuf[ICON_ARR_DOWN]);
            load(vg, StringFormat("icons/arr_left.png"), imgIconsBuf[ICON_ARR_LEFT]);
            load(vg, StringFormat("icons/arr_right.png"), imgIconsBuf[ICON_ARR_RIGHT]);
            load(vg, StringFormat("icons/arr_up.png"), imgIconsBuf[ICON_ARR_UP]);
            load(vg, StringFormat("icons/plus.png"), imgIconsBuf[ICON_PLUS]);
            load(vg, StringFormat("icons/minus.png"), imgIconsBuf[ICON_MINUS]);
            load(vg, StringFormat("icons/automation.png"), imgIconsBuf[ICON_AUTOMATION]);
            load(vg, StringFormat("led.png"), imgIconsBuf[IMG_LED]);
            load(vg, StringFormat("led_off.png"), imgIconsBuf[IMG_LED_OFF]);
            load(vg, StringFormat("led_glow.png"), imgIconsBuf[IMG_LED_GLOW]);
            load(vg, StringFormat("icons/speaker.png"), imgIconsBuf[ICON_SPEAKER]);
            load(vg, StringFormat("icons/x.png"), imgIconsBuf[ICON_X]);
            load(vg, StringFormat("icons/daw_icon.png"), imgIconsBuf[ICON_DAW_EXE]);
            load(vg, StringFormat("icons/opt_unlocked.png"), imgIconsBuf[ICON_OPT_UNLOCKED]);
            load(vg, StringFormat("icons/opt_locked.png"), imgIconsBuf[ICON_OPT_LOCKED]);
            load(vg, StringFormat("icons/midiplug.png"), imgIconsBuf[ICON_MIDIPLUG]);
            load(vg, StringFormat("icons/duplicate.png"), imgIconsBuf[ICON_DUPLICATE]);

            for (int i = 0; i < NUM_IMGS; i++) {
                ImageBuf& buf = imgIconsBuf[i];
                if (buf.w * buf.h == 0) {
                    continue;
                }
                dbgassert((int)buf.bytes.size() == buf.w * buf.h * 4);
                NvgImageTexture& nvgTex = imgIcons[i];
                int32_t nvgid = nvgCreateImageRGBA(vg, buf.w, buf.h, NVG_IMAGE_GENERATE_MIPMAPS, (const unsigned char*)buf.bytes.data());
                nvgImageSize(vg, nvgid, &nvgTex.width, &nvgTex.height);
                nvgTex.perContextId[vg] = nvgid;
            }
        }
        {
            int texSize   = 64;
            ImgData dataB = createDashedLineTexture(texSize);
            int32_t nvgid = nvgCreateImageRGBA(vg, texSize, texSize, NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY | NVG_IMAGE_NEAREST,
                                               (const unsigned char*)dataB.get());
            nvgImageSize(vg, nvgid, &imgDashedLine.width, &imgDashedLine.height);
            imgDashedLine.perContextId[vg] = nvgid;
        }
        {
            NvgFonts fonts;
            std::vector<FileFound> files;
            findFilesWithExt(App::Platform::toResourcePath("fonts/gui/"), "ttf", false, files);
            findFilesWithExt(App::Platform::toResourcePath("fonts/gui/"), "otf", false, files);
            if (files.empty()) {
                throw appexception("Please install ttf fonts to fonts/gui");
            }
            fonts.fontsInstalled.clear();
            fonts.fontsInstalled.resize(files.size());
            for (int i = 0; i < files.size(); i++) {
                fonts.fontsInstalled[i].name = files[i].name;
                fonts.fontsInstalled[i].path = files[i].path;
            }
            if (fontsInstalled.empty()) fontsInstalled = fonts.fontsInstalled;
            fonts.fontsLoaded.clear();
            String fntList;
            int loaded     = 0;
            for (int i = 0; i < MAX_FONTS && i < files.size(); i++) {
                String fontPath = (files[i].path);
                LoadedFont lf;
                String fntKey = StringFormat("font%d", i);
                log_lf(Log::L_DEBUG, "loading font %s %s\n", StringAsCStr(fntKey), StringAsCStr(fontPath));
                if (i == 0) {
                    lf.nvgId = nvgCreateFont(vg, StringAsCStr(fntKey), StringAsCStr(fontPath));
                } else {
                    lf.nvgId = -999;
                }
                lf.loaded    = lf.nvgId >= 0;
                lf.name      = files[i].name;
                lf.font.name = files[i].name;
                lf.font.path = files[i].path;
                if (lf.loaded) {
                    loaded++;
                    fntList += fntKey + ":" + lf.name + ",";
                }
                fonts.fontsLoaded.push_back(lf);
            }
            log_lf(Log::L_DEBUG, "loaded %d fonts: %s\n", loaded, StringAsCStr(fntList));
            perContextFonts[vg] = fonts;
        }
    }

} // namespace RenderResources
