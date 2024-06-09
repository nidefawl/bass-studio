#pragma once
#include "guicolors.h"
#include "host/shape/shape.h"
#include "theme.h"
#include <nanovg.h>
#include <nanovg_min.h>

namespace DAW::Shape {

void DrawGrid(NVGcontext* vg, const guitheme_t* theme, vec2 pos, vec2 size, int gridStepsH, int gridStepsV);

template<typename ShapeType>
void DrawShapeOneShot(const ShapeType& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 sizeScaled, float xClipMin, float xClipMax, const shape_t::hit_result& hit) {
    if (curve.pts.empty())
        return;
    const auto numCurvePts = CtrSize(curve.pts);
    //TODO: This is not efficient. Either add caching or at least use references and a scratch pad vector
    const auto& pts = curve.pts;
    auto nPoints = CtrSize(curve.pts);
    float radiusHandle = 3.0f;
    float strokeWidth = 3.0f;
    auto lineColor = theme->getColor(col);
    auto fillColor = lineColor;
    auto handleColor = theme->getColor(GuiColor::COL_KNOB_IND);
    auto hoverColor = theme->getColor(colHovered);
    fillColor.a = 0.3f;
    for (int32_t pass = 0; !(curve.flags&SHAPE_SHOW_ONLY_CONTROL_POINTS) && pass < 2; ++pass) {
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
                float ts = curve.shapeSegmentPt(x, pt0);
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

    if (curve.flags & SHAPE_LOCK_POINTS) {
        return;
    }

    if (!(curve.flags&SHAPE_SHOW_ONLY_CONTROL_POINTS) && hit.type == shape_t::hittype::HIT_EDGE && hit.idx >= 0 && hit.idx < numCurvePts - 1) {
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
                float ts = curve.shapeSegmentPt(x, pt0);
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

template<typename ShapeType>
void DrawShapeUnclamped(const ShapeType& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 sizeScaled, const shape_t::hit_result& hit, const std::vector<int32_t>* pSelectedPoints) {
    if (curve.pts.empty())
        return;
    const auto numCurvePts = CtrSize(curve.pts);
    //TODO: This is not efficient. Either add caching or at least use references and a scratch pad vector
    const auto& pts = curve.pts;
    const auto nPoints = CtrSize(curve.pts);
    float radiusHandle = 3.0f;
    float strokeWidth = 3.0f;
    auto lineColor = theme->getColor(col);
    auto fillColor = lineColor;
    auto handleColor = theme->getColor(GuiColor::COL_KNOB_IND);
    auto hoverColor = theme->getColor(colHovered);
    fillColor.a = 0.3f;
    for (int32_t pass = 0; !(curve.flags&SHAPE_SHOW_ONLY_CONTROL_POINTS) && pass < 2; ++pass) {
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
                float ts = curve.shapeSegmentPt(x, pt0);
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
    if (!(curve.flags&SHAPE_SHOW_ONLY_CONTROL_POINTS) && hit.type == shape_t::hittype::HIT_EDGE && hit.idx >= 0 && hit.idx < numCurvePts - 1) {
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
                float ts = curve.shapeSegmentPt(x, pt0);
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
    if (curve.flags & SHAPE_LOCK_POINTS) {
        return;
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
    if (!pSelectedPoints)
        return;
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

template<typename ShapeType>
void DrawShapeCyclic(const ShapeType& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 size, const shape_t::hit_result& hit) {
    if (curve.pts.empty())
        return;

    const auto numCurvePts = CtrSize(curve.pts);
    const auto nPoints = CtrSize(curve.pts) + 2;
    auto getPoint = [&curve,nPoints](int32_t idx) {
        auto tmp = curve.pts.front();
        if (idx == 0 || idx == -1) {
            tmp = curve.pts.back();
            tmp.pos.x -= 1;
        } else if (idx == nPoints - 1) {
            tmp = curve.pts.front();
            tmp.pos.x += 1;
        } else {
            tmp = curve.pts[idx - 1];
        }
        return tmp;
    };
    const float radiusHandle = 3.0f;
    const float strokeWidth = 3.0f;
    const auto lineColor = theme->getColor(col);
    auto fillColor = lineColor;
    const auto handleColor = theme->getColor(GuiColor::COL_KNOB_IND);
    const auto hoverColor = theme->getColor(colHovered);
    fillColor.a = 0.3f;
    for (int32_t pass = 0; !(curve.flags&SHAPE_SHOW_ONLY_CONTROL_POINTS) && pass < 2; ++pass) {
        if (pass == 0) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, pos.x + getPoint(0).pos.x * size.x-5, pos.y + size.y);
        }
        for (int32_t edge = 0; edge < nPoints - 1; edge++) {
            const auto& pt0 = getPoint(edge);
            const auto& pt1 = getPoint(edge+1);
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
                float ts = curve.shapeSegmentPt(x, pt0);
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
            nvgLineTo(vg, pos.x + getPoint(nPoints-1).pos.x*size.x+5, pos.y + size.y);
            nvgClosePath(vg);
            nvgFillColor(vg, fillColor);
            nvgFillCustomPar(vg, -2);
            nvgSetShapeExtents(vg, pos.x, pos.y, size.x, size.y);
            nvgFill(vg);
        }
    }
    if (curve.flags & SHAPE_LOCK_POINTS) {
        return;
    }
    if (!(curve.flags&SHAPE_SHOW_ONLY_CONTROL_POINTS) && hit.type == shape_t::hittype::HIT_EDGE && hit.idx >= 0 && hit.idx < numCurvePts) {
        for (int32_t pass = 0; pass < 2; ++pass) {
            int32_t idx = hit.idx;
            if (pass > 0) {
                if (idx != numCurvePts - 1)
                    break;
                idx = -1;
            }
            const auto& pt0 = getPoint(idx + 1);
            const auto& pt1 = getPoint(idx + 2);
            auto ptD = pt1.pos - pt0.pos;
            int32_t nVecs = 0;
            int32_t numSteps = math::max<int32_t>(2, math::ceilfS32(ptD.x*size.x*0.5f));
            nvgBeginPath(vg);
            for (int32_t step = 0; step < numSteps; step++) {
                float x = step / float(numSteps - 1);
                float ts = curve.shapeSegmentPt(x, pt0);
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
        auto pt = vec2(getPoint(i).pos.x, 1.0 - getPoint(i).pos.y) * size + pos;
        if (hit.type == shape_t::hittype::HIT_NODE && (i == hit.idx+1 || (hit.idx == 0 && i == nPoints-1))) {
            continue;
        }
        nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, 12);
    }
    nvgFillColor(vg, handleColor);
    nvgFillCustomPar(vg, -2);
    nvgFill(vg);
    if (hit.type == shape_t::hittype::HIT_NODE && hit.idx >= 0 && hit.idx < numCurvePts) {
        auto pt = vec2(getPoint(hit.idx+1).pos.x, 1.0 - getPoint(hit.idx+1).pos.y) * size + pos;
        nvgBeginPath(vg);
        nvgCircleFastNDivs(vg, pt.x, pt.y, radiusHandle, 16);
        if (hit.idx == 0) {
            auto pt2 = vec2(getPoint(nPoints-1).pos.x, 1.0 - getPoint(nPoints-1).pos.y) * size + pos;
            nvgCircleFastNDivs(vg, pt2.x, pt2.y, radiusHandle, 16);
        }
        nvgFillColor(vg, hoverColor);
        nvgFillCustomPar(vg, -2);
        nvgFill(vg);
    }
}

template<typename ShapeType>
struct RenderShape {
    GuiColor::constant_t col{GuiColor::COL_SHAPE_CURVE};
    GuiColor::constant_t colHovered{GuiColor::COL_SHAPE_CURVE_HIGHLIGHT};
    shape_t::hit_result hit{};
    void renderShapeView(NVGcontext* vg, guitheme_t* theme, ShapeType* curveRender, vec2 viewPos, vec2 viewSize) {
        if (!assert_expr(curveRender != nullptr))
            return;
        nvgSave(vg);
        nvgTranslate(vg, viewPos.x, viewPos.y);
        viewPos = vec2(0.0f);
        if (curveRender->flags & ShapeFlags::SHAPE_UNCLAMPPED) {
            DrawShapeUnclamped(*curveRender, vg, theme, col, colHovered, viewPos, viewSize, hit, nullptr);
        } else if (curveRender->flags & ShapeFlags::SHAPE_CYCLIC) {
            DrawShapeCyclic(*curveRender, vg, theme, col, colHovered, viewPos, viewSize, hit);
        } else {
            DrawShapeOneShot(*curveRender, vg, theme, col, colHovered, viewPos, viewSize, -0.1f, 1.1f, hit);
        }
        nvgRestore(vg);
    }
};

} // namespace DAW::Shape
