/**
 * Copyright (c) 2024 Michael Hept
 */

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/color_space.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <map>
#include <vector>
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>
#include "color_util.h"
#include "net/network.h"
#include "net/packet.h"
#include "rgb-controller.h"
#include "rgb_network_types.h"
#include "thread.h"
#include "tls.h"
#include "logging.h"
#include "math/seq_math.h"
#include "rand.h"
#include "str_util.h"
#include "threads/workerthread.h"
#include "assert_dbg.h"
#include "buildinfo.h"

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

void network_init(void);
void network_cleanup(void);
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)
#define LED_STRIP_HAS_GRBW 0
#define LED_STRIP_NUM_STRIPS 10
#define LED_STRIP_LED_NUMBERS 30
#define LED_NUM_TOTAL (LED_STRIP_NUM_STRIPS * LED_STRIP_LED_NUMBERS)

#define LED_UDP_LISTEN_PORT 54321
#define LED_COLOR_DEFAULT 0xFFFFFFFF

static uint32_t RGB_DISPLAY_FPS             = 100;
static uint32_t RGB_DISPLAY_BRIGHTNESS      = 55;
static uint32_t RGB_DISPLAY_STRIPES_ENABLED = ~0;

struct frame_rgb_buffer_t {
    uint32_t* pData;
    size_t lenData;
    int32_t w;
    int32_t h;
};
struct frame_render_ctxt_t {
    uint32_t frameStep;
    int16_t lampId;
};
static void writeToFrameBuffer(frame_rgb_buffer_t& buffer, int32_t x, int32_t y,
                               vec4 col) {

    if (x < 0 || x >= buffer.w || y < 0 || y >= buffer.h)
        return;
    uint32_t idx = x * buffer.h + y;
    if (idx < buffer.lenData) {
        uint32_t intRGBA = math::max<int32_t>(
                0, math::min<int32_t>(255, math::roundfS32(col.b * 255.0f)));
        intRGBA |=
                (math::max<int32_t>(
                         0, math::min<int32_t>(255, math::roundfS32(col.g * 255.0f))) &
                 0xFF)
                << 8;
        intRGBA |=
                (math::max<int32_t>(
                         0, math::min<int32_t>(255, math::roundfS32(col.r * 255.0f))) &
                 0xFF)
                << 16;
        intRGBA |=
                (math::max<int32_t>(
                         0, math::min<int32_t>(255, math::roundfS32(col.a * 255.0f))) &
                 0xFF)
                << 24;

        buffer.pData[idx] = intRGBA;
    }
}

vec3 HSLtoRGB(vec3 hsl) {
    float h = hsl.x;
    float s = hsl.y;
    float l = hsl.z;
    if (h == 0) {
        return { l, l, l };
    }

    int region = floor(h * 6);
    float f    = h * 6 - region;

    float p = l * (1 - s);
    float q = l * (1 - s * f);
    float t = l * (1 - s * (1 - f));
    switch (region) {
        case 0:
            return { l, t, p };
        case 1:
            return { q, l, p };
        case 2:
            return { p, l, t };
        case 3:
            return { p, q, l };
        case 4:
            return { t, p, l };
        default:
            return { l, p, q };
    }
}

vec3 rgbToHSL(vec3 rgb) {
    float r    = rgb.r;
    float g    = rgb.g;
    float b    = rgb.b;
    float minV = math::min(math::min(r, g), b);
    float maxV = math::max(math::max(r, g), b);
    float h, s, l = (maxV + minV) / 2;

    if (maxV == minV) {
        h = s = 0;// achromatic
    } else {
        auto greatest = [](auto x, auto y, auto z) {
            return x > y ? (x > z ? 0 : 2) : (y > z ? 1 : 2);
        };
        float d = maxV - minV;
        s       = l > 0.5 ? d / (2 - maxV - minV) : d / (maxV + minV);
        int mx  = greatest(r, g, b);
        if (mx == 0) {
            h = (g - b) / d + (g < b ? 6 : 0);
        } else if (mx == 1) {
            h = (b - r) / d + 2;
        } else {
            h = (r - g) / d + 4;
        }
        h /= 6;
    }

    return glm::vec3{ h, s, l };
}

