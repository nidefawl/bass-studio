#include "logging.h"
#include "types.h"
#include <exception>
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
const SupportedFileTypes FILE_TYPES_IMAGES = SupportedFileTypes{PROJECT_FILE_TYPE_DESC "s", { SupportedFileType{ "jpg", "jpg" }, SupportedFileType{ "png", "png" } } };

namespace {
    void setColor(uint8_t* b, uint32_t i) {
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
    std::unordered_map<String, NvgImageTexture> perContextTextures;
    namespace {
        void load(NVGcontext* vg, const char* path, ImageBuf& out) {
            try {
                if (ReadImage(path, out) < 0) {
                    log_lf(Log::L_ERROR, "Error loading image %s\n", path);
                }
            } catch (FileIOException& e) {
                log_lf(Log::L_ERROR, "Failed loading image %s: %s\n", path, e.what());
            }
        }
    } // namespace
    void initResources(NVGcontext* vg) {
        {
            ImageBuf imgIconsBuf[NUM_IMGS];
#ifndef NDEBUG
            for (int i = 0; i < NUM_IMGS; i++) {
                ImageBuf& buf = imgIconsBuf[i];
                dbgassert((int)buf.bytes.size() == buf.w * buf.h * 4);
            }
#endif
            //TODO: organize this betters
            load(vg, "icons/synth.png", imgIconsBuf[ICON_SYNTH]);
            load(vg, "icons/effect.png", imgIconsBuf[ICON_EFFECT]);
            load(vg, "icons/folder.png", imgIconsBuf[ICON_FOLDER]);
            load(vg, "icons/folder_open.png", imgIconsBuf[ICON_FOLDER_OPEN]);
            load(vg, "icons/file.png", imgIconsBuf[ICON_FILE]);
            load(vg, "icons/save.png", imgIconsBuf[ICON_SAVE]);
            load(vg, "icons/copy.png", imgIconsBuf[ICON_COPY]);
            load(vg, "icons/paste.png", imgIconsBuf[ICON_PASTE]);
            load(vg, "icons/cut.png", imgIconsBuf[ICON_CUT]);
            load(vg, "icons/delete.png", imgIconsBuf[ICON_DELETE]);
            load(vg, "icons/adjust.png", imgIconsBuf[ICON_ADJUST]);
            load(vg, "icons/close.png", imgIconsBuf[ICON_CLOSE]);
            load(vg, "icons/bypass.png", imgIconsBuf[ICON_BYPASS]);
            load(vg, "icons/loop.png", imgIconsBuf[ICON_LOOP]);
            load(vg, "icons/arr_down.png", imgIconsBuf[ICON_ARR_DOWN]);
            load(vg, "icons/arr_left.png", imgIconsBuf[ICON_ARR_LEFT]);
            load(vg, "icons/arr_right.png", imgIconsBuf[ICON_ARR_RIGHT]);
            load(vg, "icons/arr_up.png", imgIconsBuf[ICON_ARR_UP]);
            load(vg, "icons/plus.png", imgIconsBuf[ICON_PLUS]);
            load(vg, "icons/minus.png", imgIconsBuf[ICON_MINUS]);
            load(vg, "icons/automation.png", imgIconsBuf[ICON_AUTOMATION]);
            load(vg, "led.png", imgIconsBuf[IMG_LED]);
            load(vg, "led_off.png", imgIconsBuf[IMG_LED_OFF]);
            load(vg, "led_glow.png", imgIconsBuf[IMG_LED_GLOW]);
            load(vg, "icons/speaker.png", imgIconsBuf[ICON_SPEAKER]);
            load(vg, "icons/x.png", imgIconsBuf[ICON_X]);
            load(vg, "icons/daw_icon.png", imgIconsBuf[ICON_DAW_EXE]);
            load(vg, "icons/opt_unlocked.png", imgIconsBuf[ICON_OPT_UNLOCKED]);
            load(vg, "icons/opt_locked.png", imgIconsBuf[ICON_OPT_LOCKED]);
            load(vg, "icons/midiplug.png", imgIconsBuf[ICON_MIDIPLUG]);
            load(vg, "icons/duplicate.png", imgIconsBuf[ICON_DUPLICATE]);
            load(vg, "icons/synth_small.png", imgIconsBuf[ICON_SYNTH_SMALL]);
            load(vg, "icons/warning.png", imgIconsBuf[ICON_WARNING]);
            load(vg, "icons/modulation.png", imgIconsBuf[ICON_MODULATION]);
            load(vg, "icons/loading.png", imgIconsBuf[ICON_LOADING]);
            load(vg, "icons/modulation_input.png", imgIconsBuf[ICON_MODULATION_INPUT]);
            load(vg, "icons/icon_file_audio.png", imgIconsBuf[ICON_FILE_AUDIO]);
            load(vg, "icons/icon_file_midi.png", imgIconsBuf[ICON_FILE_MIDI]);

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
            findFilesWithExt(App::Platform::toResourcePath("fonts/gui/"), "ttf", true, files);
            findFilesWithExt(App::Platform::toResourcePath("fonts/gui/"), "otf", true, files);
            if (files.empty()) {
                throw appexception("Please install ttf fonts to fonts/gui");
            }
            fonts.fontsInstalled.clear();
            fonts.fontsInstalled.resize(files.size());
            for (size_t i = 0; i < files.size(); i++) {
                fonts.fontsInstalled[i].name = files[i].name;
                fonts.fontsInstalled[i].path = files[i].path;
            }
            if (fontsInstalled.empty()) fontsInstalled = fonts.fontsInstalled;
            fonts.fontsLoaded.clear();
            String fntList;
            int loaded     = 0;
            for (size_t i = 0; i < MAX_FONTS && i < files.size(); i++) {
                String fontPath = (files[i].path);
                LoadedFont lf;
                String fntKey = StringFormat("font%zu", i);
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
            if (loaded == 0) {
                log_lf(Log::L_WARN, "No fonts loaded\n");
            }
            perContextFonts[vg] = fonts;
        }
    }

    /** loads texture and store in perContextTextures */
    const NvgImageTexture* loadTexture(NVGcontext* vg, const String& path, const int flags) {
        auto texEntry = perContextTextures.find(path);
        if (texEntry == perContextTextures.end()) {
            perContextTextures[path] = NvgImageTexture();
        }
        texEntry = perContextTextures.find(path);
        NvgImageTexture& tex = texEntry->second;
        if (tex.flags != flags){
            for (auto& it : tex.perContextId) {
                nvgDeleteImage(it.first, it.second);
            }
            tex.perContextId.clear();
        }
        if (tex.perContextId.find(vg) == tex.perContextId.end()) {
            try {
                ImageBuf imgBuf;
                if (ReadImage(path, imgBuf) < 0) {
                    log_lf(Log::L_ERROR, "Error loading image %s\n", StringAsCStr(path));
                }
                load(vg, StringAsCStr(path), imgBuf);
                // NVG_IMAGE_GENERATE_MIPMAPS
                int32_t nvgid = nvgCreateImageRGBA(vg, imgBuf.w, imgBuf.h, flags, (const unsigned char*)imgBuf.bytes.data());
                nvgImageSize(vg, nvgid, &tex.width, &tex.height);
                tex.perContextId[vg] = nvgid;
                tex.flags = flags;
            } catch (std::exception& e) {
                log_lf(Log::L_ERROR, "Failed loading image %s: %s\n", StringAsCStr(path), e.what());
                tex.perContextId[vg] = -1;
                tex.flags = flags;
                return nullptr;
            }
        }
        return &tex;
    }
} // namespace RenderResources
