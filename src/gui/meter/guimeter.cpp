#include "guimeter.h"
#include "config.h"
#include "math/seq_math.h"
#include "gui/gui.h"
#include "theme.h"
#include "dsp_util.h"
#include "host/meter/meter.h"
#include "color_util.h"
#include <nanovg.h>

constexpr float scaledZero = dsp_util::scaledRange(0.0f, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
constexpr float FOLDED_METER_CHANNEL_WIDTH_PX = 2.5f;

void renderMeterTextLevel(NVGcontext* vg, guitheme_t* theme, textlabel_dynamic_t* label, vec2 gainPos, vec2 gainSize, const String& strLevel, NVGcolor fontColor) {
    if (label && gainSize.x > 4 && gainSize.y > 4) {
        label->alignment = NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER;
        label->fontSize = gainSize.y * 1.1f;
        label->pos = gainPos + vec2(0);
        label->size = gainSize - vec2(0);
        label->adjustWidth();
        label->render(vg, theme, strLevel, fontColor);
    }
}

void renderMeterChannelHorizontal(NVGcontext* vg, guitheme_t* theme, ivec2 mtrPos, const ivec2& mtrSize, float channelH, float levels[3], NVGcolor colGainLvl[6]) {
    nvgBeginPath(vg);
    nvgRect(vg, mtrPos.x, mtrPos.y, mtrSize.x, channelH);
    nvgFillColor(vg, theme->getFrameColorOutline());
    nvgFill(vg);
    const auto wZero = (1.0f - scaledZero) * mtrSize.x;
    const auto xZero = mtrPos.x + mtrSize.x - wZero;
    for (int j = 0; j < 3; j++) {
        auto fLvl = levels[j];
        if (fLvl < math::F_MIN) {
            continue;
        }
        auto scale = dsp_util::scaledRange(dsp_util::dBFS(fLvl), dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
        auto hVal = (1.0f - scale) * mtrSize.x;
        auto x = mtrPos.x;
        auto y = mtrPos.y;
        if (j == 2) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, x + hVal, y);
            nvgLineTo(vg, x + hVal, y + channelH);
            //int32_t col = fLvl >= 1.0f ? 1 : 0;
            int32_t col = x < xZero ? 1 : 0;
            nvgStrokeColor(vg, colGainLvl[j * 2 + col]);
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
            continue;
        }
        if (hVal > 0.5) {
            auto hOvershoot = math::max(0.0f, hVal - wZero);
            nvgBeginPath(vg);
            nvgRect(vg, x, y, math::min(hVal, wZero), channelH);
            nvgFillColor(vg, colGainLvl[j * 2 + 0]);
            nvgFillCustomPar(vg, -3);
            nvgFill(vg);
            if (hOvershoot > 0) {
                nvgBeginPath(vg);
                nvgRect(vg, wZero, y, hOvershoot, channelH);
                nvgFillCustomPar(vg, -3);
                nvgFillColor(vg, colGainLvl[j * 2 + 1]);
                nvgFill(vg);
            }
        }
    }
}

void renderMeterChannelVertical(NVGcontext* vg, guitheme_t* theme, ivec2 mtrPos, const ivec2& mtrSize, float channelW, float levels[3], NVGcolor colGainLvl[6]) {
    nvgBeginPath(vg);
    nvgRect(vg, mtrPos.x, mtrPos.y, channelW, mtrSize.y);
    nvgFillColor(vg, theme->getFrameColorOutline());
    nvgFill(vg);
    const float hZero = (1.0f - scaledZero) * mtrSize.y;
    const float yZero = mtrPos.y + mtrSize.y - hZero;
    for (int j = 0; j < 3; j++) {
        float fLvl = levels[j];
        if (fLvl < math::F_MIN) {
            continue;
        }
        double scale = dsp_util::scaledRange(dsp_util::dBFS(fLvl), dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
        float hVal   = (1.0f - scale) * mtrSize.y;
        float x = mtrPos.x;
        float y = mtrPos.y + mtrSize.y - hVal;
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
            nvgFillCustomPar(vg, -3);
            nvgFill(vg);
            if (hOvershoot > 0) {
                nvgBeginPath(vg);
                nvgRect(vg, x, y, channelW, hOvershoot);
                nvgFillCustomPar(vg, -3);
                nvgFillColor(vg, colGainLvl[j * 2 + 1]);
                nvgFill(vg);
            }
        }
    }
}

