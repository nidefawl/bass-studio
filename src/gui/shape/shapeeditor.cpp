
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <nanovg.h>
#include <nanovg_min.h>
#include <type_traits>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <array>

#include "assert_dbg.h"
#include "grid.h"
#include "gui/controls/button.h"
#include "gui/dialog/dialogs.h"
#include "gui/dropdown/dropdown_preset_tree.h"
#include "gui/gui.h"
#include "gui/shape/shapeeditor.h"
#include "guicolors.h"
#include "guiglobals.h"
#include "keyboard.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "renderresources.h"
#include "seq_util.h"
#include "str_util.h"
#include "basectrl.h"
#include "gui/container/container.h"
#include "gui/container/container_layout.h"
#include "gui/container/container_layout_types.h"
#include "gui/controls/knob.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/colorpick.h"
#include "gui/dropdown/dropdown_generic.h"
#include "logging.h"
#include "platform.h"
#include "theme.h"
#include "util/presetmanager.h"
#include "shape.h"
#include "file/shapefile.h"

namespace DAW::Shape {

void DrawShapeOneShot(const shape_t& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 sizeScaled, float xClipMin, float xClipMax, const shape_t::hit_result& hit) {
    if (curve.pts.empty())
        return;
    auto numCurvePts = CtrSize(curve.pts);
    //TODO: This is not efficient. Either add caching or at least use references and a scratch pad vector
    const std::vector<shape_pt_t>& pts = curve.pts;
    auto nPoints = CtrSize(curve.pts);
    float radiusHandle = 3.0f;
    float strokeWidth = 3.0f;
    auto lineColor = theme->getColor(col);
    auto fillColor = lineColor;
    auto handleColor = theme->getColor(GuiColor::COL_KNOB_IND);
    auto hoverColor = theme->getColor(colHovered);
    fillColor.a = 0.3;
    for (int32_t pass = 0; pass < 2; ++pass) {
        if (pass == 0) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, pos.x + pts.front().pos.x*sizeScaled.x-5, pos.y + sizeScaled.y);
        }
        for (int32_t edge = 0; edge < nPoints - 1; edge++) {
            const auto& pt0 = pts[edge];
            const auto& pt1 = pts[edge+1];
            auto ptD = pt1.pos - pt0.pos;
            int32_t numSteps = math::max<int32_t>(0, math::ceilfS32(ptD.x*sizeScaled.x*0.5f));
            if (numSteps == 1)numSteps++;
            if (!numSteps) continue;
            if (pass == 0) {
                nvgLineTo(vg, pos.x + pt0.pos.x*sizeScaled.x, pos.y + (1.0f - pt0.pos.y) * sizeScaled.y);
            }
            if (pass == 1) { // lines
                nvgBeginPath(vg);
                nvgMoveTo(vg, pos.x + pt0.pos.x*sizeScaled.x, pos.y + (1.0f - pt0.pos.y) * sizeScaled.y);
            }
            for (int32_t step = 0; step < numSteps; step++) {
                float x = step / float(numSteps);
                float ts = curve.shapeSegment(x, pt0.shape);
                dbgassert(ts >= 0.0f && ts <= 1.0f);
                auto ps = vec2(x, ts) * ptD + pt0.pos;
                if (ps.x < xClipMin)
                    continue;
                if (ps.x > xClipMax)
                    continue;
                ps.y = 1.0f - ps.y;
                auto pt = ps * sizeScaled + pos;
                nvgLineTo(vg, pt.x, pt.y);
            }
            nvgLineTo(vg, pos.x + pt1.pos.x*sizeScaled.x, pos.y + (1.0f - pt1.pos.y) * sizeScaled.y);
            if (pass == 1) {
                nvgStrokeColor(vg, lineColor);
                nvgStrokeWidth(vg, strokeWidth);
                nvgStroke(vg);
            }
        }
        if (pass == 0) {
            nvgLineTo(vg, pos.x + pts.back().pos.x*sizeScaled.x+5, pos.y + sizeScaled.y);
            nvgClosePath(vg);
            nvgFillColor(vg, fillColor);
            nvgFillCustomPar(vg, -2);
            nvgSetShapeExtents(vg, pos.x, pos.y, sizeScaled.x, sizeScaled.y);
            nvgFill(vg);
        }
    }
    if (hit.type == shape_t::hittype::HIT_EDGE && hit.idx >= 0 && hit.idx < numCurvePts - 1) {
        for (int32_t pass = 0; pass < 2; ++pass) {
            int32_t idx = hit.idx;
            if (pass > 0) {
                if (idx != numCurvePts - 2)
                    break;
                idx = 0;
            }
            const auto& pt0 = pts[idx];
            const auto& pt1 = pts[idx+1];
            auto ptD = pt1.pos - pt0.pos;
            int32_t nVecs = 0;
            int32_t numSteps = math::max<int32_t>(2, math::ceilfS32(ptD.x*sizeScaled.x*0.5f));
            nvgBeginPath(vg);
            for (int32_t step = 0; step < numSteps; step++) {
                float x = step / float(numSteps - 1);
                float ts = curve.shapeSegment(x, pt0.shape);
                dbgassert(ts >= 0.0f && ts <= 1.0f);
                auto ps = vec2(x, ts) * ptD + pt0.pos;
                if (ps.x < xClipMin)
                    continue;
                if (ps.x > xClipMax)
                    continue;
                ps.y = 1.0f - ps.y;
                auto pt = ps * sizeScaled + pos;
                if (nVecs == 0) {
                    nvgMoveTo(vg, pt.x, pt.y);
                } else {
                    nvgLineTo(vg, pt.x, pt.y);
                }
                nVecs++;
            }
            nvgStrokeColor(vg, hoverColor);
            nvgStrokeWidth(vg, strokeWidth+1);
            nvgStroke(vg);
        }
    }

    nvgBeginPath(vg);
    for (int32_t i = 0; i < nPoints; i++) {
        auto pt = vec2(pts[i].pos.x, 1.0 - pts[i].pos.y) * sizeScaled + pos;
        if (hit.type == shape_t::hittype::HIT_NODE && (i == hit.idx)) {
            continue;
        }
        nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, 12);
    }
    nvgFillColor(vg, handleColor);
    nvgFillCustomPar(vg, -2);
    nvgFill(vg);
    if (hit.type == shape_t::hittype::HIT_NODE && hit.idx >= 0 && hit.idx < numCurvePts) {
        auto pt = vec2(pts[hit.idx].pos.x, 1.0 - pts[hit.idx].pos.y) * sizeScaled + pos;
        nvgBeginPath(vg);
        nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, 16);
        nvgFillColor(vg, hoverColor);
        nvgFillCustomPar(vg, -2);
        nvgFill(vg);
    }
}