vec3 fromHex(uint32_t i) {
    vec3 c;
    c.b = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    c.g = (float) ((i & 0xFF) / 255.);
    i >>= 8;
    c.r = (float) ((i & 0xFF) / 255.);
    return c;
}

struct RGBNetworkController {
    struct lamp_context_t {
        uint32_t frameId   = 0;
        uint32_t frameStep = 0;
        uint64_t packetId  = 0;
    };

    int64_t lastResyncCycle = -1;
    int64_t tmLastSync      = 0;
    int64_t tmBegin         = 0;
    std::map<int32_t, lamp_context_t> ctxts;
    int n = 0;
    //	int masterFrameStep = 0;
    packet_hdr_t recvHeader{};
    uint8_t writeBuf[RGB_PROTOCOL_WRITE_BUF_SIZE]{};
    uint32_t arr[LED_NUM_TOTAL]{};
    uint32_t curProgram = 0;
    uint32_t brightness = 80;
    RGBNetworkController() = default;

    void runProgram0(frame_render_ctxt_t& renderCtxt, frame_rgb_buffer_t& buffer) {
        if (0 == renderCtxt.frameStep) {
            memset(arr, 0, sizeof(uint32_t) * LED_NUM_TOTAL);
        }
        const uint32_t iFrameStep = renderCtxt.frameStep;
        const float frameStep     = iFrameStep * 0.4f;
        const int16_t lampId      = renderCtxt.lampId;
        float fTempoFrames        = 333.0f;
        float fTempoSlowFrames    = 24.0f;
        for (int xOut = 0; xOut < buffer.w; xOut++) {
            for (int yOut = 0; yOut < buffer.h; yOut++) {

                int x = xOut;
                int y = yOut;
                if (lampId >= 2) {
                    y += buffer.h;
                }
                float fx  = x / (float) (LED_STRIP_LED_NUMBERS - 1);
                float fy  = y / (float) (LED_STRIP_NUM_STRIPS - 1);
                float ff  = fmodf(frameStep / fTempoFrames, 1.0f);
                float ff2 = fmodf(frameStep / fTempoSlowFrames, 1.0f);
                //						ff = ff2 = fmodf(ctxt.frameStep/166.0f, 1.0f);
                ff2 = 1.0f - abs(ff2 - 0.5f) * 2.0f;
                ff  = 1.0f - abs(ff - 0.5f) * 2.0f;
                ff *= ff;

                float cntx = (1.0f - abs(fx - 0.5f) * 2.0f) + 0.0f;
                float cnty = (1.0f - abs(fy - 0.5f) * 2.0f) + 0.12f;
                float g    = pow(cntx, 1.8f + 9.0f * ff) * pow(cnty, 1.6f + 3.0f * ff2) * 1.1f;
                float r    = pow(cntx, 1.2f + 2.0f * ff2) * pow(cnty, 1.0f + 7.0f * ff) * 1.2f;
                //							float b = pow(cntx, 1.0f+7.0f*ff)*pow(cnty, 1.0f+7.0f*ff);
                //							float g = pow(cntx, 1.8f+9.0f*ff)*pow(cnty, 1.6f+3.0f*ff2);
                //							float r = pow(cntx, 1.2f+2.0f*ff2)*pow(cnty, 1.0f+7.0f*ff);
                //							float r, g;
                float b = 0;
                g       = 0;
                r       = 0;
                //							float sc = ((ctxt.frameStep%1000)/1000.0f)*244.0f/256.0f;
                //							if (ff2*ff2>0.3*ff) {
                //								sc+=0.4f;
                //							}
                float fColHue = (cnty * cntx) + (frameStep + y) * 0.01f;
                auto hsl      = rgbToHSL(vec3{ math::clamp(r, 0.0f, 1.0f), math::clamp(g, 0.0f, 1.0f), math::clamp(b, 0.0f, 1.0f) });
                auto col      = HSLtoRGB(vec3(0.05 + fmodf(fColHue, 1.0f) * 0.9, 0.98f, 0.4f));
                r             = math::max(0.0f, cntx - 0.9f) * 1.0f * cnty * col.r * (0.5 + 0.5 * sin(ff * 3.1495 * 2.0 + M_PI * 0.0f / 3.0f));
                g             = math::max(0.0f, cntx - 0.9f) * 1.0f * cnty * col.g * (0.5 + 0.5 * sin(ff * 3.1495 * 2.0 + M_PI * 2.0f / 3.0f));
                b             = math::max(0.0f, cntx - 0.9f) * 1.0f * cnty * col.b * (0.5 + 0.5 * sin(ff * 3.1495 * 2.0 + M_PI * 4.0f / 3.0f));
                r             = col.b;
                g             = col.r;
                b             = col.g;
                if ((x & 1) ^ (y & 1)) {
                    //								float ffff = fmodf(fx+(ctxt.frameStep+y)*0.05f, 1.0f);
                    //								NVGcolor col = HSLtoRGB(0.05+(1.0-0.05)*fx*fy*ff, (0.98), 0.39f);
                    //								b += pow(cntx, 1.6f+1.0f*ff)*pow(cnty, 1.6f+6.0f*ff2)*1.1f;
                    float sc = 0.2 + (6.4f) * (0.5 + 0.5 * sin(3.1495f * 2 * ((frameStep) * 0.00754f)));
                    r *= sc;
                    g *= sc;
                    b *= sc;

                } else {
                    if (fmodf((frameStep / 66.0f), 8.0f) < 4.0f) {
                        auto hsl = rgbToHSL(vec3{ r, g, b });
                        col      = HSLtoRGB(vec3{ fmodf(hsl.x + 0.5f, 1.0f), hsl.y, hsl.z });
                        r        = col.r;
                        g        = col.g;
                        b        = col.b;
                    }
                    //								g += pow(cntx, 1.6f+2.0f*ff)*pow(cnty, 1.6f+4.0f*ff2)*2.1f;
                }
                fColHue = hsl.r + fColHue;
                fColHue = fmodf(fColHue, 1.0f);
                //							NVGcolor col = HSLtoRGB(fColHue, hsl.g, hsl.b);
                float ffff = fmodf(fx + (frameStep + y) * 0.05f, 1.0f);
                //							NVGcolor col = HSLtoRGB(0.05+(1.0-0.05)*fx*fy*ff, (0.98), 0.39f);
                //							NVGcolor col = HSLtoRGB(0.05+(1.0-0.05)*(rand.rng_bits(12)/4095.0f), (0.98), 0.0f+0.5f*(rand.rng_bits(12)/4095.0f));

                float sc2 = 1.0f;
                r *= sc2;
                g *= sc2;
                b *= sc2;
                //							g=col.g*sc;
                //							b=col.b*sc;
                //							if (x*y<ctxt.frameStep%LED_NUM_TOTAL*2) {
                //								r+=0.3f;
                //							}
                //							if (x+y*LED_STRIP_LED_NUMBERS==ctxt.frameStep%LED_NUM_TOTAL*2) {
                //								b+=0.3f;
                //							}
                writeToFrameBuffer(buffer, xOut, yOut, vec4{ r, g, b, 1.0f });
            }
        }
    }
    void runProgram0_(frame_render_ctxt_t& renderCtxt,
                      frame_rgb_buffer_t& buffer) {
        if (0 == renderCtxt.frameStep) {
            memset(arr, 0, sizeof(uint32_t) * uint32_t(LED_NUM_TOTAL));
        }
        const uint32_t iFrameStep = renderCtxt.frameStep;
        const float frameStep     = iFrameStep * 0.4f;
        const int16_t lampId      = renderCtxt.lampId;
        float fTempoFrames        = 333.0f * 0.2f;
        float fTempoSlowFrames    = 24.0f * 0.2f;
        for (int xOut = 0; xOut < buffer.w; xOut++) {
            for (int yOut = 0; yOut < buffer.h; yOut++) {

                int x = xOut;
                int y = yOut;
                if (lampId >= 2) {
                    y += buffer.h;
                }
                float fx  = x / (float) (LED_STRIP_LED_NUMBERS - 1);
                float fy  = y / (float) (LED_STRIP_NUM_STRIPS - 1);
                float ff  = fmodf(frameStep / fTempoFrames, 1.0f);
                float ff2 = fmodf(frameStep / fTempoSlowFrames, 1.0f);
                //						ff = ff2 =
                //fmodf(ctxt.frameStep/166.0f, 1.0f);
                ff2 = 1.0f - abs(ff2 - 0.5f) * 2.0f;
                ff  = 1.0f - abs(ff - 0.5f) * 2.0f;
                ff *= ff;

                float cntx = (1.0f - abs(fx - 0.5f) * 2.0f) + 0.0f;
                float cnty = (1.0f - abs(fy - 0.5f) * 2.0f) + 0.12f;
                float g =
                        pow(cntx, 1.8f + 9.0f * ff) * pow(cnty, 1.6f + 3.0f * ff2) * 1.1f;
                float r =
                        pow(cntx, 1.2f + 2.0f * ff2) * pow(cnty, 1.0f + 7.0f * ff) * 1.2f;
                //							float b =
                //pow(cntx, 1.0f+7.0f*ff)*pow(cnty, 1.0f+7.0f*ff); 							float g =
                //pow(cntx, 1.8f+9.0f*ff)*pow(cnty, 1.6f+3.0f*ff2); 							float r =
                //pow(cntx, 1.2f+2.0f*ff2)*pow(cnty, 1.0f+7.0f*ff); 							float r, g;
                float b = 0;
                g       = 0;
                r       = 0;
                //							float sc =
                //((ctxt.frameStep%1000)/1000.0f)*244.0f/256.0f; 							if (ff2*ff2>0.3*ff) {
                //								sc+=0.4f;
                //							}
                float fColHue = (cnty * cntx) + (frameStep + y) * 0.01f;
                auto hsl      = glm::hsvColor(vec3{ math::clamp(r, 0.0f, 1.0f),
                                               math::clamp(g, 0.0f, 1.0f),
                                               math::clamp(b, 0.0f, 1.0f) });
                auto col =
                        glm::rgbColor(vec3{ 0.05 + fmodf(fColHue, 1.0f) * 0.9, 0.98f, 0.4f });
                r = math::max(0.0f, cntx - 0.9f) * 1.0f * cnty * col.r *
                    (0.5 + 0.5 * sin(ff * 3.1495 * 2.0 + M_PI * 0.0f / 3.0f));
                g = math::max(0.0f, cntx - 0.9f) * 1.0f * cnty * col.g *
                    (0.5 + 0.5 * sin(ff * 3.1495 * 2.0 + M_PI * 2.0f / 3.0f));
                b = math::max(0.0f, cntx - 0.9f) * 1.0f * cnty * col.b *
                    (0.5 + 0.5 * sin(ff * 3.1495 * 2.0 + M_PI * 4.0f / 3.0f));
                r = col.b;
                g = col.r;
                b = col.g;
                if ((x & 1) ^ (y & 1)) {
                    //								float
                    //ffff = fmodf(fx+(ctxt.frameStep+y)*0.05f, 1.0f); 								NVGcolor col =
                    //HSLtoRGB(0.05+(1.0-0.05)*fx*fy*ff, (0.98), 0.39f); 								b +=
                    //pow(cntx, 1.6f+1.0f*ff)*pow(cnty, 1.6f+6.0f*ff2)*1.1f;
                    float sc = 0.2 + (6.4f) * (0.5 + 0.5 * sin(3.1495f * 2 *
                                                               ((frameStep) * 0.00754f)));
                    r *= sc;
                    g *= sc;
                    b *= sc;

                } else {
                    if (fmodf((frameStep / 66.0f), 8.0f) < 4.0f) {
                        auto hsl = rgbToHSL(vec3{ r, g, b });
                        col      = HSLtoRGB(vec3{ fmodf(hsl.x + 0.5f, 1.0f), hsl.y, hsl.z });
                        r        = col.r;
                        g        = col.g;
                        b        = col.b;
                    }
                    //								g +=
                    //pow(cntx, 1.6f+2.0f*ff)*pow(cnty, 1.6f+4.0f*ff2)*2.1f;
                }
                fColHue = hsl.r + fColHue;
                fColHue = fmodf(fColHue, 1.0f);
                //							NVGcolor col =
                //HSLtoRGB(fColHue, hsl.g, hsl.b);
                float ffff = fmodf(fx + (frameStep + y) * 0.05f, 1.0f);
                //							NVGcolor col =
                //HSLtoRGB(0.05+(1.0-0.05)*fx*fy*ff, (0.98), 0.39f); 							NVGcolor col =
                //HSLtoRGB(0.05+(1.0-0.05)*(rand.rng_bits(12)/4095.0f), (0.98),
                //0.0f+0.5f*(rand.rng_bits(12)/4095.0f));

                float sc2 = 1.0f;
                r *= sc2;
                g *= sc2;
                b *= sc2;
                //							g=col.g*sc;
                //							b=col.b*sc;
                //							if
                //(x*y<ctxt.frameStep%LED_NUM_TOTAL*2) { 								r+=0.3f;
                //							}
                //							if
                //(x+y*LED_STRIP_LED_NUMBERS==ctxt.frameStep%LED_NUM_TOTAL*2) { 								b+=0.3f;
                //							}
                writeToFrameBuffer(buffer, xOut, yOut, vec4{ r, g, b, 1.0f });
            }
        }
    }
    void runProgram2(frame_render_ctxt_t& renderCtxt,
                     frame_rgb_buffer_t& buffer) {
        const uint32_t frameStep = renderCtxt.frameStep;
        const int16_t lampId     = renderCtxt.lampId;
        if (0 == renderCtxt.frameStep) {
        }
        seq_rand rand;
        rand.rng_seed(frameStep >> 1);
        memset(arr, 0, sizeof(uint32_t) * LED_NUM_TOTAL);
        for (int xOut = 0; xOut < buffer.w; xOut++) {
            for (int yOut = 0; yOut < buffer.h; yOut++) {
                if (rand.rng_bits(8)) {
                    writeToFrameBuffer(
                            buffer, xOut, yOut,
                            vec4{ 0, 0, (rand.rng_rand() & 0xFFF) / (float) 0xFFF, 1 });
                } else {
                    writeToFrameBuffer(buffer, xOut, yOut, vec4{ 1, 1, 1, 1 } * 0.5f);
                }
            }
        }
    }
    void runProgram3(frame_render_ctxt_t& renderCtxt,
                     frame_rgb_buffer_t& buffer) {
        const uint32_t frameStep = renderCtxt.frameStep;
        const int16_t lampId     = renderCtxt.lampId;
        memset(arr, 0, sizeof(uint32_t) * LED_NUM_TOTAL);
        for (int xOut = 0; xOut < buffer.w; xOut++) {
            for (int yOut = 0; yOut < buffer.h; yOut++) {

                int x = xOut;
                int y = yOut;
                if (lampId >= 2) {
                    y += buffer.h;
                }
                float fx = x / (float) (LED_STRIP_LED_NUMBERS - 1);
                float fy = y / (float) (LED_STRIP_NUM_STRIPS - 1);
                float r, g, b, a;
                r = g = b = a = 1.0f;

                r = fmodf(frameStep / 120.0f, 1.0f);
                g = fy;
                b = fx;

                if (frameStep % LED_STRIP_LED_NUMBERS == x) {
                    r = g = b = 0;
                }
                if ((frameStep / 3) % (LED_STRIP_NUM_STRIPS) == y) {
                    r = g = b = 0;
                }
                writeToFrameBuffer(buffer, xOut, yOut, vec4{ r, g, b, a });
            }
        }
    }
    void runProgram4(frame_render_ctxt_t& renderCtxt,
                     frame_rgb_buffer_t& buffer) {
        const uint32_t frameStep = renderCtxt.frameStep;
        const int16_t lampId     = renderCtxt.lampId;
        memset(arr, 0, sizeof(uint32_t) * LED_NUM_TOTAL);
        static uint32_t prevStep = -1;
        uint32_t curStripeStep =
                uint32_t(frameStep / 2) % uint32_t(LED_STRIP_NUM_STRIPS);
        if (prevStep != curStripeStep) {
            prevStep = curStripeStep;
        }
        for (int xOut = 0; xOut < buffer.w; xOut++) {
            for (int yOut = 0; yOut < buffer.h; yOut++) {
                // int x = xOut;
                int y = yOut;
                if (lampId >= 2) {
                    y += buffer.h;
                }
                float r, g, b, a;
                r = g = b = a = 1.0f;
                if (curStripeStep == (uint32_t) y) {
                    r = g = b = 0;
                }
                writeToFrameBuffer(buffer, xOut, yOut, vec4{ r, g, b, a });
            }
        }
    }
    void runProgram5(frame_render_ctxt_t& renderCtxt,
                     frame_rgb_buffer_t& buffer) {
        const uint32_t frameStep = renderCtxt.frameStep;
        const int16_t lampId     = renderCtxt.lampId;
        memset(arr, 0, sizeof(uint32_t) * LED_NUM_TOTAL);
        static uint32_t prevStep = -1;
        uint32_t curStripeStep =
                uint32_t(frameStep / 2) % uint32_t(LED_STRIP_NUM_STRIPS);
        if (prevStep != curStripeStep) {
            prevStep = curStripeStep;
        }
        auto col = fromHex(LED_COLOR_DEFAULT);
        log_lf(Log::L_DEBUG, "color %f %f %f\n", col.r, col.g, col.b);
        float r = col.r;
        float g = col.g;
        float b = col.b;
        for (int xOut = 0; xOut < buffer.w; xOut++) {
            for (int yOut = 0; yOut < buffer.h; yOut++) {
                // int x = xOut;
                int y = yOut;
                if (lampId >= 2) {
                    y += buffer.h;
                }
                // r = g = b = a = 1.0f;
                // if (curStripeStep == (uint32_t)y) {
                //   r = g = b = 0;
                // }
                // simulate 600 to 700 nm spectrum
                // float f = (y / (float)(LED_STRIP_NUM_STRIPS));
                // if (f < 0.5f) {
                //   r = 0.0f;
                //   g = f * 2.0f;
                //   b = 1.0f - f * 2.0f;
                // } else {
                //   r = (f - 0.5f) * 2.0f;
                //   g = 1.0f - (f - 0.5f) * 2.0f;
                //   b = 0.0f;
                // }
                writeToFrameBuffer(buffer, xOut, yOut, vec4{ r, g, b, 1.0 });
            }
        }
    }
    void runProgram1(frame_render_ctxt_t& renderCtxt,
                     frame_rgb_buffer_t& buffer) {
        const uint32_t frameStep = renderCtxt.frameStep;
        if (0 == renderCtxt.frameStep) {
            memset(arr, 0, sizeof(uint32_t) * LED_NUM_TOTAL);
        }

        for (int xOut = 0; xOut < buffer.w; xOut++) {
            for (int yOut = 0; yOut < buffer.h; yOut++) {

                writeToFrameBuffer(buffer, xOut, yOut, vec4{ 0, 0, 0, 1 });
            }
        }

        int X = buffer.w;
        int Y = buffer.h;
        int x, y, dx, dy;
        x = y = dx    = 0;
        dy            = -1;
        int t         = std::max(X, Y);
        int maxI      = t * t;
        int numWrites = 0;
        int spiralLen = (renderCtxt.frameStep >> 1) % (LED_NUM_TOTAL * 2);
        for (int i = 0; i < maxI && numWrites < spiralLen; i++) {
            if ((-X / 2 <= x) && (x <= X / 2) && (-Y / 2 <= y) && (y < Y / 2)) {
                int32_t posX = (X / 2) + x - 1;
                int32_t posY = (Y / 2) + y;
                float f =
                        fmodf((numWrites) * (1.0f / (float) (LED_NUM_TOTAL * 2.0f)), 1.0f);
                //				NVGcolor col = HSLtoRGB(0.05f+f*0.9f,
                //0.98f, 0.5f);
                float fx = x / (float) (LED_STRIP_LED_NUMBERS - 1);
                float fy = y / (float) (LED_STRIP_NUM_STRIPS - 1);
                auto col = HSLtoRGB(
                        vec3{ 0.05f + fx * 0.9f, 0.01f + (1.0f - f) * 0.98f, 1.0f });

                writeToFrameBuffer(buffer, posX, posY, vec4{ col.r, col.g, col.b, 1 });
                numWrites++;
                // DO STUFF...
            }
            if ((x == y) || ((x < 0) && (x == -y)) || ((x > 0) && (x == 1 - y))) {
                t  = dx;
                dx = -dy;
                dy = t;
            }
            x += dx;
            y += dy;
        }
    }

