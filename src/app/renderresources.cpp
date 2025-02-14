#include "archive.h"
#include "archive_entry.h"
#include "logging.hpp"
#include "renderresources_zip.hpp"
#include "types.hpp"
#include <exception>
#include <map>
#include <memory>
#include <variant>
#include <vector>
#include <nanovg.h>

#include "str_util.hpp"
#include "fileio.hpp"
#include "platform.hpp"
#include "renderresources.hpp"
#include "guifonts.hpp"
#include "exceptions.hpp"

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
    using ResourceMap = std::map<String, std::vector<uint8_t>>;
    ResourceMap resources;
    NvgImageTexture imgDashedLine;
    NvgImageTexture imgIcons[NUM_IMGS];
    std::unordered_map<NVGcontext*, NvgFonts> perContextFonts;
    std::vector<FontDesc> fontsInstalled;
    std::unordered_map<String, NvgImageTexture> perContextTextures;
    LoadedFont emojiFont{};

    std::variant<ResourceMap, String> loadEmbeddedRenderResources() {
        auto resData = RenderResources::getResData();
        std::map<String, std::vector<uint8_t>> files;
        // unpack inmemory using libarchive
        // read the archive
        struct archive* a = archive_read_new();
        if (!a) {
            return "archive_read_new() failed";
        }
        if (ARCHIVE_OK != archive_read_support_filter_all(a)) {
            return "archive_read_support_filter_all() failed";
        }
        if (ARCHIVE_OK != archive_read_support_format_all(a)) {
            return "archive_read_support_format_all() failed";
        }
        if (ARCHIVE_OK != archive_read_open_memory(a, resData.data(), resData.size())) {
            return "archive_read_open_memory() failed";
        }
        // iterate over all files in the archive
        for (;;) {
            // read the next archive entry
            struct archive_entry* entry;
            int r = archive_read_next_header(a, &entry);
            if (r == ARCHIVE_EOF) {
                break;
            }
            if (r != ARCHIVE_OK) {
                auto errorMsg = archive_error_string(a);
                archive_read_free(a);
                return String("archive_read_next_header() failed: ") + errorMsg;
            }
            auto pathName = archive_entry_pathname(entry);
            if (archive_entry_filetype(entry) == AE_IFREG) {
                auto size = archive_entry_size(entry);
                if (size <= 0) {
                    continue;
                }
                if (!pathName) {
                    continue;
                }
                auto pathNameStr = String(pathName);
                auto buffer = std::vector<uint8_t>(size);
                auto readsize = archive_read_data(a, buffer.data(), buffer.size());
                if (readsize != ssize_t(buffer.size())) {
                    continue;
                }
                files[pathNameStr] = buffer;
            }
        }
        archive_read_free(a);
        return files;
    }

    
    namespace {
        void load(NVGcontext* vg, const char* path, ImageBuf& out) {
            auto it = resources.find(path);
            if (it != resources.end()) {
                try {
                    if (ReadImageFromBuffer(it->second, out) < 0) {
                        log_lf(Log::L_ERROR, "Error loading image %s\n", path);
                    } else {
                        return;
                    }
                } catch (FileIOException& e) {
                    log_lf(Log::L_ERROR, "Failed loading image %s: %s\n", path, e.what());
                }
            }
            try {
                if (ReadImage(path, out) < 0) {
                    log_lf(Log::L_ERROR, "Error loading image %s\n", path);
                }
            } catch (FileIOException& e) {
                log_lf(Log::L_ERROR, "Failed loading image %s: %s\n", path, e.what());
            }
        }
    } // namespace

    void reloadFonts(NVGcontext* vg) {
        emojiFont = {};
        String emojiFontFile = "EmojiOneBW.otf";
        auto emojiFontPath = "fonts/gui/" + emojiFontFile;
        auto fileEmojiFontPath = App::Platform::toResourcePath(emojiFontPath);
        if (0&&FileExists(fileEmojiFontPath)) {
            emojiFont.nvgId = nvgCreateFont(vg, "emoji", emojiFontPath.c_str());
            emojiFont.font = { "emoji", emojiFontPath };
            emojiFont.loaded = emojiFont.nvgId >= 0;
            emojiFont.name = "emoji";
        } else {
            // try loading emojiFontPath from resources
            auto it = resources.find(emojiFontPath);
            if (it != resources.end()) {
                emojiFont.nvgId = nvgCreateFontMem(vg, "emoji", it->second.data(), it->second.size(), 0);
                emojiFont.font = { "emoji", emojiFontPath };
                emojiFont.loaded = emojiFont.nvgId >= 0;
                emojiFont.name = "emoji";
            }
        }
        NvgFonts fonts;
        fonts.fontsInstalled.clear();
        fonts.fontsLoaded.clear();
        {
            for (auto& [path, data] : resources) {
                String fileExt;
                String filenameNoExt;
                SplitPath(path, nullptr, &filenameNoExt, &fileExt);
                if (fileExt == "ttf" || fileExt == "otf") {
                    LoadedFont lf;
                    lf.loaded = false;
                    lf.name = filenameNoExt;
                    lf.font.name = filenameNoExt;
                    lf.font.path = path;
                    fonts.fontsLoaded.push_back(lf);
                    fonts.fontsInstalled.push_back({ filenameNoExt, path });
                }
            }
            perContextFonts[vg] = fonts;
        }
        {
            std::vector<FileFound> files;
            findFilesWithExt(App::Platform::toResourcePath("fonts/gui/"), "ttf", true, files);
            findFilesWithExt(App::Platform::toResourcePath("fonts/gui/"), "otf", true, files);
            for (size_t i = 0; i < files.size(); i++) {
                fonts.fontsInstalled.push_back({ files[i].name, files[i].path });
            }
            for (size_t i = 0; i < files.size(); i++) {
                LoadedFont lf;
                lf.loaded    = lf.nvgId >= 0;
                lf.name      = files[i].name;
                lf.font.name = files[i].name;
                lf.font.path = files[i].path;
                fonts.fontsLoaded.push_back(lf);
            }
        }
        perContextFonts[vg] = fonts;
        fontsInstalled = fonts.fontsInstalled;
    }

    void initResources(NVGcontext* vg) {
        auto loadRes = loadEmbeddedRenderResources();
        if (std::holds_alternative<String>(loadRes)) {
            log_lf(Log::L_ERROR, "Failed loading embedded resources: %s\n", std::get<String>(loadRes).c_str());
        } else {
            resources = std::get<ResourceMap>(loadRes);
        }
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
            load(vg, "icons/checkbox_unchecked.png", imgIconsBuf[ICON_CHECKBOX_UNCHECKED]);
            load(vg, "icons/checkbox_checked.png", imgIconsBuf[ICON_CHECKBOX_CHECKED]);
            load(vg, "icons/search.png", imgIconsBuf[ICON_SEARCH]);
            load(vg, "icons/debug.png", imgIconsBuf[ICON_DEBUG]);
            load(vg, "icons/history.png", imgIconsBuf[ICON_HISTORY]);
            load(vg, "icons/keyboard.png", imgIconsBuf[ICON_KEYBOARD]);
            load(vg, "icons/export.png", imgIconsBuf[ICON_EXPORT]);
            load(vg, "icons/settings.png", imgIconsBuf[ICON_SETTINGS]);
            load(vg, "icons/performance.png", imgIconsBuf[ICON_PERFORMANCE]);
            load(vg, "icons/theme.png", imgIconsBuf[ICON_THEME]);

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
        reloadFonts(vg);
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