void DrawShapeUnclamped(const shape_t& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 sizeScaled, const shape_t::hit_result& hit, const std::vector<int32_t>* pSelectedPoints) {
    if (curve.pts.empty())
        return;
    auto numCurvePts = CtrSize(curve.pts);
    //TODO: This is not efficient. Either add caching or at least use references and a scratch pad vector
    const std::vector<shape_pt_t>& pts = curve.pts;
    auto nPoints = CtrSize(curve.pts);
    float radiusHandle = 3.0f;
    float strokeWidth = 3.0f;
    auto lineColor = theme->getColor(col);
    auto fillColor = lineColor;
    auto handleColor = theme->getColor(GuiColor::COL_KNOB_IND);
    auto hoverColor = theme->getColor(colHovered);
    fillColor.a = 0.3;
    for (int32_t pass = 0; pass < 2; ++pass) {
        if (pass == 0) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, pos.x + pts.front().pos.x*sizeScaled.x, pos.y + sizeScaled.y);
        }
        for (int32_t edge = 0; edge < nPoints - 1; edge++) {
            const auto& pt0 = pts[edge];
            const auto& pt1 = pts[edge+1];
            auto ptD = pt1.pos - pt0.pos;
            int32_t numSteps = math::max<int32_t>(0, math::ceilfS32(ptD.x*sizeScaled.x*0.5f));
            if (numSteps == 1)numSteps++;
            if (!numSteps) continue;
            if (pass == 0) {
                nvgLineTo(vg, pos.x + pt0.pos.x*sizeScaled.x, pos.y + (1.0f - pt0.pos.y) * sizeScaled.y);
            }
            if (pass == 1) { // lines
                nvgBeginPath(vg);
                nvgMoveTo(vg, pos.x + pt0.pos.x*sizeScaled.x, pos.y + (1.0f - pt0.pos.y) * sizeScaled.y);
            }
            for (int32_t step = 0; step < numSteps; step++) {
                float x = step / float(numSteps);
                float ts = curve.shapeSegment(x, pt0.shape);
                dbgassert(ts >= 0.0f && ts <= 1.0f);
                auto ps = vec2(x, ts) * ptD + pt0.pos;
                ps.y = 1.0f - ps.y;
                auto pt = ps * sizeScaled + pos;
                nvgLineTo(vg, pt.x, pt.y);
            }
            nvgLineTo(vg, pos.x + pt1.pos.x*sizeScaled.x, pos.y + (1.0f - pt1.pos.y) * sizeScaled.y);
            if (pass == 1) {
                nvgStrokeColor(vg, lineColor);
                nvgStrokeWidth(vg, strokeWidth);
                nvgStroke(vg);
            }
        }
        if (pass == 0) {
            nvgLineTo(vg, pos.x + pts.back().pos.x*sizeScaled.x, pos.y + sizeScaled.y);
            nvgClosePath(vg);
            nvgFillColor(vg, fillColor);
            nvgFillCustomPar(vg, -2);
            nvgSetShapeExtents(vg, pos.x, pos.y, sizeScaled.x, sizeScaled.y);
            nvgFill(vg);
        }
    }
    if (hit.type == shape_t::hittype::HIT_EDGE && hit.idx >= 0 && hit.idx < numCurvePts - 1) {
        for (int32_t pass = 0; pass < 2; ++pass) {
            int32_t idx = hit.idx;
            if (pass > 0) {
                if (idx != numCurvePts - 2)
                    break;
                idx = 0;
            }
            const auto& pt0 = pts[idx];
            const auto& pt1 = pts[idx+1];
            auto ptD = pt1.pos - pt0.pos;
            int32_t nVecs = 0;
            int32_t numSteps = math::max<int32_t>(2, math::ceilfS32(ptD.x*sizeScaled.x*0.5f));
            nvgBeginPath(vg);
            for (int32_t step = 0; step < numSteps; step++) {
                float x = step / float(numSteps - 1);
                float ts = curve.shapeSegment(x, pt0.shape);
                dbgassert(ts >= 0.0f && ts <= 1.0f);
                auto ps = vec2(x, ts) * ptD + pt0.pos;
                ps.y = 1.0f - ps.y;
                auto pt = ps * sizeScaled + pos;
                if (nVecs == 0) {
                    nvgMoveTo(vg, pt.x, pt.y);
                } else {
                    nvgLineTo(vg, pt.x, pt.y);
                }
                nVecs++;
            }
            nvgStrokeColor(vg, hoverColor);
            nvgStrokeWidth(vg, strokeWidth+1);
            nvgStroke(vg);
        }
    }

    nvgBeginPath(vg);
    for (int32_t i = 0; i < nPoints; i++) {
        auto pt = vec2(pts[i].pos.x, 1.0 - pts[i].pos.y) * sizeScaled + pos;
        if (hit.type == shape_t::hittype::HIT_NODE && (i == hit.idx)) {
            continue;
        }
        nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, 12);
    }
    nvgFillColor(vg, handleColor);
    nvgFillCustomPar(vg, -2);
    nvgFill(vg);
    if (hit.type == shape_t::hittype::HIT_NODE && hit.idx >= 0 && hit.idx < numCurvePts) {
        auto pt = vec2(pts[hit.idx].pos.x, 1.0 - pts[hit.idx].pos.y) * sizeScaled + pos;
        nvgBeginPath(vg);
        nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, 16);
        nvgFillColor(vg, hoverColor);
        nvgFillCustomPar(vg, -2);
        nvgFill(vg);
    }
    for (auto idx : *pSelectedPoints) {
        if (idx < 0 || idx >= numCurvePts) continue;
        auto pt = vec2(pts[idx].pos.x, 1.0 - pts[idx].pos.y) * sizeScaled + pos;
        nvgBeginPath(vg);
        nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, 16);
        nvgFillColor(vg, hoverColor);
        nvgFillCustomPar(vg, -2);
        nvgFill(vg);
    }
}
void DrawShapeCyclic(const shape_t& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 size, const shape_t::hit_result& hit) {
    if (curve.pts.empty())
        return;

    auto numCurvePts = CtrSize(curve.pts);

    //TODO: This is not efficient. Either add caching or at least use references and a scratch pad vector
    std::vector<shape_pt_t> pts;
    auto ptFront = curve.pts.back();
    ptFront.pos.x -= 1;
    auto ptBack = curve.pts.front();
    ptBack.pos.x += 1;
    pts.push_back(ptFront);
    pts.insert(pts.end(), curve.pts.begin(), curve.pts.end());
    pts.push_back(ptBack);
    auto nPoints = CtrSize(pts);
    float radiusHandle = 3.0f;
    float strokeWidth = 3.0f;
    auto lineColor = theme->getColor(col);
    auto fillColor = lineColor;
    auto handleColor = theme->getColor(GuiColor::COL_KNOB_IND);
    auto hoverColor = theme->getColor(colHovered);
    fillColor.a = 0.3;
    for (int32_t pass = 0; pass < 2; ++pass) {
        if (pass == 0) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, pos.x + pts.front().pos.x*size.x-5, pos.y + size.y);
        }
        for (int32_t edge = 0; edge < nPoints - 1; edge++) {
            const auto& pt0 = pts[edge];
            const auto& pt1 = pts[edge+1];
            const auto ptD = pt1.pos - pt0.pos;
            int32_t numSteps = math::max<int32_t>(0, math::ceilfS32(ptD.x*size.x*0.5f));
            if (numSteps == 1)numSteps++;
            if (!numSteps) continue;
            if (pass == 0) {
                nvgLineTo(vg, pos.x + pt0.pos.x*size.x, pos.y + (1.0f - pt0.pos.y) * size.y);
            }
            if (pass == 1) { // lines
                nvgBeginPath(vg);
                nvgMoveTo(vg, pos.x + pt0.pos.x*size.x, pos.y + (1.0f - pt0.pos.y) * size.y);
            }
            for (int32_t step = 0; step < numSteps; step++) {
                float x = step / float(numSteps);
                float ts = curve.shapeSegment(x, pt0.shape);
                dbgassert(ts >= 0.0f && ts <= 1.0f);
                auto ps = vec2(x, ts) * ptD + pt0.pos;
                if (ps.x < -0.1f)
                    continue;
                if (ps.x > 1.1f)
                    continue;
                ps.y = 1.0f - ps.y;
                auto pt = ps * size + pos;
                nvgLineTo(vg, pt.x, pt.y);
            }
            nvgLineTo(vg, pos.x + pt1.pos.x*size.x, pos.y + (1.0f - pt1.pos.y) * size.y);
            if (pass == 1) {
                nvgStrokeColor(vg, lineColor);
                nvgStrokeWidth(vg, strokeWidth);
                nvgStroke(vg);
            }
        }
        if (pass == 0) {
            nvgLineTo(vg, pos.x + pts.back().pos.x*size.x+5, pos.y + size.y);
            nvgClosePath(vg);
            nvgFillColor(vg, fillColor);
            nvgFillCustomPar(vg, -2);
            nvgSetShapeExtents(vg, pos.x, pos.y, size.x, size.y);
            nvgFill(vg);
        }
    }
    if (hit.type == shape_t::hittype::HIT_EDGE && hit.idx >= 0 && hit.idx < numCurvePts) {
        for (int32_t pass = 0; pass < 2; ++pass) {
            int32_t idx = hit.idx;
            if (pass > 0) {
                if (idx != numCurvePts - 1)
                    break;
                idx = -1;
            }
            const auto& pt0 = pts[idx + 1];
            const auto& pt1 = pts[idx + 2];
            auto ptD = pt1.pos - pt0.pos;
            int32_t nVecs = 0;
            int32_t numSteps = math::max<int32_t>(2, math::ceilfS32(ptD.x*size.x*0.5f));
            nvgBeginPath(vg);
            for (int32_t step = 0; step < numSteps; step++) {
                float x = step / float(numSteps - 1);
                float ts = curve.shapeSegment(x, pt0.shape);
                dbgassert(ts >= 0.0f && ts <= 1.0f);
                auto ps = vec2(x, ts) * ptD + pt0.pos;
                if (ps.x < -0.1f)
                    continue;
                if (ps.x > 1.1f)
                    continue;
                ps.y = 1.0f - ps.y;
                auto pt = ps * size + pos;
                if (nVecs == 0) {
                    nvgMoveTo(vg, pt.x, pt.y);
                } else {
                    nvgLineTo(vg, pt.x, pt.y);
                }
                nVecs++;
            }
            nvgStrokeColor(vg, hoverColor);
            nvgStrokeWidth(vg, strokeWidth+1);
            nvgStroke(vg);
        }
    }

    nvgBeginPath(vg);
    for (int32_t i = 0; i < nPoints; i++) {
        auto pt = vec2(pts[i].pos.x, 1.0 - pts[i].pos.y) * size + pos;
        if (hit.type == shape_t::hittype::HIT_NODE && (i == hit.idx+1 || (hit.idx == 0 && i == nPoints-1))) {
            continue;
        }
        nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, 12);
    }
    nvgFillColor(vg, handleColor);
    nvgFillCustomPar(vg, -2);
    nvgFill(vg);
    if (hit.type == shape_t::hittype::HIT_NODE && hit.idx >= 0 && hit.idx < numCurvePts) {
        auto pt = vec2(pts[hit.idx+1].pos.x, 1.0 - pts[hit.idx+1].pos.y) * size + pos;
        nvgBeginPath(vg);
        nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, 16);
        if (hit.idx == 0) {
            auto pt2 = vec2(pts.back().pos.x, 1.0 - pts.back().pos.y) * size + pos;
            nvgCircleFastNDivs(vg, pt2.x, pt2.y, radiusHandle, 16);
        }
        nvgFillColor(vg, hoverColor);
        nvgFillCustomPar(vg, -2);
        nvgFill(vg);
    }
}
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
        paint.customPar  = 1;
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
            paint.customPar = 2;
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
            paint.customPar = 3;
            nvgFillPaint(vg, paint);
            nvgBatchedRender(vg);
        }
    }
}
class guictr_curve_shape : public guictr_base, public ShapeEdit {
    friend class guictr_curve_editor;
    shape_t curveInternal;
public:
    guictr_curve_shape()
    {
        bIsGridEnabledH = true;
        bIsGridEnabledV = true;
        curveInternal.pts.push_back({ { 0, 0 }, 0.5f });
        setEditorCurve(&curveInternal);
        padding = 4;
        margin = 4;
        setBackgroundRendered(true);
        setCanMouseHit(true);
    }
    GuiColor::constant_t getOuterBackgroundColorFromState(int32_t stateflags) const override {
        return GuiColor::COL_BG_DRKER2;
    }
    void render(NVGcontext* vg) override {
        auto cs = getSizeContent();
        drawBackground(vg, theme, getPosContent(), cs, margin, isBackgroundRenderedInset());

        if (!setScissorTransform(vg)) {
            return;
        }
        auto relMousePos = toControlsObjectSpace(parentCtrl->m_mousePos, this);
        renderEditor(vg, {0, 0}, theme, relMousePos, true);
    }
    void layout() override {
        auto cs = getSizeContent();
        layoutEditor(cs);
        guictr_base::layout();
    }