void renderMeterAt(NVGcontext* vg, guitheme_t* theme, const ivec2& pos, const ivec2& size, DAW::rmsmeter* meter, textlabel_dynamic_t* label) {
    const int32_t CONST_PADDING_TRACK_CONTROLS = theme->get(GuiConstant::CONST_PADDING_TRACK_CONTROLS);
    const int32_t TRACK_HEIGHT_STEP   = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    const auto NCHANNELS = meter->getNumChannels();
    UIFont::font_instance instance    = theme->getFont(UIFont::FONT_DECIMAL);
    UIFont::bindFont(vg, instance);
    //  int32_t spacing = CONST_LAYOUT_MARGIN;
    //  ivec2 inset(spacing);
    vec2 gainPos  = { pos.x, pos.y };
    vec2 gainSize = { size.x, TRACK_HEIGHT_STEP - 2 * CONST_PADDING_TRACK_CONTROLS };
    ivec2 mtrPos  = { pos.x, pos.y + TRACK_HEIGHT_STEP };
    ivec2 mtrSize = { size.x, size.y - TRACK_HEIGHT_STEP };

    bool hasLegend = false;
    float lW       = mtrSize.x * 0.15f;
    hasLegend      = lW > 10;
    if (hasLegend) {
        mtrSize.x -= lW;
    }


    nvgBeginPath(vg);
    nvgRect(vg, gainPos.x, gainPos.y, gainSize.x, gainSize.y);
    nvgFillColor(vg, theme->getBgColor(0));
    //  nvgFillColor(vg, G_GREEN);
    nvgFill(vg);

    float x           = mtrPos.x;
    float channelW    = (mtrSize.x - (NCHANNELS - 1) * CONST_PADDING_TRACK_CONTROLS) / (float) NCHANNELS;

    float mixedlevels[3]    = { 0, 0, 0 };
    NVGcolor colGainLvl[6] = {
        G_GREEN_DRK,
        G_YELLOW_DRK,
        G_GREEN,
        G_YELLOW,
        G_GREEN_DRKER,
        G_YELLOW_DRKER,
    };
    for (channelnum_t i = 0; i < NCHANNELS; i++) {
        auto chLvl      = meter->getChannelLvls(i);
        float fMax      = chLvl.fMax;
        float fRms      = chLvl.fLvl;
        float fPeak     = chLvl.fPeak;
        mixedlevels[0]  = math::max(mixedlevels[0], fMax);
        mixedlevels[1]  = fRms;
        mixedlevels[2]  = math::max(mixedlevels[2], fPeak);
        float levels[3] = { fMax, fRms, fPeak };
        //    float levels[3] = {fMax, fRms, fPeak};
        if (mtrSize.y > 4 && channelW >= 1.0f) {
            renderMeterChannelVertical(vg, theme, {x, mtrPos.y}, mtrSize, channelW, levels, colGainLvl);
        } else  if (mtrSize.x > 20) {
            // render 2 pixel high channel near text label
            auto mtrPos2 = gainPos + vec2(0, gainSize.y - (NCHANNELS - i) * FOLDED_METER_CHANNEL_WIDTH_PX);
            auto mtrSize2 = vec2(gainSize.x, FOLDED_METER_CHANNEL_WIDTH_PX);
            renderMeterChannelHorizontal(vg, theme, mtrPos2, mtrSize2, FOLDED_METER_CHANNEL_WIDTH_PX, levels, colGainLvl);
            gainSize.y -= FOLDED_METER_CHANNEL_WIDTH_PX;
        }


        x += channelW;
        x += CONST_PADDING_TRACK_CONTROLS;
    }

    if (hasLegend && size.y > TRACK_HEIGHT_STEP * 1.5) {
        const float hZero = (1.0f - scaledZero) * mtrSize.y;
        const float yZero = mtrPos.y + mtrSize.y - hZero;
        float x2           = pos.x + size.x - lW;
        const int steps    = 8;
        float stops[steps] = {
            0.0f, -3.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f
        };
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
        nvgFontSize(vg, lW * 0.7);
        nvgFillColor(vg, THEMECOL_TEXT);
        nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        float prevStopY = -4;
        for (int i = 0; i < steps; i++) {
            float lvlStop = stops[i];
            double scale  = dsp_util::scaledRange(lvlStop, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
            float y       = mtrPos.y + scale * mtrSize.y;
            if (i > 0 && y - prevStopY < lW * 0.6) {
                continue;
            }
            prevStopY = y;

            String strLevel = ".";
            strLevel = StringFormat("%.0f", lvlStop);
            nvgText(vg, x2 + lW * 7.8f / 8.0f, y, StringAsCStr(strLevel), NULL);
        }
    }

    if (label && gainSize.x > 4 && gainSize.y > 4) {
        mixedlevels[1] /= (float) NCHANNELS;
        float fMaxAll = mixedlevels[2];
        float lvl     = dsp_util::dBFS(fMaxAll);
        nvgSave(vg);
        nvgIntersectScissor(vg, gainPos.x, gainPos.y, gainSize.x, gainSize.y);
        auto fontColor = THEMECOL_TEXT;
        String strLevel = ".";
        if (lvl > -96.0f) {
            strLevel = StringFormat("%.1f", lvl);
        } else {
            strLevel = StringFormat("%.0f", lvl);
        }
        renderMeterTextLevel(vg, theme, label, gainPos, gainSize, strLevel, fontColor);
        nvgRestore(vg);
    }
}
void renderMeterHorizontal(NVGcontext *vg, guitheme_t *theme, const vec2 &pos, const vec2 &size, DAW::rmsmeter *meter, textlabel_dynamic_t* label) {
    const int32_t CONST_PADDING_TRACK_CONTROLS = theme->get(GuiConstant::CONST_PADDING_TRACK_CONTROLS);
    const int32_t TRACK_HEIGHT_STEP   = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    const auto NCHANNELS = meter->getNumChannels();
    UIFont::font_instance instance    = theme->getFont(UIFont::FONT_DECIMAL);
    UIFont::bindFont(vg, instance);
    auto widthGain = size.x * 0.20f;
    vec2 mtrPos  = pos;
    vec2 mtrSize = {size.x - widthGain - CONST_PADDING_TRACK_CONTROLS, size.y};
    vec2 gainPos  = {size.x - widthGain, mtrPos.y};
    vec2 gainSize = { widthGain, mtrSize.y };

    bool hasLegend = false;
    float lW       = mtrSize.y * 0.15f;

    nvgBeginPath(vg);
    nvgRect(vg, gainPos.x, gainPos.y, gainSize.x, gainSize.y);
    nvgFillColor(vg, theme->getBgColor(0));
    nvgFill(vg);

    auto y           = mtrPos.y;
    auto channelH    = (mtrSize.y - (NCHANNELS - 1) * CONST_PADDING_TRACK_CONTROLS) / (float) NCHANNELS;

    float mixedlevels[3]    = { 0, 0, 0 };
    NVGcolor colGainLvl[6] = {
        G_GREEN_DRK,
        G_YELLOW_DRK,
        G_GREEN,
        G_YELLOW,
        G_GREEN_DRKER,
        G_YELLOW_DRKER,
    };
    for (channelnum_t i = 0; i < NCHANNELS; i++) {
        auto chLvl      = meter->getChannelLvls(i);
        float fMax      = chLvl.fMax;
        float fRms      = chLvl.fLvl;
        float fPeak     = chLvl.fPeak;
        mixedlevels[0]  = math::max(mixedlevels[0], fMax);
        mixedlevels[1]  = fRms;
        mixedlevels[2]  = math::max(mixedlevels[2], fPeak);
        float levels[3] = { fMax, fRms, fPeak };
        //    float levels[3] = {fMax, fRms, fPeak};
        if (mtrSize.x > 4 && channelH >= 1.0f) {
            renderMeterChannelHorizontal(vg, theme, {mtrPos.x, y}, mtrSize, channelH, levels, colGainLvl);
        } else  if (mtrSize.y > 20) {
            // render 2 pixel high channel near text label
            auto mtrPos2 = gainPos + vec2(gainSize.x - (NCHANNELS - i) * FOLDED_METER_CHANNEL_WIDTH_PX, 0);
            auto mtrSize2 = vec2(FOLDED_METER_CHANNEL_WIDTH_PX, gainSize.y);
            renderMeterChannelVertical(vg, theme, mtrPos2, mtrSize2, FOLDED_METER_CHANNEL_WIDTH_PX, levels, colGainLvl);
            gainSize.x -= FOLDED_METER_CHANNEL_WIDTH_PX;
        }


        y += channelH;
        y += CONST_PADDING_TRACK_CONTROLS;
    }

    if (hasLegend && size.x > TRACK_HEIGHT_STEP * 1.5) {
        const auto wZero = (1.0f - scaledZero) * mtrSize.x;
        const auto xZero = mtrPos.x + mtrSize.x - wZero;
        float y2 = pos.y + size.y - lW;
        const int steps    = 8;
        float stops[steps] = {
            0.0f, -3.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f
        };
        nvgBeginPath(vg);
        nvgMoveTo(vg, xZero, mtrPos.y);
        nvgLineTo(vg, xZero, y2 + lW * 2.0f / 8.0f);
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
        nvgBeginPath(vg);
        for (float lvlStop : stops) {
            auto scale  = dsp_util::scaledRange(lvlStop, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
            auto x = mtrPos.x + scale * mtrSize.x;
            nvgMoveTo(vg, x, mtrPos.y);
            nvgLineTo(vg, x, y2 + lW * 2.0f / 8.0f);
        }
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
        nvgStrokeWidth(vg, 0.75f);
        nvgStroke(vg);
        nvgFontSize(vg, lW * 0.7f);
        nvgFillColor(vg, THEMECOL_TEXT);
        nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        float prevStopX = -4;
        for (int i = 0; i < steps; i++) {
            auto lvlStop = stops[i];
            auto scale  = dsp_util::scaledRange(lvlStop, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
            auto x = mtrPos.x + scale * mtrSize.x;
            if (i > 0 && x - prevStopX < lW * 0.6) {
                continue;
            }
            prevStopX = x;

            String text = StringFormat("%.0f", lvlStop);
            nvgText(vg, x, y2 + lW * 7.8f / 8.0f, text.c_str(), &text.back() + 1);
        }
    }
                    
    if (label && gainSize.x > 4 && gainSize.y > 4) {
        mixedlevels[1] /= (float) NCHANNELS;
        float fMaxAll = mixedlevels[2];
        float lvl     = dsp_util::dBFS(fMaxAll);
        nvgSave(vg);
        nvgIntersectScissor(vg, gainPos.x, gainPos.y, gainSize.x, gainSize.y);
        auto fontColor = getContrastFontColor(nvgToRGBA(theme->getBgColor(0)));
        String strLevel = ".";
        if (lvl > -96.0f) {
            strLevel = StringFormat("%.1f", lvl);
        } else {
            strLevel = StringFormat("%.0f", lvl);
        }
        renderMeterTextLevel(vg, theme, label, gainPos, gainSize, strLevel, fontColor);
        nvgRestore(vg);
    }
}

void gui_trackmeter::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    if (bRenderHorizontal) {
        renderMeterHorizontal(vg, theme, pos, size, meter, &label);
    } else {
        renderMeterAt(vg, theme, pos, size, meter, &label);
    }
}