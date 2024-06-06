#pragma once
#include "guicolors.h"
#include "host/shape/shape.h"
#include "theme.h"
#include <nanovg_min.h>

namespace DAW::Shape {

void DrawShapeCyclic(const shape_t& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 size, const shape_t::hit_result& hit);
void DrawShapeOneShot(const shape_t& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 sizeScaled, float xClipMin, float xClipMax, const shape_t::hit_result& hit);
void DrawShapeUnclamped(const shape_t& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 sizeScaled, const shape_t::hit_result& hit, const std::vector<int32_t>* pSelectedPoints);
void DrawGrid(NVGcontext* vg, const guitheme_t* theme, vec2 pos, vec2 size, int gridStepsH, int gridStepsV);

struct RenderShape {
    shape_t* curveRender;
    guitheme_t* theme;
    GuiColor::constant_t col;
    GuiColor::constant_t& colHovered;
    void RenderShapeView(NVGcontext* vg, shape_t* curveRender, vec2 viewPos, vec2 viewSize);
};

} // namespace DAW::Shape