    void handleRightClick(MouseEvent& evt) override {
        if (onRightClickCurveEditor(evt)) {
            return;
        }
        guictr_base::handleRightClick(evt);
    }

    void handleDraggedBegin(MouseEvent& evt) override {
        onBeginDragCurveEditor(evt);
    }

    void handleDraggedMove(MouseEvent& evt) override {
        onMoveDragCurveEditor(evt);
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        onReleaseDragCurveEditor(evt);
    }
};
class guictr_curve_grid_control : public guictr_base {
    friend class guictr_curve_controls;
    friend class guictr_curve_editor;
    const int32_t axis;
    guibuttonstate buttonGrid;
    gui_numberinput_i32 inputGridSteps;
    const String axes[2] = { "X", "Y" };
public:
    guictr_curve_grid_control(int32_t axis)
        : axis(axis), inputGridSteps(nullptr) {
        add(&buttonGrid);
        add(&inputGridSteps);
        padding = 2;
        margin = 4;
        setCanMouseHit(true);
        setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        buttonGrid.setText(axes[axis] + " Grid: " + String(buttonGrid.getState() ? "On" : "Off"));
        inputGridSteps.setLabel("Steps");
        inputGridSteps.fnClamp = [](int32_t value) -> int32_t {
            return math::clamp<int32_t>(value, 1, 16);
        };
    }
    ~guictr_curve_grid_control() {
        removeGuis();
    }
    void buttonClicked(guibase* button) override {
        if (&buttonGrid == button) {
            buttonGrid.setText(axes[axis] + " Grid: " + String(buttonGrid.getState() ? "On" : "Off"));
        }
        guictr_base::buttonClicked(button);
    }
};
class guictr_curve_controls : public guictr_base {
    friend class guictr_curve_editor;
    guictr_curve_grid_control gridControlH;
    guictr_curve_grid_control gridControlV;
    guidropdown_select_preset selectPreset;
    guibutton buttonSave;

public:
    guictr_curve_controls()
        : gridControlH(0),
        gridControlV(1)
    {
        padding = 2;
        margin = 4;
        setCanMouseHit(true);
        setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        buttonSave.drawFn   = drawTextureSymbol;
        buttonSave.drawParm = ICON_SAVE;
        buttonSave.setText("Save");
        buttonSave.setFlag(FLG_NO_LAYOUT, true);
        add(&selectPreset);
        add(&buttonSave);
        add(&gridControlH);
        add(&gridControlV);
    }
    ~guictr_curve_controls() {
        removeGuis();
    }
    void layout() override {
        guictr_base::layout();
        buttonSave.size = ivec2{selectPreset.size.y*3, selectPreset.size.y};
        selectPreset.size.x = selectPreset.size.x - buttonSave.size.x - padding;
        buttonSave.pos = ivec2(selectPreset.right(), 0);
        buttonSave.size -= ivec2(4);
        buttonSave.pos += ivec2(2);
        selectPreset.size -= ivec2(4);
        selectPreset.pos += ivec2(2);
        selectPreset.layout();
        buttonSave.layout();
    }
};
class guictr_curve_editor : public guictr_base, public i_ctr_shape_editor {
    seq_rand rand;
    guictr_curve_shape shape;
    guictr_curve_controls controls;
    PresetManager presetManager;
    int32_t inputHeight = HEIGHT_DEFAULT_INPUT;
public:
    guictr_curve_editor() : guictr_base(){
        setGuiType(gui_type::CTR_TYPE_SHAPE_EDITOR);
        presetManager.setFileExtension("shape");
        presetManager.load(App::Platform::toUserdataPath("presets/Shapes"));
        add(&controls);
        add(&shape);
        controls.gridControlH.inputGridSteps.setRef(&this->shape.gridStepsH);
        controls.gridControlV.inputGridSteps.setRef(&this->shape.gridStepsV);
        controls.gridControlH.buttonGrid.setStateRef(&this->shape.bIsGridEnabledH);
        controls.gridControlV.buttonGrid.setStateRef(&this->shape.bIsGridEnabledV);
        controls.selectPreset.setPresetManager(presetManager);
        controls.selectPreset.setCallback([this](const String& path) {
            shape_preset_t shapeLoaded{};
            if (loadShapePresetFile(path, shapeLoaded)) {
                if (shapeLoaded.version) {
                    auto& presetShape = shapeLoaded.curve;
                    shape_t tmp{presetShape.flags, std::move(presetShape.pts), presetShape.name, 0.0f };
                    // tmp.sort();
                    controls.selectPreset.setString(tmp.name);
                    *shape.curve = tmp;
                    if (shape.callback)
                        shape.callback(*shape.curve, false);
                }
            }
        });
        controls.selectPreset.setString("Empty");
        setBackgroundRendered(true);
        rand.rng_seed(static_cast<uint64_t>(getTimeMicros()));
        setCanMouseHit(true);
        padding = 0;
        margin = 4;
    }
    ~guictr_curve_editor() override {
        removeGuis();
    }
    void setShapeEditorCallback(std::function<void(const DAW::Shape::shape_t&, bool)> callback) override {
        shape.callback = std::move(callback);
    }
    void setShapeEditorShapeRef(DAW::Shape::shape_t* shape) override {
        this->shape.curve = shape ? shape : &this->shape.curveInternal;
    }
    guictr_base* getGuiContainer() override {
        return this;
    }
    void setInputHeight(int32_t height) {
        inputHeight = height;
    }

