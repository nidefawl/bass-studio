#include <stdint.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <nanovg.h>
#include <algorithm>
#include "color_util.h"
#include "seq_math.h"
#include "platform.h"
using glm::ivec2;
using glm::ivec3;
using glm::ivec4;

NVGcolor getCursorColor() {
	float f1 = 0.3f;
	NVGcolor cursorColor;
	cursorColor.r = cursorColor.a = 1;
	cursorColor.g = cursorColor.b = CLAMP_F((1.0f-f1)+sin(getTimeMillis()/160.0f)*f1);
	return cursorColor;
}
NVGcolor getContrastFontColor(uint32_t i);
NVGcolor rgbToNvg(uint32_t i) {
	NVGcolor c;
	c.b = (float)((i & 0xFF) / 255.); i >>= 8;
	c.g = (float)((i & 0xFF) / 255.); i >>= 8;
	c.r = (float)((i & 0xFF) / 255.); i >>= 8;
	c.a = 1.0f;
	return c;
}
NVGcolor rgbaToNvg(uint32_t i) {
	NVGcolor c;
	c.b = (float)((i & 0xFF) / 255.); i >>= 8;
	c.g = (float)((i & 0xFF) / 255.); i >>= 8;
	c.r = (float)((i & 0xFF) / 255.); i >>= 8;
	c.a = (float)((i & 0xFF) / 255.);
	return c;
}
NVGcolor rgbfToNvg(uint32_t i, float f) {
	NVGcolor c;
	c.b = (float)((i & 0xFF) / 255.); i >>= 8;
	c.g = (float)((i & 0xFF) / 255.); i >>= 8;
	c.r = (float)((i & 0xFF) / 255.); i >>= 8;
	c.a = f;
	return c;
}
uint32_t nvgToRGB(NVGcolor c) {
	int32_t r = CLAMP_I((int32_t)(c.r*255.0), 0, 255);
	int32_t g = CLAMP_I((int32_t)(c.g*255.0), 0, 255);
	int32_t b = CLAMP_I((int32_t)(c.b*255.0), 0, 255);
	uint32_t rgb = 0;
	rgb |= b;
	rgb |= g<<8;
	rgb |= r<<16;
	rgb |= 0xFF000000;
	return rgb;
}
uint32_t nvgToRGBA(NVGcolor c) {
	int32_t r = CLAMP_I((int32_t)(c.r*255.0), 0, 255);
	int32_t g = CLAMP_I((int32_t)(c.g*255.0), 0, 255);
	int32_t b = CLAMP_I((int32_t)(c.b*255.0), 0, 255);
	int32_t a = CLAMP_I((int32_t)(c.a*255.0), 0, 255);
	uint32_t rgba = 0;
	rgba |= b;
	rgba |= g<<8;
	rgba |= r<<16;
	rgba |= a<<24;
	return rgba;
}
NVGcolor mulSatBright(NVGcolor rgb, float sat, float brt) {
	NVGcolor hsl = nvgToHSL(rgb);
	return nvgHSL(hsl.r, CLAMP_F(hsl.g*sat), CLAMP_F(hsl.b*brt));
}
NVGcolor HSVtoRGB(float h, float s, float v)
{

	struct rgbdouble {
		double x, y, z;
	};
	rgbdouble RGB;
    double H = h, S = s, V = v,
            P, Q, T,
            fract;

    (H == 360.)?(H = 0.):(H /= 60.);
    fract = H - floor(H);

    P = V*(1. - S);
    Q = V*(1. - S*fract);
    T = V*(1. - S*(1. - fract));

    if      (0. <= H && H < 1.)
        RGB = {V, T, P};
    else if (1. <= H && H < 2.)
        RGB = {Q, T, P};
    else if (2. <= H && H < 3.)
        RGB = {P, V, T};
    else if (3. <= H && H < 4.)
        RGB = {P, Q, V};
    else if (4. <= H && H < 5.)
        RGB = {T, P, V};
    else if (5. <= H && H < 6.)
        RGB = {V, P, Q};
    else
        RGB = {0.f, 0.f, 0.f};

    return nvgRGBAf(RGB.x, RGB.y, RGB.z, 1.0);
}
NVGcolor HSLtoRGB(float h, float s, float l)
{

    if (h == 0)
    {
        return {l, l, l, 1.0f};
    }

    int region = floor(h*6);
    float f = h*6-region;

    float p = l * ( 1 - s );
    float q = l * ( 1 - s * f );
    float t = l * ( 1 - s * ( 1 - f ) );
    NVGcolor rgb{};
    switch (region)
    {
        case 0:
        	return {l, t, p, 1.0f};
        case 1:
        	return {q, l, p, 1.0f};
        case 2:
        	return {p, l, t, 1.0f};
        case 3:
        	return {p, q, l, 1.0f};
        case 4:
        	return {t, p, l, 1.0f};
        default:
        	return {l, p, q, 1.0f};
    }

    return rgb;
}

