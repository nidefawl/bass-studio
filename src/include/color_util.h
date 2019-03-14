#pragma once

#include <stdint.h>
#include <glm/glm.hpp>
#include <glm/vec4.hpp>
#include <nanovg.h>

glm::vec4 RGBtoHSV(glm::vec4 rgb);
NVGcolor HSVtoRGB(float h, float s, float l);
glm::vec4 colorHex(uint32_t color);
glm::vec4 hexToHSL(uint32_t color);
glm::vec4 int32vec4(uint32_t i);
NVGcolor getContrastFontColor(uint32_t i);
NVGcolor getContrastFontColorNvg(NVGcolor i);
NVGcolor rgbToNvg(uint32_t i);
NVGcolor rgbaToNvg(uint32_t i);
NVGcolor rgbfToNvg(uint32_t i, float f);
uint32_t nvgToRGB(NVGcolor i);
uint32_t getContrastFontColoru32(uint32_t color);
NVGcolor nvgToHSL(NVGcolor rgb);
NVGcolor mulSatBright(NVGcolor rgb, float sat, float brt);
NVGcolor getCursorColor();
int col(int bits);