    void runProgram6(frame_render_ctxt_t& renderCtxt,
                     frame_rgb_buffer_t& buffer) {
        const uint32_t frameStep = renderCtxt.frameStep;
        if (0 == renderCtxt.frameStep) {
            memset(arr, 0, sizeof(uint32_t) * LED_NUM_TOTAL);
        }
        for (int xOut = 0; xOut < buffer.w; xOut++) {
            for (int yOut = 0; yOut < buffer.h; yOut++) {
                int x = xOut;
                int y = yOut;
                float fx = x / (float) (LED_STRIP_NUM_STRIPS - 1);
                float fy = y / (float) (LED_STRIP_LED_NUMBERS - 1);
                float dst = sqrtf((fx - 0.5f) * (fx - 0.5f) + (fy - 0.5f) * (fy - 0.5f));
                float hue = dst * 360.0f;
                float speed = 3.0f;
                hue -= frameStep * speed;
                hue = -hue;
                while (hue < 0) {
                    hue += 360;
                }
                while (hue >= 360) {
                    hue -= 360;
                }
                float r = 0;
                float g = 0;
                float b = 0;
                float w = 0;
                if (hue < 60) {
                    r = 255;
                    g = hue / 60 * 255;
                } else if (hue < 120) {
                    r = (120 - hue) / 60 * 255;
                    g = 255;
                } else if (hue < 180) {
                    g = 255;
                    b = (hue - 120) / 60 * 255;
                } else if (hue < 240) {
                    g = (240 - hue) / 60 * 255;
                    b = 255;
                } else if (hue < 300) {
                    r = (hue - 240) / 60 * 255;
                    b = 255;
                } else {
                    r = 255;
                    b = (360 - hue) / 60 * 255;
                }
                writeToFrameBuffer(buffer, xOut, yOut, vec4{ r, g, b, w } / 255.0f);
            }
        }
    }
};

