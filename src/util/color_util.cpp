#include "types.hpp"
#include <nanovg.h>
#include <algorithm>
#include "math/seq_math.hpp"
#include "math/vec.hpp"
#include "color_util.hpp"
#include "platform.hpp"

NVGcolor getCursorColor() {
    float f1 = 0.3f;
    NVGcolor cursorColor;
    cursorColor.r = cursorColor.a = 1;
    cursorColor.g = cursorColor.b = CLAMP_F((1.0f - f1) + sin(getTimeMillisF() / 160.0f) * f1);
    return cursorColor;
}

NVGcolor rgbToNvg(uint32_t i) {
    NVGcolor c;
    c.b = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    c.g = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    c.r = (float) ((i & 0xFF) / 255.);
    c.a = 1.0f;
    return c;
}

NVGcolor rgbaToNvg(uint32_t i) {
    NVGcolor c;
    c.b = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    c.g = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    c.r = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    c.a = (float) ((i & 0xFF) / 255.);
    return c;
}

NVGcolor rgbfToNvg(uint32_t i, float f) {
    NVGcolor c;
    c.b = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    c.g = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    c.r = (float) ((i & 0xFF) / 255.);
    c.a = f;
    return c;
}

uint32_t nvgToRGB(NVGcolor c) {
    int32_t r    = CLAMP_I((int32_t) (c.r * 255.0), 0, 255);
    int32_t g    = CLAMP_I((int32_t) (c.g * 255.0), 0, 255);
    int32_t b    = CLAMP_I((int32_t) (c.b * 255.0), 0, 255);
    uint32_t rgb = 0;
    rgb |= b;
    rgb |= g << 8;
    rgb |= r << 16;
    rgb |= 0xFF000000;
    return rgb;
}

uint32_t vec3ToRgbU32(glm::vec3 i) {
    uint32_t rgb = 0;
    rgb |= (uint32_t) (i.z * 255.0f);
    rgb |= (uint32_t) (i.y * 255.0f) << 8;
    rgb |= (uint32_t) (i.x * 255.0f) << 16;
    rgb |= 0xFF000000;
    return rgb;
}

uint32_t vec4ToRgbU32(glm::vec4 i) {
    uint32_t rgb = 0;
    rgb |= (uint32_t) (i.z * 255.0f);
    rgb |= (uint32_t) (i.y * 255.0f) << 8;
    rgb |= (uint32_t) (i.x * 255.0f) << 16;
    rgb |= (uint32_t) (i.w * 255.0f) << 24;
    return rgb;
}

uint32_t nvgToRGBA(NVGcolor c) {
    int32_t r     = CLAMP_I((int32_t) (c.r * 255.0), 0, 255);
    int32_t g     = CLAMP_I((int32_t) (c.g * 255.0), 0, 255);
    int32_t b     = CLAMP_I((int32_t) (c.b * 255.0), 0, 255);
    int32_t a     = CLAMP_I((int32_t) (c.a * 255.0), 0, 255);
    uint32_t rgba = 0;
    rgba |= b;
    rgba |= g << 8;
    rgba |= r << 16;
    rgba |= a << 24;
    return rgba;
}

NVGcolor getContrastFontColor(uint32_t color) {
    return rgbToNvg(getContrastFontColoru32(color));
}

NVGcolor getContrastFontColorNvg(NVGcolor i) {
    return rgbToNvg(getContrastFontColoru32(nvgToRGB(i)));
}

float getLuminance(uint32_t color) {
    glm::vec4 rgb4 = int32vec4(color);
    for (int i = 0; i < 3; i++) {
        auto& c = rgb4[i];
        if (c <= 0.03928f) {
            c = c / 12.92f;
        } else {
            c = math::powf(((c + 0.055f) / 1.055f), 2.4f);
        }
    }
    return 0.2126f * rgb4.x + 0.7152f * rgb4.y + 0.0722f * rgb4.z;
}

uint32_t getContrastFontColoru32(uint32_t color) {
    auto lum = getLuminance(color);
    if (lum > 0.179f) {
        return 0x000000;
    }
    return 0xffffff;
}

glm::vec4 colorHex(uint32_t color) {
    // input format 0xAARRGGBB
    float b = (float) (0x000000FF & color) / 255.f;
    color   = color >> 8;
    float g = (float) (0x000000FF & color) / 255.f;
    color   = color >> 8;
    float r = (float) (0x000000FF & color) / 255.f;
    color   = color >> 8;
    float a = (float) (0x000000FF & color) / 255.f;

    glm::vec4 nvgColor;
    nvgColor.x = r;
    nvgColor.y = g;
    nvgColor.z = b;
    nvgColor.w = a;
    return nvgColor;
}

glm::vec4 int32vec4(uint32_t i) {
    glm::vec4 r;
    r.z = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    r.y = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    r.x = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    r.w = (float) ((i & 0xFF) / 255.);
    return r;
}

int col(int bits) {
    int r = 0xff * (bits & 1);
    int g = 0xff * ((bits & 2) * 128);
    int b = 0xff * ((bits & 4) * 64 * 256);
    return r | g | b;
}
