#pragma once

#include "config.h"
#include "math/seq_math.h"
#include "gui.h"
#include "theme.h"
#include "dsp_util.h"
#include "meter.h"
#include "color_util.h"

template<uint32_t NCHANNELS = 1, typename METER>
void renderMeterAt(NVGcontext* vg, guitheme_t* theme, const ivec2& pos, const ivec2& size, METER* meter) {
    const int32_t CONST_LAYOUT_MARGIN = math::min(6, theme->get(GuiConstant::CONST_LAYOUT_MARGIN));
    const int32_t TRACK_HEIGHT_STEP   = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    UIFont::font_instance instance    = theme->getFont(UIFont::FONT_DECIMAL);
    UIFont::bindFont(vg, instance);
    //  int32_t spacing = CONST_LAYOUT_MARGIN;
    //  ivec2 inset(spacing);
    vec2 gainPos  = { pos.x, pos.y };
    vec2 gainSize = { size.x, TRACK_HEIGHT_STEP - 2 * CONST_LAYOUT_MARGIN };
    ivec2 mtrPos  = { pos.x, pos.y + TRACK_HEIGHT_STEP };
    ivec2 mtrSize = { size.x, size.y - TRACK_HEIGHT_STEP };

    bool hasLegend = false;
    float lW       = mtrSize.x * 0.33f;
    hasLegend      = lW > 12;
    if (hasLegend) {
        mtrSize.x -= lW;
    }


    nvgBeginPath(vg);
    nvgRect(vg, gainPos.x, gainPos.y, gainSize.x, gainSize.y);
    nvgFillColor(vg, theme->getBgColor(0));
    //  nvgFillColor(vg, G_GREEN);
    nvgFill(vg);
    const double scaledZero = dsp_util::scaledRange(0, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
    float hZero             = (1.0f - scaledZero) * mtrSize.y;
    float yZero             = mtrPos.y + mtrSize.y - hZero;
    auto lvls               = meter->getLevels();
    float x                 = mtrPos.x;
    float channelW          = (mtrSize.x - (NCHANNELS - 1) * CONST_LAYOUT_MARGIN) / (float) NCHANNELS;
    float mixedlevels[3]    = { 0, 0, 0 };
    if (mtrSize.y > 4) {
        //    nvgBeginPath(vg);
        //    nvgRect(vg, mtrPos.x, mtrPos.y, mtrSize.x, mtrSize.y);
        //    nvgFillColor(vg, G_WHITE);
        //    nvgFill(vg);
        //    nvgBeginPath(vg);
        //    nvgRect(vg, mtrPos.x+1, mtrPos.y+1, mtrSize.x-2, mtrSize.y-2);
        //    nvgFillColor(vg, G_GREEN_DRK);
        //    nvgFill(vg);
        //    nvgFillColor(vg, G_WHITE);
    }
    for (int i = 0; i < NCHANNELS; i++) {
        auto& chLvl     = lvls[i];
        float fMax      = chLvl.fMax;
        float fRms      = chLvl.fLvl;
        float fPeak     = chLvl.fPeak;
        mixedlevels[0]  = math::max(mixedlevels[0], fMax);
        mixedlevels[1]  = fRms;
        mixedlevels[2]  = math::max(mixedlevels[2], fPeak);
        float levels[3] = { fMax, fRms, fPeak };
        //    float levels[3] = {fMax, fRms, fPeak};
        if (mtrSize.y > 4) {
            nvgBeginPath(vg);
            nvgRect(vg, x, mtrPos.y, channelW, mtrSize.y);
            nvgFillColor(vg, theme->getFrameColorOutline());
            nvgFill(vg);
            NVGcolor colGainLvl[6] = {
                G_GREEN_DRK,
                G_YELLOW_DRK,
                G_GREEN,
                G_YELLOW,
                G_GREEN_DRKER,
                G_YELLOW_DRKER,
            };
            for (int j = 0; j < 3; j++) {
                float fLvl = levels[j];
                if (fLvl < math::F_MIN) {
                    continue;
                }
                double scale = dsp_util::scaledRange(dsp_util::dBFS(fLvl), dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
                float hVal   = (1.0f - scale) * mtrSize.y;
                float y      = mtrPos.y + mtrSize.y - hVal;
                if (j == 2) {
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, x, y);
                    nvgLineTo(vg, x + channelW, y);
                    //            int32_t col = fLvl >= 1.0f ? 1 : 0;
                    int32_t col = y < yZero ? 1 : 0;
                    nvgStrokeColor(vg, colGainLvl[j * 2 + col]);
                    nvgStrokeWidth(vg, 1.5f);
                    nvgStroke(vg);
                    continue;
                }
                if (hVal > 0.5) {
                    float hOvershoot = math::max(0.0f, hVal - hZero);
                    nvgBeginPath(vg);
                    nvgRect(vg, x, math::max(y, yZero), channelW, math::min(hVal, hZero));
                    nvgFillColor(vg, colGainLvl[j * 2 + 0]);
                    nvgFill(vg);
                    if (hOvershoot > 0) {
                        nvgBeginPath(vg);
                        nvgRect(vg, x, y, channelW, hOvershoot);
                        nvgFillColor(vg, colGainLvl[j * 2 + 1]);
                        nvgFill(vg);
                    }
                }
            }
        }


        x += channelW;
        x += CONST_LAYOUT_MARGIN;
    }

    if (hasLegend && size.y > TRACK_HEIGHT_STEP * 1.5) {
        float x2           = pos.x + size.x - lW;
        const int steps    = 8;
        float stops[steps] = {
            0.0f, -3.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f
        };
        float hZero = (1.0f - scaledZero) * mtrSize.y;
        float yZero = mtrPos.y + mtrSize.y - hZero;
        nvgBeginPath(vg);
        nvgMoveTo(vg, mtrPos.x, yZero);
        nvgLineTo(vg, x2 + lW * 2.0f / 8.0f, yZero);
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
        nvgBeginPath(vg);
        for (int i = 0; i < steps; i++) {
            float lvlStop = stops[i];
            double scale  = dsp_util::scaledRange(lvlStop, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);

            float y = (i) / (steps - 1.0f);

            y = mtrPos.y + scale * mtrSize.y;
            nvgMoveTo(vg, mtrPos.x, y);
            nvgLineTo(vg, x2 + lW * 2.0f / 8.0f, y);
        }
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
        nvgStrokeWidth(vg, 0.75f);
        nvgStroke(vg);
        //    UTIL_setFont(vg, theme, lW*0.375, G_WHITE, NVG_ALIGN_BOTTOM | NVG_ALIGN_RIGHT);
        nvgFontSize(vg, lW * 0.375);
        nvgFillColor(vg, G_WHITE);
        nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        float prevStopY = -4;//dsp_util::scaledRange(0, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL)*mtrSize.y;
        for (int i = 0; i < steps; i++) {
            float lvlStop = stops[i];
            double scale  = dsp_util::scaledRange(lvlStop, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
            float y       = mtrPos.y + scale * mtrSize.y;
            if (i > 0 && y - prevStopY < lW * 0.6) {
                continue;
            }
            prevStopY = y;

            String strLevel = ".";
            //      if (lW > 36) {
            //        strLevel = StringFormat("%.2fdB", lvlStop);
            //      } else if (lW >= 15) {
            //        strLevel = StringFormat("%.2f", lvlStop);
            //      } else {
            strLevel = StringFormat("%.0fdB", lvlStop);
            //      }
            nvgText(vg, x2 + lW * 7.8f / 8.0f, y, StringAsCStr(strLevel), NULL);
        }
    }


    if (NCHANNELS > 0) {
        mixedlevels[1] /= (float) NCHANNELS;
    }
    float fMaxAll = mixedlevels[0];
    float lvl     = dsp_util::dBFS(fMaxAll);
    //  if (lvl > dsp_util::DBFS_INF_POS) {
    nvgSave(vg);
    getContrastFontColor(nvgToRGBA(theme->getBgColor(0)));
    nvgIntersectScissor(vg, gainPos.x, gainPos.y, gainSize.x, gainSize.y);
    //    UTIL_setFont(vg, theme, gainSize.y*0.8, G_WHITE, NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER);
    nvgFontSize(vg, gainSize.y * 0.8);
    nvgFillColor(vg, G_WHITE);
    nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);

    String strLevel = ".";
    if (gainSize.x >= 60) {
        strLevel = StringFormat("%.2fdB", lvl);
    } else if (gainSize.x >= 35) {
        strLevel = StringFormat("%.2f", lvl);
    } else {
        strLevel = StringFormat("%.0f", lvl);
    }
    //      if (lvl > -10.0f && lvl < 10.0f)
    //        strLevel = StringFormat("%0.1f", lvl);
    //      nvgBeginPath(vg);
    //      nvgRect(vg, mtrPos.x, mtrPos.y+mtrSize.y, mtrSize.x, textHeight);
    //      nvgFillColor(vg, G_WHITE);
    //      nvgFill(vg);
    //      nvgBeginPath(vg);
    //      nvgRect(vg, mtrPos.x+1, mtrPos.y+mtrSize.y+1, mtrSize.x-2, textHeight-2);
    //      nvgFillColor(vg, G_GREEN_DRK);
    //      nvgFill(vg);
    //      nvgFillColor(vg, G_WHITE);
    nvgText(vg, gainPos.x + gainSize.x * 0.98f, gainPos.y + gainSize.y / 2.0f + 0.5f, StringAsCStr(strLevel), NULL);
    nvgRestore(vg);

    //  }
}
template<uint32_t NCHANNELS = 1, typename METER>
void renderMeterAt2(NVGcontext* vg, guitheme_t* theme, const ivec2& pos, const ivec2& size, METER* meter) {
    const int32_t CONST_LAYOUT_MARGIN = math::min(6, theme->get(GuiConstant::CONST_LAYOUT_MARGIN));
    const int32_t TRACK_HEIGHT_STEP   = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    int32_t spacing                   = CONST_LAYOUT_MARGIN;
    ivec2 inset(spacing);
    ivec2 mtrPos   = pos + inset;
    ivec2 mtrSize  = size;
    bool hasLegend = false;
    float lW       = mtrSize.x * 0.33f;
    //  if (mtrSize.x >= 120) {
    //    lW = 36;
    //  } else if (mtrSize.x >= 80) {
    //    lW = 20;
    //  }
    if (mtrSize.x < TRACK_HEIGHT_STEP * 2) {
        //    lW = 0;
    }
    hasLegend = lW > 0;
    if (hasLegend)
        mtrSize.x -= lW;
    mtrSize -= inset;
    lW -= inset.x * 2;
    auto lvls         = meter->getLevels();
    int32_t nChannels = lvls.size();
    float channelW    = (mtrSize.x - (nChannels + 1) * spacing) / (float) nChannels;
    int textHeight    = TRACK_HEIGHT_STEP;
    const double scaledZero = dsp_util::scaledRange(0, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
    float hZero             = (1.0f - scaledZero) * mtrSize.y;
    float yZero             = mtrPos.y + mtrSize.y - hZero;

    float x              = mtrPos.x + spacing;
    float mixedlevels[3] = { 0, 0, 0 };
    for (int i = 0; i < NCHANNELS; i++) {
        auto& chLvl     = lvls[i];
        float fMax      = chLvl.fMax;
        float fRms      = chLvl.fLvl;
        float fPeak     = chLvl.fPeak;
        mixedlevels[0]  = math::max(mixedlevels[0], fMax);
        mixedlevels[1]  = fRms;
        mixedlevels[2]  = math::max(mixedlevels[2], fPeak);
        float levels[3] = { fMax, fRms, fPeak };

        nvgBeginPath(vg);
        nvgRect(vg, x, mtrPos.y, channelW, mtrSize.y);
        nvgFillColor(vg, theme->getFrameColorOutline());
        nvgFill(vg);
        NVGcolor colGainLvl[6] = {
            G_GREEN_DRK,
            G_YELLOW_DRK,
            G_GREEN,
            G_YELLOW,
            G_GREEN_DRKER,
            G_YELLOW_DRKER,
        };

        for (int j = 0; j < 3; j++) {
            float fLvl = levels[j];
            if (fLvl < math::F_MIN) {
                continue;
            }
            double scale = dsp_util::scaledRange(dsp_util::dBFS(fLvl), dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
            float hVal   = (1.0f - scale) * mtrSize.y;
            float y      = mtrPos.y + mtrSize.y - hVal;
            if (j == 2) {
                nvgBeginPath(vg);
                nvgMoveTo(vg, x, y);
                nvgLineTo(vg, x + channelW, y);
                //            int32_t col = fLvl >= 1.0f ? 1 : 0;
                int32_t col = y < yZero ? 1 : 0;
                nvgStrokeColor(vg, colGainLvl[j * 2 + col]);
                nvgStrokeWidth(vg, 1.5f);
                nvgStroke(vg);
                continue;
            }
            if (hVal > 0.5) {
                float hOvershoot = math::max(0.0f, hVal - hZero);
                nvgBeginPath(vg);
                nvgRect(vg, x, math::max(y, yZero), channelW, math::min(hVal, hZero));
                nvgFillColor(vg, colGainLvl[j * 2 + 0]);
                nvgFill(vg);
                if (hOvershoot > 0) {
                    nvgBeginPath(vg);
                    nvgRect(vg, x, y, channelW, hOvershoot);
                    nvgFillColor(vg, colGainLvl[j * 2 + 1]);
                    nvgFill(vg);
                }
            }
        }
        x += channelW;
        x += spacing;
    }
    if (nChannels > 0) {
        mixedlevels[1] /= nChannels;
    }

    x        = mtrPos.x + spacing;
    float x2 = mtrPos.x + (spacing + channelW) * nChannels;
    nvgBeginPath(vg);
    nvgMoveTo(vg, x, yZero);
    nvgLineTo(vg, x2, yZero);
    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
    nvgStrokeWidth(vg, 1.5f);
    nvgStroke(vg);

    if (mtrSize.x < TRACK_HEIGHT_STEP * 2) {
        //    lW = 0;
    }
    x2 += lW / 8.0;
    if (hasLegend && size.y > TRACK_HEIGHT_STEP * 1.5) {
        const int steps    = 7;
        float stops[steps] = {
            0.0f, -3.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f
        };
        float hZero = (1.0f - scaledZero) * mtrSize.y;
        float yZero = mtrPos.y + mtrSize.y - hZero;
        nvgBeginPath(vg);
        nvgMoveTo(vg, x2, yZero);
        nvgLineTo(vg, x2 + lW * 2.0f / 8.0f, yZero);
        for (int i = 0; i < steps; i++) {
            float lvlStop = stops[i];
            double scale  = dsp_util::scaledRange(lvlStop, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);

            float y = (i) / (steps - 1.0f);

            y = mtrPos.y + scale * mtrSize.y;
            nvgMoveTo(vg, x2, y);
            nvgLineTo(vg, x2 + lW * 2.0f / 8.0f, y);
        }
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
        UTIL_setFont(vg, theme, lW * 0.375, G_WHITE, NVG_ALIGN_BOTTOM | NVG_ALIGN_RIGHT);
        float prevStopY = mtrPos.y + textHeight;
        for (int i = 1; i < steps; i++) {
            float lvlStop = stops[i];
            double scale  = dsp_util::scaledRange(lvlStop, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);

            float y = (i) / (steps - 1.0f);

            y = mtrPos.y + scale * mtrSize.y;
            if (i > 0 && y - prevStopY < lW * 0.6) {
                continue;
            }

            String strLevel = ".";
            //      if (lW > 36) {
            //        strLevel = StringFormat("%.2fdB", lvlStop);
            //      } else if (lW >= 15) {
            //        strLevel = StringFormat("%.2f", lvlStop);
            //      } else {
            strLevel = StringFormat("%.0fdB", lvlStop);
            //      }
            nvgText(vg, x2 + lW, y, StringAsCStr(strLevel), NULL);
        }
    }
    float fMaxAll = mixedlevels[0];
    float lvl     = dsp_util::dBFS(fMaxAll);
    if (lvl > dsp_util::DBFS_INF_POS) {
        if (textHeight) {
            nvgSave(vg);
            nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
            UTIL_setFont(vg, theme, lW * 0.5, G_WHITE, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);

            String strLevel = ".";
            if (mtrSize.x >= 60) {
                strLevel = StringFormat("%.2fdB", lvl);
            } else if (mtrSize.x >= 35) {
                strLevel = StringFormat("%.2f", lvl);
            } else {
                strLevel = StringFormat("%.0f", lvl);
            }
            //      if (lvl > -10.0f && lvl < 10.0f)
            //        strLevel = StringFormat("%0.1f", lvl);
            //      nvgBeginPath(vg);
            //      nvgRect(vg, mtrPos.x, mtrPos.y+mtrSize.y, mtrSize.x, textHeight);
            //      nvgFillColor(vg, G_WHITE);
            //      nvgFill(vg);
            //      nvgBeginPath(vg);
            //      nvgRect(vg, mtrPos.x+1, mtrPos.y+mtrSize.y+1, mtrSize.x-2, textHeight-2);
            //      nvgFillColor(vg, G_GREEN_DRK);
            //      nvgFill(vg);
            //      nvgFillColor(vg, G_WHITE);
            nvgText(vg, mtrPos.x + mtrSize.x + spacing + lW, mtrPos.y + textHeight / 2.0f + 0.5f, StringAsCStr(strLevel), NULL);
            nvgRestore(vg);
        }
    }
}
