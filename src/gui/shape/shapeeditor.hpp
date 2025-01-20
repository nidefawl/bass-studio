#pragma once
#include <functional>
#include "gui/contextmenu/contextmenu_base.hpp"
#include "host/shape/shape.hpp"
#include "gui/container/container.hpp"

class i_ctr_shape_editor {
protected:
    ~i_ctr_shape_editor() = default;

public:
    i_ctr_shape_editor() = default;

public:
    virtual void setShapeEditorCallback(std::function<void(const DAW::Shape::shape_t&, bool bIsDragMove)> callback) = 0;
    virtual void setShapeEditorShapeRef(DAW::Shape::shape_t* shape) = 0;
    virtual guictr_base* getGuiContainer() = 0;
    virtual void setInputHeight(int32_t height) = 0;
};

i_ctr_shape_editor* makeShapeEditor();


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

class guictr_curve_shape final : public guictr_base, public ShapeEdit {
    friend class guictr_curve_editor;
    shape_t curveInternal;
    bool bHandleMouseDrag = true;
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
    void setHandleMouseDrag(bool b) {
        bHandleMouseDrag = b;
    }
    GuiColor::constant_t getOuterBackgroundColorFromState(int32_t stateflags) const override {
        return GuiColor::COL_BG_DRKER2;
    }
    void render(NVGcontext* vg) override;
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
        if (bHandleMouseDrag)
            onBeginDragCurveEditor(evt);
    }

    void handleDraggedMove(MouseEvent& evt) override {
        if (bHandleMouseDrag)
            onMoveDragCurveEditor(evt);
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        if (bHandleMouseDrag)
            onReleaseDragCurveEditor(evt);
    }
    shape_t& getShape() {
        return curveInternal;
    }
    const shape_t& getShape() const {
        return curveInternal;
    }
};

class ctxtmenu_lfo_shape_select final : public ctxtmenu_entry {
    struct _shape_sel_entry {
        DAW::Shape::ShapeWaveform shape;
        int x;
        int y;
        int w;
        String name;
    };
    std::vector<_shape_sel_entry> entries;

public:
    const int pad   = 10;
    const int inset = 5;
public:
    ctxtmenu_lfo_shape_select(String _title, int _id)
        : ctxtmenu_entry(std::move(_title), _id)
    {
        using DAW::Shape::ShapeWaveform;
        entries.push_back({ ShapeWaveform::SHAPE_SINE, 0, 0, 0, "Sine" });
        entries.push_back({ ShapeWaveform::SHAPE_TRIANGLE, 0, 0, 0, "Triangle" });
        entries.push_back({ ShapeWaveform::SHAPE_SAW, 0, 0, 0, "Saw" });
        entries.push_back({ ShapeWaveform::SHAPE_SQUARE, 0, 0, 0, "Square" });
        entries.push_back({ ShapeWaveform::SHAPE_SINE_INV, 0, 0, 0, "Sine Inv" });
        entries.push_back({ ShapeWaveform::SHAPE_TRIANGLE_INV, 0, 0, 0, "Triangle Inv" });
        entries.push_back({ ShapeWaveform::SHAPE_SAW_INV, 0, 0, 0, "Saw Inv" });
        entries.push_back({ ShapeWaveform::SHAPE_SQUARE_INV, 0, 0, 0, "Square Inv" });
    }

    void layout(ivec2 size, float _fontSize, determine_string_width& strw) override {
        width = size.x;
        this->fontSize = _fontSize;
        const int h    = math::roundfS32(_fontSize);
        layoutE(width, h, 4);
    }

    void layoutE(int tw, int h, int perRow) {
        int iX      = inset;
        int iY      = h + 2;
        int elW     = (tw - inset * 2) / perRow;
        for (_shape_sel_entry& e : entries) {
            this->height = iY + h;
            e.x = iX;
            e.y = iY;
            e.w = elW;
            iX += e.w;
            if (iX >= tw - inset * 2) {
                iX = inset;
                iY += h;
            }
        }
    }

    void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
        auto h = fontSize * 1.1f;

        renderTextLabel(vg,
                        vec2(leftOffset(), y + h * 0.5f),
                        vec2(width, h),
                        title,
                        theme,
                        fontSize,
                        theme->getColor(GuiColor::COL_TEXT),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        for (_shape_sel_entry& e : entries) {
            if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                nvgBeginPath(vg);
                nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                nvgFill(vg);
            }
            int inset = 4;
            vec2 waveformPos = vec2(e.x, y + e.y) + vec2(inset, inset);
            vec2 waveformSize = vec2(e.w, h) - vec2(inset * 2, inset * 2);
            drawWaveform(vg, waveformPos, waveformSize, e.shape, theme->getColor(GuiColor::COL_TEXT));
        }
    }

    bool contains(ivec2& ctxtSize, ivec2& mouse) const override {
        return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
    }

    int getClicked(ivec2& ctxtSize, ivec2& mouse) override {
        if (contains(ctxtSize, mouse)) {
            const auto h = this->fontSize;
            for (_shape_sel_entry& e : entries) {
                if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= 0 && mouse.x < e.x + e.w) {
                    return this->id + e.shape;
                }
            }
        }
        return -1;
    }
};

DAW::Shape::guictr_curve_shape* makeShapeCurveView();

} // namespace DAW::Shape