    void layout() override {
        auto cs = getSizeContent();
        shape.pos = controls.pos = {0,0};
        shape.size = controls.size = cs;
        auto controlsHeight = math::clamp<int32_t>(size.y/8, 12, inputHeight);
        if (controlsHeight < inputHeight*2/3) {
            controls.size.y = 0;
            controls.setVisible(false);
        } else {
            controls.size.y = controlsHeight;
            controls.setVisible(true);
        }
        shape.pos.y = controls.bottom() + padding;
        shape.size.y = cs.y - controls.size.y;
        guictr_base::layout();
    }

    void buttonClicked(guibase* button) override {
        if (&controls.gridControlH.buttonGrid == button) {
            shape.bIsGridEnabledH = !shape.bIsGridEnabledH;
            return;
        }
        if (&controls.gridControlV.buttonGrid == button) {
            shape.bIsGridEnabledV = !shape.bIsGridEnabledV;
            return;
        }
        if (&controls.buttonSave == button) {
            shape_preset_t shapePreset { 1, *shape.curve };
            String defaultPresetPath = presetManager.getPresetPath();
            CreateDirectoryIfNotExists(defaultPresetPath);
            String path;
            auto window = parentCtrl->window;
            if (promptUserFilePath(window, 1, vFILE_TYPE_SHAPEPRESET, path, defaultPresetPath)) {
                String ext;
                String name;
                SplitPath(path, nullptr, &name, &ext);
                if (ext.empty()) {
                    path += "." + vFILE_TYPE_SHAPEPRESET[0].ext;
                }
                shapePreset.curve.name = name;
                controls.selectPreset.setString(name);
                saveShapePresetFile(shapePreset, path);
            }
            return;
        }
        guictr_base::buttonClicked(button);
    }
};

