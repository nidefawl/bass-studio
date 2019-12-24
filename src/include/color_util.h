#pragma once

#include <stdint.h>
#include "math/vec.h"
#include <nanovg_min.h>

vec4 rgbToHSL(float r, float g, float b);
vec4 RGBtoHSV(vec4 rgb);
NVGcolor HSLtoRGB(float h, float s, float l);
NVGcolor HSVtoRGB(float h, float s, float v);
vec4 colorHex(uint32_t color);
vec4 hexToHSL(uint32_t color);
vec4 int32vec4(uint32_t i);
inline NVGcolor vec4ToNvg(vec4 v) {
	return {v.x, v.y, v.z, v.w};
}
NVGcolor getContrastFontColor(uint32_t i);
NVGcolor getContrastFontColorNvg(NVGcolor i);
NVGcolor rgbToNvg(uint32_t i);
NVGcolor rgbaToNvg(uint32_t i);
NVGcolor rgbfToNvg(uint32_t i, float f);
uint32_t nvgToRGB(NVGcolor i);
uint32_t nvgToRGBA(NVGcolor c);
uint32_t getContrastFontColoru32(uint32_t color);
NVGcolor nvgToHSL(NVGcolor rgb);
NVGcolor mulSatBright(NVGcolor rgb, float sat, float brt);
NVGcolor getCursorColor();
int col(int bits);
