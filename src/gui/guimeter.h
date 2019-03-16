#pragma once

#include "gui.h"
#include "theme.h"
#include "dsp_util.h"
#include "meter.h"

class gui_trackmeter : public guibase {
public:
	rmsmeter<16000>* const meter;
	gui_trackmeter(rmsmeter<16000>* _meter) :
		guibase(), meter(_meter) {
	}
	void render(NVGcontext* vg) {
		int32_t spacing = 1;
		ivec2 inset(spacing);
		ivec2 mtrPos = pos + inset;
		ivec2 mtrSize = size - inset * 2;
		float channelW = (mtrSize.x-(OUTPUT_CHANNELS+1)*spacing) / (float) OUTPUT_CHANNELS;
		const double scaledZero = dsp_util::scaledRange(0, dsp_util::MTR_FLOOR, dsp_util::MTR_CEIL);
		float hZero = (1.0f - scaledZero) * mtrSize.y;
		float yZero = mtrPos.y + mtrSize.y - hZero;

		float x = mtrPos.x+spacing;
		for (int i = 0; i < OUTPUT_CHANNELS; i++) {
			float fMax = meter->getMax(i);
			float fRms = meter->getRms(i);
			float fPeak = meter->getStandingPeak(i);
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
				if (fLvl < F_MIN) {
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
					float hOvershoot = max(0.0f, hVal-hZero);
					nvgBeginPath(vg);
					nvgRect(vg, x, max(y, yZero), channelW, min(hVal, hZero));
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

		x = mtrPos.x+spacing;
		float x2 = mtrPos.x+(spacing+channelW)*2.0f;
		nvgBeginPath(vg);
		nvgMoveTo(vg, x, yZero);
		nvgLineTo(vg, x2, yZero);
		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
		nvgStrokeWidth(vg, 1.5f);
		nvgStroke(vg);
	}
};