bool ShapeEdit::onBeginDragCurveEditor(MouseEvent& evt) {
    dragged = {};
    curveBegin = *curve;
    curveTmp   = *curve;
    vec2 local = toNormalizedSpace(evt.relMousepos);
    bool hasBegin = false;
    if (evt.type == MouseEventType::M_EVT_DOUBLECLICK&&!(curve->flags & SHAPE_LOCK_POINTS)) {
        int64_t idx = -1;
        int64_t len = int64_t(curveTmp.pts.size());
        for (int64_t i = 0; i < len; ++i) {
            if (curveTmp.pts[i].pos.x>local.x) {
                idx = i;
                break;
            }
        }
        if (idx == -1) {
            curveTmp.pts.push_back({ { local.x, local.y }, 0.5f });
        } else {
            curveTmp.pts.insert(curveTmp.pts.begin() + idx, { { local.x, local.y }, 0.5f });
        }
        if (callback)
            callback(curveTmp, false);
        hasBegin = true;
    } else if (hasControlHandles()) {
        dragged      = curve->getMouseHit(local, editorScale);
        dragBeginPos = local;
        hasBegin     = dragged.type != shape_t::hittype::HIT_NONE;
    }
    wasAltBegin   = isAlt(evt.kbmods);
    wasShiftBegin = isShift(evt.kbmods);
    return hasBegin;
}

