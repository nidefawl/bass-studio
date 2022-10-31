#pragma once
#include <functional>
#include "shape.h"
#include "gui/container/container.h"

class i_ctr_shape_editor {
protected:
    ~i_ctr_shape_editor() = default;

public:
    i_ctr_shape_editor() = default;

public:
    virtual void setShapeEditorCallback(std::function<void(const DAW::Shape::shape_t&, bool bIsDragMove)> callback) = 0;
    virtual void setShapeEditorShapeRef(DAW::Shape::shape_t* shape) = 0;
    virtual guictr_base* getGuiContainer() = 0;
};

i_ctr_shape_editor* makeShapeEditor();

namespace DAW::Shape {
    void DrawShapeCyclic(const shape_t& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 size, const shape_t::hit_result& hit);
    void DrawShapeOneShot(const shape_t& curve, NVGcontext*vg, const guitheme_t* theme, const GuiColor::constant_t& col, const GuiColor::constant_t& colHovered, vec2 pos, vec2 sizeScaled, float xClipMin, float xClipMax, const shape_t::hit_result& hit);
}

namespace DAW::Shape {

class ShapeEdit {
    friend class guictr_curve_editor;
public:
    shape_t* curve = nullptr;
    shape_t curveBegin;
    shape_t curveTmp;
    vec2 dragBeginPos{ 0, 0 };
    vec2 editorScale{ 1, 1 };
    bool wasAltBegin = false;
    bool wasShiftBegin = false;
public:
    shape_t::hit_result dragged;
    bool bIsGridEnabledH = false;
    bool bIsGridEnabledV = false;
    int32_t gridStepsH = 8;
    int32_t gridStepsV = 8;
    std::function<void(const DAW::Shape::shape_t&, bool bIsDragMove)> callback;
public:
    ShapeEdit() = default;
    virtual ~ShapeEdit() = default;
    bool hasControlHandles() const;
    void setEditorCurve(shape_t* curve);
    bool onBeginDragCurveEditor(MouseEvent& evt);
    void onMoveDragCurveEditor(MouseEvent& evt);
    void onReleaseDragCurveEditor(MouseEvent& evt);
    bool onRightClickCurveEditor(MouseEvent& evt);
    bool mouseHitCurveEditor(const shape_t& shape, ivec2 mpos) const;
    void renderEditor(NVGcontext* vg, vec2 pos, const guitheme_t* theme, ivec2 relMousepos, bool bDrawGrid, const std::vector<int32_t>* pSelectedPoints = nullptr);
    virtual void layoutEditor(ivec2 size);
    virtual float snapH(float x) {
        return math::roundfS32(x * this->gridStepsH) / float(this->gridStepsH);
    }
    virtual float snapV(float y) {
        return math::roundfS32(y * this->gridStepsV) / float(this->gridStepsV);
    }
    virtual vec2 toParentSpace(const vec2& ctrlPt) const {
        auto scaledPt = vec2{ctrlPt.x, 1.0f - ctrlPt.y};
        return editorScale * scaledPt;
    }
    virtual vec2 toNormalizedSpace(const vec2& pt) const {
        vec2 ctrlPt = vec2(pt / editorScale);
        return vec2{ctrlPt.x, 1.0f - ctrlPt.y};
    }
};
} // namespace DAW::Shape