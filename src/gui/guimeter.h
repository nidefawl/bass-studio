#pragma once

#include "config.h"
#include "math/seq_math.h"
#include "gui.h"
#include "theme.h"
#include "dsp_util.h"
#include "meter.h"

template<typename METER>
void renderMeterAt(NVGcontext* vg, guitheme_t* theme, const ivec2& pos, const ivec2& size, METER* meter) {
		int32_t spacing = 1;
		ivec2 inset(spacing);
		ivec2 mtrPos = pos + inset;
		ivec2 mtrSize = size - inset * 2;
		auto lvls = meter->getLevels();
		int32_t nChannels = lvls.size();
		float channelW = (mtrSize.x-(nChannels+1)*spacing) / (float) nChannels;
		int textHeight = channelW;
		if (mtrSize.y - textHeight < 20) {
			textHeight = 0;
		}
		mtrSize.y-=textHeight;
		const double scaledZero = dsp_util::scaledRange(0, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
		float hZero = (1.0f - scaledZero) * mtrSize.y;
		float yZero = mtrPos.y + mtrSize.y - hZero;

		float x = mtrPos.x+spacing;
		float mixedlevels[3] = {0, 0, 0};
		for (int i = 0; i < lvls.size(); i++) {
			auto& chLvl = lvls[i];
			float fMax = chLvl.fMax;
			float fRms = chLvl.fLvl;
			float fPeak = chLvl.fPeak;
			mixedlevels[0] = math::max(mixedlevels[0], fMax);
			mixedlevels[1] = fRms;
			mixedlevels[2] = math::max(mixedlevels[2], fPeak);
			float levels[3] = {fMax, fRms, fPeak};

			nvgBeginPath(vg);
			nvgRect(vg, x, mtrPos.y, channelW, mtrSize.y);
			nvgFillColor(vg, theme->getFrameColorOutline());
			nvgFill(vg);
			NVGcolor colGainLvl[6] = {
				G_GREEN_DRK, G_YELLOW_DRK,
				G_GREEN, G_YELLOW,
				G_GREEN_DRKER, G_YELLOW_DRKER,
			};

			for (int i = 0; i < 3; i++ ){
				float fLvl = levels[i];
				if (fLvl < math::F_MIN) {
					continue;
				}
				double scale = dsp_util::scaledRange(dsp_util::dBFS(fLvl), dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
				float hVal = (1.0f - scale) * mtrSize.y;
				float y = mtrPos.y + mtrSize.y - hVal;
				if (i == 2) {
					nvgBeginPath(vg);
					nvgMoveTo(vg, x, y);
					nvgLineTo(vg, x+channelW, y);
//						int32_t col = fLvl >= 1.0f ? 1 : 0;
					int32_t col = y < yZero ? 1 : 0;
					nvgStrokeColor(vg, colGainLvl[i*2+col]);
					nvgStrokeWidth(vg, 1.5f);
					nvgStroke(vg);
					continue;
				}
				if (hVal > 0.5) {
					float hOvershoot = math::max(0.0f, hVal-hZero);
					nvgBeginPath(vg);
					nvgRect(vg, x, math::max(y, yZero), channelW, math::min(hVal, hZero));
					nvgFillColor(vg, colGainLvl[i*2+0]);
					nvgFill(vg);
					if (hOvershoot > 0) {
						nvgBeginPath(vg);
						nvgRect(vg, x, y, channelW, hOvershoot);
						nvgFillColor(vg, colGainLvl[i*2+1]);
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

		x = mtrPos.x+spacing;
		float x2 = mtrPos.x+(spacing+channelW)*nChannels;
		nvgBeginPath(vg);
		nvgMoveTo(vg, x, yZero);
		nvgLineTo(vg, x2, yZero);
		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
		nvgStrokeWidth(vg, 1.5f);
		nvgStroke(vg);
		if (textHeight) {
			UTIL_setFont(vg, theme, channelW*1.4, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
			float fMaxAll = mixedlevels[0];
			float lvl = dsp_util::dBFS(fMaxAll);
			String strLevel = StringFormat("%0.2f", lvl);
	//		nvgBeginPath(vg);
	//		nvgRect(vg, mtrPos.x, mtrPos.y+mtrSize.y, mtrSize.x, textHeight);
	//		nvgFillColor(vg, G_GREEN_DRK);
	//		nvgFill(vg);
	//		nvgFillColor(vg, G_WHITE);
			nvgText(vg, mtrPos.x+mtrSize.x, mtrPos.y+mtrSize.y, StringAsCStr(strLevel), NULL);
		}
	}
template<uint32_t T, uint32_t C = 2>
class gui_trackmeter : public guibase {
public:
	rmsmeter<T>* const meter;
	rmsmeterimpl<T, C>* const meterImpl;
	gui_trackmeter(rmsmeter<T>* _meter) :
		guibase(), meter(_meter), meterImpl(nullptr) {
	}
	gui_trackmeter(rmsmeterimpl<T, C>* _meterImpl) :
		guibase(), meter(nullptr), meterImpl(_meterImpl) {
	}
	void render(NVGcontext* vg) {
		if (meter)
		renderMeterAt(vg, theme, pos, size, meter);
		else
		renderMeterAt(vg, theme, pos, size, meterImpl);
	}
};