bool ShapeEdit::mouseHitCurveEditor(const shape_t& shape, ivec2 mpos) const {
    if (hasControlHandles()) {
        vec2 local = toNormalizedSpace(mpos);
        auto hit = shape.getMouseHit(local, editorScale);
        return hit.type != shape_t::hittype::HIT_NONE;
    }
    return false;
}

void ShapeEdit::onMoveDragCurveEditor(MouseEvent& evt) {
    if (hasControlHandles()) {
        vec2 local = toNormalizedSpace(evt.relMousepos);
        if (wasShiftBegin&&!(curve->flags & SHAPE_LOCK_POINTS)) {
            curveTmp = curveBegin;
            if (bIsGridEnabledH) {
                local.x = math::floorfS32(local.x * this->gridStepsH) / float(this->gridStepsH);
            }
            if (bIsGridEnabledV && !isAlt(evt.kbmods)) {
                local.y = snapV(local.y);
            }
            if (!(curve->flags&ShapeFlags::SHAPE_UNCLAMPPED)) {
                local.x = math::clamp(local.x, 0.0f, 1.0f);
                local.y = math::clamp(local.y, 0.0f, 1.0f);
            }
            float beginRange = local.x;
            float endRange   = beginRange + 1.0f / math::max<float>(1.0f, gridStepsH);
            float leftVal    = curveTmp.sampleCurve(beginRange, false);
            float rightVal   = curveTmp.sampleCurve(endRange, true);
            erase_if(curveTmp.pts, [beginRange, endRange](const auto& pt) {
                return pt.pos.x >= beginRange && pt.pos.x <= endRange;
            });
            size_t idxInsert = curveTmp.pts.size();
            for (size_t i = 0; i < curveTmp.pts.size(); i++) {
                if (curveTmp.pts[i].pos.x >= beginRange) {
                    idxInsert = i;
                    break;
                }
            }
            if ((curve->flags&ShapeFlags::SHAPE_UNCLAMPPED) || ((beginRange >= 0.0f && beginRange <= 1.0f && endRange >= 0.0f && endRange <= 1.0f))) {
                curveTmp.pts.insert(curveTmp.pts.begin() + idxInsert++, { { beginRange, leftVal }, 0.5f });
                curveTmp.pts.insert(curveTmp.pts.begin() + idxInsert++, { { beginRange, local.y }, 0.5f });
                curveTmp.pts.insert(curveTmp.pts.begin() + idxInsert++, { { endRange, local.y }, 0.5f });
                curveTmp.pts.insert(curveTmp.pts.begin() + idxInsert++, { { endRange, rightVal }, 0.5f });
                // curveTmp.sort();
            }
            if (callback)
                callback(curveTmp, true);
            return;
        }
        if (dragged.type == shape_t::hittype::HIT_NODE && dragged.idx < CtrSize(curveBegin.pts)) {
            curveTmp                     = curveBegin;
            const bool canJumpOverPoints = false;
            auto snapped                 = local;
            if (bIsGridEnabledH && !isAlt(evt.kbmods)) {
                snapped.x = snapH(local.x);
            }
            if (bIsGridEnabledV && !isAlt(evt.kbmods)) {
                snapped.y = snapV(local.y);
            }
            if (!(curve->flags&ShapeFlags::SHAPE_UNCLAMPPED)) {
                if (!canJumpOverPoints) {
                    if (snapped.x > 1.0f) {
                        snapped.x = 1.0f;
                    }
                    if (snapped.x < 0.0f) {
                        snapped.x = 0.0f;
                    }
                    if (dragged.idx > 0) {
                        snapped.x = math::max(snapped.x, curveTmp.pts[dragged.idx - 1].pos.x);
                    }
                    if (dragged.idx + 1 < CtrSize(curveTmp.pts)) {
                        snapped.x = math::min(snapped.x, curveTmp.pts[dragged.idx + 1].pos.x);
                    }
                }
                while (snapped.x < 0.0f) {
                    snapped.x += 1.0f;
                }
                auto unclamped = snapped;
                double fModfInput = snapped.x;
                snapped.x      = math::clamp<float>(modf(fModfInput, &fModfInput), 0.0f, 1.0f);
                snapped.y      = math::clamp(snapped.y, 0.0f, 1.0f);
                auto& pt       = curveTmp.pts[dragged.idx];
                if (unclamped.x > snapped.x + 0.9) {
                    snapped.x += 1.0;
                    dbgassert(snapped.x == 1.0);
                }
                pt.pos = snapped;
            } else {
                if (!canJumpOverPoints) {
                    if (dragged.idx > 0) {
                        snapped.x = math::max(snapped.x, curveTmp.pts[dragged.idx - 1].pos.x);
                    }
                    if (dragged.idx + 1 < CtrSize(curveTmp.pts)) {
                        snapped.x = math::min(snapped.x, curveTmp.pts[dragged.idx + 1].pos.x);
                    }
                }
                snapped.y      = math::clamp(snapped.y, 0.0f, 1.0f);
                snapped.x      = math::max(snapped.x, 0.0f);
                auto& pt       = curveTmp.pts[dragged.idx];
                pt.pos = snapped;
            }

            // curveTmp.sort();
            if (callback)
                callback(curveTmp, true);
            if (curve && curve->flags & SHAPE_LOCK_POINTS) {
                *curve = curveTmp;
            }
            return;
        }
        if (dragged.type == shape_t::hittype::HIT_EDGE && dragged.idx < CtrSize(curveBegin.pts)) {
            curveTmp          = curveBegin;
            auto& pt          = curveTmp.pts[dragged.idx];
            auto& ptNext      = curveTmp.getPointAfterIdx(dragged.idx);
            float fDist       = (local - dragBeginPos).y;
            *evt.dragDistance = ivec2(0);
            if (wasAltBegin) {
                if (curveTmp.flags & SHAPE_SHAPED) {
                    float fSign = ptNext.pos.y > pt.pos.y ? 1.0f : -1.0f;
                    pt.shape    = math::clamp(pt.shape - fSign * fDist, 0.0f, 1.0f);
                }
            } else {
                float fd = (ptNext.pos.y - pt.pos.y);
                float fy = (ptNext.pos.y + pt.pos.y) * 0.5f;
                if (bIsGridEnabledV && !isAlt(evt.kbmods)) {
                    local.y = snapV(local.y);
                    fDist   = local.y - fy;
                    if (math::abs(fDist) < 1.0f / this->gridStepsV) {
                        fDist = 0.0f;
                    }
                }
                pt.pos.y = math::clamp(fy-fd*0.5f+fDist, 0.0f, 1.0f);
                if (&ptNext != &pt) {
                    ptNext.pos.y = math::clamp(fy+fd*0.5f+fDist, 0.0f, 1.0f);
                }
            }
            if (callback)
                callback(curveTmp, true);
            if (curve && curve->flags & SHAPE_LOCK_POINTS) {
                *curve = curveTmp;
            }
        }
    }
}