NVGcolor nvgToHSL(NVGcolor rgb) {
	double r, g, b;
	r = rgb.r;
	g = rgb.g;
	b = rgb.b;
	double fCMax = std::max(std::max(r, g), b);
	double fCMin = std::min(std::min(r, g), b);
	double diff = fCMax - fCMin;

	double h = 0.0f, s = 0.0f, l = (fCMin + fCMax) / 2.0;

	if (diff != 0) {
		s = l < 0.5 ? diff / (fCMax + fCMin) : diff / (2.0 - fCMax - fCMin);

		h = (r == fCMax ? (g - b) / diff : g == fCMax ? 2.0 + (b - r) / diff : 4.0 + (r - g) / diff) / 6.0;
	}
	NVGcolor hsv;
	hsv.r = (float)h;
	hsv.g = (float)s;
	hsv.b = (float)l;
	hsv.a = 1.0f;
	return hsv;
}
NVGcolor getContrastFontColor(uint32_t color) {
	return rgbToNvg(getContrastFontColoru32(color));
}

NVGcolor getContrastFontColorNvg(NVGcolor i) {
	return rgbToNvg(getContrastFontColoru32(nvgToRGB(i)));
}
uint32_t getContrastFontColoru32(uint32_t color) {
	glm::vec4 rgb4 = int32vec4(color);
	for (int i = 0; i < 3; i++) {
		double c = rgb4[i];
		if (c <= 0.03928) {
			c = c/12.92;
		} else {
			c = pow(((c+0.055)/1.055), 2.4);
		}
		rgb4[i] = (float) c;
	}
	double lum = 0.2126f * rgb4.x + 0.7152f * rgb4.y + 0.0722f * rgb4.z;

	if (lum > 0.179) {
		return 0x000000;
	}
	return 0xffffff;
}
glm::vec4 colorHex(uint32_t color)
{
	// input format 0xAARRGGBB
	float b = (float)(0x000000FF & color) / 255.f;
	color = color >> 8;
	float g = (float)(0x000000FF & color) / 255.f;
	color = color >> 8;
	float r = (float)(0x000000FF & color) / 255.f;
	color = color >> 8;
	float a = (float)(0x000000FF & color) / 255.f;

	glm::vec4 nvgColor;
	nvgColor.x = r;
	nvgColor.y = g;
	nvgColor.z = b;
	nvgColor.w = a;
	return nvgColor;
}
glm::vec4 RGBtoHSV(glm::vec4 rgb) {
	double r, g, b;
	r = rgb.x;
	g = rgb.y;
	b = rgb.z;
	double fCMax = std::max(std::max(r, g), b);
	double fCMin = std::min(std::min(r, g), b);
	double diff = fCMax - fCMin;

	double h = 0.0f, s = 0.0f, l = (fCMin + fCMax) / 2.0;

	if (diff != 0) {
		s = l < 0.5 ? diff / (fCMax + fCMin) : diff / (2.0 - fCMax - fCMin);

		h = (r == fCMax ? (g - b) / diff : g == fCMax ? 2.0 + (b - r) / diff : 4.0 + (r - g) / diff) / 6.0;
	}
	glm::vec4 hsv;
	hsv.x = (float) h;
	hsv.y = (float) s;
	hsv.z = (float) l;
	hsv.w = 1.0f;
	return hsv;
}
glm::vec4 hexToHSL(uint32_t color) {
	glm::vec4 color4f = colorHex(color);
	return RGBtoHSV(color4f);
}
NVGcolor hexToHSLNvg(uint32_t color) {
	glm::vec4 color4f = RGBtoHSV(colorHex(color));
	return {color4f.x, color4f.y, color4f.z, color4f.w};
}

glm::vec4 int32vec4(uint32_t i) {
	glm::vec4 r;
	r.z = (float) ((i & 0xFF) / 255.); i >>= 8;
	r.y = (float) ((i & 0xFF) / 255.); i >>= 8;
	r.x = (float) ((i & 0xFF) / 255.); i >>= 8;
	r.w = (float) ((i & 0xFF) / 255.); i >>= 8;
	return r;
}

int col(int bits) {
	int r = 0xff*(bits&1);
	int g = 0xff*((bits&2)*128);
	int b = 0xff*((bits&4)*64*256);
	return r | g | b;
}