std::vector<uint32_t> createEmptyFrameData(int w, int h, uint32_t rgb) {
    std::vector<uint32_t> buf(size_t(w * h));
    std::fill(buf.begin(), buf.end(), rgb);    
    return buf;
}

class rgb_control_app {
    std::shared_ptr<network_conn_t> conn;
    const char* host = "192.168.188.10";
    uint16_t port = 54321;
public:
    void sendPacket(uint8_t* data, size_t size) {
        conn->sendTo(host, port, data, size);
    }
    void sendConfigMessage(uint32_t cfgId, uint32_t value) {
        packet_config_t config{};
        config.header.packetType = RGBNetworkPacketType::PKT_TYPE_CONFIG;
        config.header.len        = sizeof(config) - sizeof(config.header);
        config.message.cfgId     = cfgId;
        config.message.value     = value;
        sendPacket((uint8_t*) &config, sizeof(config));
    };
    void sendHeartbeatMessage(uint32_t frameId) {
        packet_heartbeat_t heartbeat{};
        heartbeat.header.packetType = RGBNetworkPacketType::PKT_TYPE_HEARTBEAT;
        heartbeat.header.len        = sizeof(heartbeat) - sizeof(heartbeat.header);
        heartbeat.message.frameId   = frameId;
        sendPacket((uint8_t*) &heartbeat, sizeof(heartbeat));
    }
    void sendFrameMessage(uint32_t frameId, std::vector<uint32_t>& data) {
        packet_led_frame_t frame{};
        frame.header.packetType = RGBNetworkPacketType::PKT_TYPE_LED_FRAME;
        frame.header.len        = data.size() * sizeof(uint32_t) +  sizeof(frame.message);
        frame.message.frameSize   = data.size();
        frame.message.frameOffset = 0;
        std::vector<uint8_t> buf(sizeof(frame) + data.size() * sizeof(uint32_t));
        memcpy(buf.data(), &frame, sizeof(frame));
        memcpy(buf.data() + sizeof(frame), data.data(), data.size() * sizeof(uint32_t));
        sendPacket(buf.data(), buf.size());
    }
    int run(const std::vector<String>& args) {
        network_init();
        class rgbprotocol_udp_net_server : public inetwork_handler {
        };
        rgbprotocol_udp_net_server handler;
        network_io netio(&handler);
        netio.openUDPSocket(host, port, conn);
        auto emptyFrame = createEmptyFrameData(LED_STRIP_NUM_STRIPS, LED_STRIP_LED_NUMBERS, 0xffff00ff);
        if (conn) {
            sendFrameMessage(0, emptyFrame);
        }
        frame_render_ctxt_t ctxt = { 0, 0 };
        frame_rgb_buffer_t buffer = { emptyFrame.data(), emptyFrame.size(), LED_STRIP_NUM_STRIPS, LED_STRIP_LED_NUMBERS };

        if (conn) {
            RGBNetworkController controller;

            // first send a heartbeat message
            sendHeartbeatMessage(0);

            sendConfigMessage(CFG_ID_MAX_BRIGHTNESS, RGB_DISPLAY_BRIGHTNESS);
            sendConfigMessage(CFG_ID_FRAME_RATE, RGB_DISPLAY_FPS);
            sendConfigMessage(CFG_ID_STRIPES_ENABLE, RGB_DISPLAY_STRIPES_ENABLED);

            while (true) {
                controller.runProgram1(ctxt, buffer);
                sendFrameMessage(ctxt.frameStep, emptyFrame);
                // sleep according to the frame rate
                seqthreads::threadSleep(int32_t(1000/ RGB_DISPLAY_FPS));
                ctxt.frameStep++;
            }
        }
        network_cleanup();
        return 0;
    }
};

volatile bool fatalError;
int main(int argc, char* argv[]) {
    std::vector<String> vecArgs(&argv[0], &argv[argc]);
    setExceptionHandler();
    App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
    daw_tls::initNewTls();
    rgb_control_app app;
    return app.run(vecArgs);
}