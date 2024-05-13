#pragma once

#include "types.h"
#include "math/vec.h"
#include <nanovg_min.h>

vec4 colorHex(uint32_t color);
vec4 int32vec4(uint32_t i);
inline NVGcolor vec4ToNvg(vec4 v) {
    return { v.x, v.y, v.z, v.w };
}
float getLuminance(uint32_t color);
NVGcolor getContrastFontColor(uint32_t i);
NVGcolor getContrastFontColorNvg(NVGcolor i);
NVGcolor rgbToNvg(uint32_t i);
NVGcolor rgbaToNvg(uint32_t i);
NVGcolor rgbfToNvg(uint32_t i, float f);
uint32_t vec3ToRgbU32(glm::vec3 i);
uint32_t vec4ToRgbU32(glm::vec4 i);
uint32_t nvgToRGB(NVGcolor i);
uint32_t nvgToRGBA(NVGcolor c);
uint32_t getContrastFontColoru32(uint32_t color);
NVGcolor getCursorColor();
int col(int bits);
