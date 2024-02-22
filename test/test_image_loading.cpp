#include "TestBase.hpp"
#include "assert_dbg.h"
#include "color_util.h"
#include "common/test_common.h"
#include "exceptions.h"
#include "math/seq_math.h"
#include "seq_util.h"
#include "str_util.h"
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtx/color_space.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "stb/stb_image.h"
#include "fileio.h"
#include "platform.h"
#include "buildinfo.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace {
    int64_t ReadImage_(const String& path, ImageBuf& ref) {
        if (!FileExists(path)) {
            throw FileIOException(StringFormat("File not found: %s", StringAsCStr(path)));
        }
        unsigned char* data = stbi_load(StringAsCStr(path), &ref.w, &ref.h, &ref.bitdepth, 0);
        if (!data) {
            throw FileIOException(StringFormat("%s: %s", StringAsCStr(path), stbi_failure_reason()));
        }
        int64_t bufSize = ref.w * ref.h * ref.bitdepth;
        ref.bytes.reserve(bufSize);
        ref.bytes.assign(data, data + bufSize);
        stbi_image_free(data);
        return bufSize;
    }

    std::vector<ivec4> createColorPaletteQuantized(const ImageBuf& img, uint8_t quantizationBits, uint32_t palletteSize) {
        // iterate over all pixels and count distinct color values
        int q = 1 << (8-quantizationBits);
        std::unordered_map<uint32_t, uint32_t> colors;
        for (int y = 0; y < img.h; y++) {
            for (int x = 0; x < img.w; x++) {
                uint8_t r = img.bytes[(y * img.w + x) * 3 + 0];
                uint8_t g = img.bytes[(y * img.w + x) * 3 + 1];
                uint8_t b = img.bytes[(y * img.w + x) * 3 + 2];
                auto hsv = glm::hsvColor(glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f));
                if (fp_math::isNanOrInfd(hsv.x)) {
                    hsv.x = 0.0f;
                }
                hsv.x /= 360.0f;
                uint8_t hQuantized = math::floorfS32(hsv.x * 255.0f / q) * q;
                uint8_t sQuantized = math::floorfS32(hsv.y * 255.0f / q) * q;
                uint8_t lQuantized = math::floorfS32(hsv.z * 255.0f / q) * q;
                dbgassert(hQuantized >= 0 && hQuantized <= 255);
                dbgassert(sQuantized >= 0 && sQuantized <= 255);
                dbgassert(lQuantized >= 0 && lQuantized <= 255);
                uint32_t colorIndex = (hQuantized << 16) | (sQuantized << 8) | lQuantized;
                colors[colorIndex]++;
            }
        }

        // print out the color histogram, order by count
        std::vector<std::pair<uint32_t, uint32_t>> colorPairs;
        colorPairs.reserve(colors.size());
        for (auto& kv : colors) {
            colorPairs.emplace_back(kv);
        }
        std::sort(colorPairs.begin(), colorPairs.end(), [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) {
            return a.second > b.second;
        });
        std::vector<ivec4> colorPalette;
        for (auto& kv : colorPairs) {
            auto c = kv.first;
            auto v = vec4((c >> 16) & 0xff, (c >> 8) & 0xff, (c >> 0) & 0xff, 0);
            auto hslI = v;
            auto hslF = glm::vec3(hslI.x / 255.0f, hslI.y / 255.0f, hslI.z / 255.0f);
            auto rgb = glm::rgbColor(hslF);
            // if (hslF.y < 0.3f || hslF.z < 0.3f) {
            //     continue;
            // }
            log_lf(Log::L_INFO, "color %08x: %f %f %f %f %f %f\n", kv.first, hslF.x, hslF.y, hslF.z, rgb.x, rgb.y, rgb.z);
            colorPalette.emplace_back(v);
            if (colorPalette.size() >= palletteSize) {
                break;
            }
        }
        return colorPalette;
    }
    void applyColorPalette(ImageBuf& img, const std::vector<ivec4>& colorPalette) {
        std::vector<uint32_t> colorPaletteU32;
        for (int y = 0; y < img.h; y++) {
            for (int x = 0; x < img.w; x++) {
                uint8_t& r = img.bytes[(y * img.w + x) * 3 + 0];
                uint8_t& g = img.bytes[(y * img.w + x) * 3 + 1];
                uint8_t& b = img.bytes[(y * img.w + x) * 3 + 2];
                auto hsv = glm::hsvColor(glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f));
                if (fp_math::isNanOrInfd(hsv.x)) {
                    hsv.x = 0.0f;
                }
                hsv.x /= 360.0f;
                glm::ivec4 hsvI(hsv * 255.0f, 0);
                // find closest color in palette
                glm::vec3 closestHSL = hsv;
                double closestDistance = -1.0;
                int32_t idx = 0;
                int32_t idxSel = 0;
                for (auto& cVI : colorPalette) {
                    idx++;
                    auto delta = cVI - hsvI;
                    auto d = sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                    if (closestDistance < 0 || d < closestDistance) {
                        closestDistance = d;
                        closestHSL = glm::vec3(cVI.x / 255.0f, cVI.y / 255.0f, cVI.z / 255.0f);
                        idxSel = idx;
                    }
                }
                dbgassert(!fp_math::isNanOrInfd(closestHSL.x));
                dbgassert(!fp_math::isNanOrInfd(closestHSL.y));
                dbgassert(!fp_math::isNanOrInfd(closestHSL.z));
                closestHSL.x = closestHSL.x * 360.0f;
                // closestHSL.x += 360.0f + hsv.y* sinf((hsv.z*hsv.x+(y/float(img.h-1))) * 2.0f * M_PI) * 180.0f;
                // closestHSL.x = fmod(closestHSL.x, 360.0f);
                dbgassert(closestHSL.x >= 0.0f && closestHSL.x <= 360.0f);
                dbgassert(closestHSL.y >= 0.0f && closestHSL.y <= 1.0f);
                dbgassert(closestHSL.z >= 0.0f && closestHSL.z <= 1.0f);
                // closestHSL.y = hsv.y;
                // closestHSL.z = hsv.z;
                // v.r = fmod((v.r + 180.0f), 360.0f);
                // v.r = fmod((v.r + 180.0f), 360.0f);
                auto rgb = glm::rgbColor(closestHSL);
                int chR  = rgb.r * 255.0f;
                dbgassert(chR >= 0 && chR <= 255);
                chR = rgb.g * 255.0f;
                dbgassert(chR >= 0 && chR <= 255);
                chR = rgb.b * 255.0f;
                dbgassert(chR >= 0 && chR <= 255);
                r = rgb.r * 255.0f;
                g = rgb.g * 255.0f;
                b = rgb.b * 255.0f;
            }
        }

    }
    void test_createAndApplyPaletteToImage(const String& path) {
        TEST_BEGIN("test_loadJPGFile");
        ImageBuf imgInput;
        try {
            if (ReadImage_(path, imgInput) < 0) {
                log_lf(Log::L_ERROR, "Error loading image %s\n", StringAsCStr(path));
            }
        } catch (std::exception& e) {
            log_lf(Log::L_ERROR, "Failed loading cursor %s: %s\n", StringAsCStr(path), e.what());
            return;
        }
        // int n = 0;
        // for (int numColors = 8; numColors <= 256; numColors*=2) {
        //     auto colorPalette = createColorPaletteQuantized(imgInput, 4, numColors);
        //     ImageBuf imgOutput = imgInput;
        //     applyColorPalette(imgOutput, colorPalette);
        //     String filename = StringFormat("out/image%d_%d_colors.png", n++, numColors);
        //     stbi_write_png(StringAsCStr(filename), imgOutput.w, imgOutput.h, 3, imgOutput.bytes.data(), imgOutput.w * 3);
        // }
        int numColors = 4;
        auto colorPalette = createColorPaletteQuantized(imgInput, 4, numColors);
        ImageBuf imgOutput = imgInput;
        applyColorPalette(imgOutput, colorPalette);
        String name;
        SplitPath(path, nullptr, &name, nullptr);
        String filename = StringFormat("test_image_loading/%s_%d_colors.png", StringAsCStr(name), numColors);
        String parentPath;
        SplitPath(filename, &parentPath, nullptr, nullptr);
        CreateDirectoryIfNotExists(parentPath);
        stbi_write_png(StringAsCStr(filename), imgOutput.w, imgOutput.h, 3, imgOutput.bytes.data(), imgOutput.w * 3);
        TEST_END();
    }

}// namespace

int main() {
    App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
    std::vector<FileFound> files;
    findFilesWithExt(TEST_PATH("images"), "jpg", true, files);
    findFilesWithExt(TEST_PATH("images"), "png", true, files);
    for (auto& f : files) {
        test_createAndApplyPaletteToImage(f.path);
    }
    return 0;
}
