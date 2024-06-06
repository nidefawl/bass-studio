#include "guicolors.h"
#include "host/shape/shape.h"
#include "theme.h"
#include <nanovg.h>
#include "shape-render.hpp"

namespace DAW::Shape {

void DrawGrid(NVGcontext* vg, const guitheme_t* theme, vec2 pos, vec2 size, int gridStepsH, int gridStepsV) {
    dbgassert(gridStepsH && gridStepsV);
    auto gridStep = vec2(size) / vec2(gridStepsH, gridStepsV);

    double bgRepeat = gridStep.x * 2.0;
    int32_t steps_bg    = math::ceildS32((size.x + bgRepeat) / gridStep.x);
    NVGpaint paint{};
    paint.image = -1;

    nvgGlobalAlpha(vg, 0.5f);
    nvgBeginPath(vg);
    nvgRect(vg, -2, 0, size.x + 2, size.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
    nvgFill(vg);
    /* draw dark grid areas */
    int32_t nRendered = 0;
    float x = pos.x;
    for (int32_t i = 0; i < steps_bg; i += 2) {
        nvgBatchedRect(vg, x, pos.y, gridStep.x, size.y);
        nRendered++;
        x += gridStep.x * 2.0f;
        if (x > size.x)
            break;
    }

    if (nRendered) {
        paint.innerColor = theme->getColor(GuiColor::COL_GRID_DRK);
        paint.customPar  = NVGBatchedShading::NVG_BATCHED_SHADED;
        nvgFillPaint(vg, paint);
        nvgBatchedRender(vg);
    }

    nvgGlobalAlpha(vg, 1.0f);
    int32_t stepBeat = 2;
    int32_t stepNth = 2;
    if (gridStepsH%3 == 0) {
        stepBeat = 3;
        stepNth = 1;
    }
    for (int32_t pass = 0; pass < 3; ++pass) {
        int32_t start = pass == 2 ? 1 : pass == 1 ? stepBeat : 0;
        int32_t step = pass < 2 ? stepBeat*2 : stepNth;
        nRendered = 0;
        for (int32_t i = start; i < gridStepsH; i += step) {
            float lineThickness = 4.0f;
            nvgBatchedRect(vg, pos.x + gridStep.x * i - lineThickness * 0.5f, pos.y, lineThickness, size.y);
            paint.feather = 2.5f - pass * 0.75f;
            nRendered++;
        }
        if (nRendered) {
            switch (pass) {
                case 0:
                    paint.innerColor = theme->getColor(GuiColor::COL_LINE_BAR);
                    break;
                case 1:
                    paint.innerColor = theme->getColor(GuiColor::COL_LINE_QRT);
                    break;
                case 2:
                default:
                    paint.innerColor = theme->getColor(GuiColor::COL_LINE_XTH);
                    break;
            }
            paint.customPar = NVGBatchedShading::NVG_BATCHED_LINE_VERTICAL;
            nvgFillPaint(vg, paint);
            nvgBatchedRender(vg);
        }
    }
    for (int32_t pass = 1; pass < 2; ++pass) {
        int32_t start = 0;
        int32_t step = 1;
        nRendered = 0;
        for (int32_t i = start; i < gridStepsV; i += step) {
            float lineThickness = 4.0f;
            nvgBatchedRect(vg, pos.x, pos.y+gridStep.y * i - lineThickness * 0.5f, size.x, lineThickness);
            paint.feather = 2.5f - pass * 0.75f;
            nRendered++;
        }
        if (nRendered) {
            switch (pass) {
                case 0:
                    paint.innerColor = theme->getColor(GuiColor::COL_LINE_BAR);
                    break;
                case 1:
                    paint.innerColor = theme->getColor(GuiColor::COL_LINE_QRT);
                    break;
                case 2:
                default:
                    paint.innerColor = theme->getColor(GuiColor::COL_LINE_XTH);
                    break;
            }
            paint.customPar = NVGBatchedShading::NVG_BATCHED_LINE_HORIZONTAL;
            nvgFillPaint(vg, paint);
            nvgBatchedRender(vg);
        }
    }
}

}// namespace DAW::Shape