void ShapeEdit::onReleaseDragCurveEditor(MouseEvent& evt) {
    dragged = {};
    if (hasControlHandles()) {
        if (!(curve->flags & SHAPE_LOCK_POINTS)) {
            curveTmp.eraseDuplicates();
        }
        if (callback) {
            callback(curveTmp, false);
        } else if (curve) {
            *curve = curveTmp;
        }
    }
}

bool ShapeEdit::onRightClickCurveEditor(MouseEvent& evt) {
    if (hasControlHandles() && curve && !(curve->flags & SHAPE_LOCK_POINTS)) {
        vec2 local    = toNormalizedSpace(evt.relMousepos);
        float minDist = 0.0f;
        int32_t minPt = curve->getMinPt(local, editorScale, &minDist);
        if (minPt > -1) {
            if (callback) {
                curveTmp = *curve;
                curveTmp.pts.erase(curveTmp.pts.begin() + minPt);
                // curveTmp.sort();
                callback(curveTmp, false);
            } else if (curve) {
                curve->pts.erase(curve->pts.begin() + minPt);
                curve->sort();
            }
            return true;
        }
    }
    return false;
}

void ShapeEdit::renderEditor(NVGcontext* vg, vec2 pos, const guitheme_t* theme, ivec2 relMousepos, bool bDrawGrid, const std::vector<int32_t>* pSelectedPoints) {
    // if (editorScale.x < 1.0f || editorScale.y < 1.0f)
    //     return;
    if (!assert_expr(curve != nullptr))
        return;
    nvgSave(vg);
    nvgTranslate(vg, pos.x, pos.y);
    pos = vec2(0.0f);
    if (bDrawGrid) {
        int32_t gridStepsH = math::clamp<int32_t>(this->gridStepsH, 1, 128);
        int32_t gridStepsV = math::clamp<int32_t>(this->gridStepsV, 1, 128);
        DrawGrid(vg, theme, pos, editorScale, gridStepsH, gridStepsV);
    }
    const auto mouseLocal = toNormalizedSpace(relMousepos);
    auto higlightHit = shape_t::hit_result();
    if (((curve->flags&ShapeFlags::SHAPE_UNCLAMPPED) || (mouseLocal.x >= 0.0f && mouseLocal.x <= 1.0f)) && mouseLocal.y >= 0.0f && mouseLocal.y <= 1.0f) {
        higlightHit = curve->getMouseHit(mouseLocal, editorScale);
    }
    auto* curveRender     = curve;
    if (dragged.type != shape_t::hittype::HIT_NONE) {
        higlightHit = dragged;
        curveRender = &curveTmp;
    }
    if (curveRender->flags & ShapeFlags::SHAPE_UNCLAMPPED) {
        DrawShapeUnclamped(*curveRender, vg, theme, GuiColor::COL_SHAPE_CURVE, GuiColor::COL_SHAPE_CURVE_HIGHLIGHT, pos, editorScale, higlightHit, pSelectedPoints);
    } else if (curveRender->flags & ShapeFlags::SHAPE_CYCLIC) {
        DrawShapeCyclic(*curveRender, vg, theme, GuiColor::COL_SHAPE_CURVE, GuiColor::COL_SHAPE_CURVE_HIGHLIGHT, pos, editorScale, higlightHit);
    } else {
        DrawShapeOneShot(*curveRender, vg, theme, GuiColor::COL_SHAPE_CURVE, GuiColor::COL_SHAPE_CURVE_HIGHLIGHT, pos, editorScale, -0.1f, 1.1f, higlightHit);
    }

    if (curveRender->renderPhase > -1.0f) {
        float playBackX = curveRender->renderPhase * editorScale.x;
        nvgBeginPath(vg);
        nvgMoveTo(vg, playBackX, 0);
        nvgLineTo(vg, playBackX, editorScale.y);
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PLAYHEAD_OUTLINE));
        nvgStrokeWidth(vg, 2);
        nvgStroke(vg);
        nvgBeginPath(vg);
        nvgMoveTo(vg, playBackX, 0);
        nvgLineTo(vg, playBackX, editorScale.y);
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PLAYHEAD));
        nvgStrokeWidth(vg, 1);
        nvgStroke(vg);
    }
    nvgRestore(vg);
}

void ShapeEdit::layoutEditor(ivec2 size) {
    editorScale = {math::max(4, size.x), size.y};
}

void ShapeEdit::setEditorCurve(shape_t* curve) {
    this->curve      = curve;
    this->curveBegin = *curve;
}

bool ShapeEdit::hasControlHandles() const {
    return !curve->pts.empty();
}

}// namespace DAW::Shape


i_ctr_shape_editor* makeShapeEditor() {
    return new DAW::Shape::guictr_curve_editor();
}
