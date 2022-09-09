
#include <GLFW/glfw3.h>
#include <__algorithm/remove_if.h>
#include <cmath>
#include <cstddef>
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
#include "file/shape_file.h"

namespace DAW::Shape {

void DrawShape(const shape_t& curve, NVGcontext*vg, guitheme_t* theme, vec2 pos, vec2 size, vec2 mousePos, const shape_t::hit_result& hit) {
    if (curve.pts.empty())
        return;

    auto numCurvePts = CtrSize(curve.pts);
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
    auto lineColor = theme->getColor(GuiColor::COL_KNOB);
    auto fillColor = lineColor;
    auto handleColor = theme->getColor(GuiColor::COL_KNOB_IND);
    auto hoverColor = theme->getColor(GuiColor::COL_AUTOMATED);
    fillColor.a = 0.3;
    for (int32_t pass = 0; pass < 2; ++pass) {
        if (pass == 0) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, pos.x + pts.front().pos.x*size.x-5, pos.y + size.y);
        }
        for (int32_t edge = 0; edge < nPoints - 1; edge++) {
            auto pt0 = pts[edge];
            auto pt1 = pts[edge+1];
            auto ptD = pt1.pos - pt0.pos;
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
            auto pt0 = pts[idx + 1];
            auto pt1 = pts[idx + 2];
            nvgBeginPath(vg);
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
class guictr_curve_shape : public guictr_base {
    friend class guictr_curve_editor;
    shape_t curveInternal;
    shape_t* curve;
    shape_t curveBegin;
    shape_t curveTmp;
    shape_t::hit_result dragged;
    vec2 dragBeginPos{ 0, 0 };
    int32_t INSET_OUTER = 0;
    int32_t INSET_INNER = 0;
    vec2 aspectView{ 1, 1 };
    bool bIsGridEnabledH = true;
    bool bIsGridEnabledV = true;
    int32_t gridStepsH = 8;
    int32_t gridStepsV = 8;
    bool wasAltBegin = false;
    bool wasShiftBegin = false;
    std::function<void(const DAW::Shape::shape_base_t&)> callback;
public:
    guictr_curve_shape() : curve(&curveInternal)
    {
        padding = 4;
        margin = 4;
        setBackgroundRendered(true);
        setCanMouseHit(true);
        curve->pts.push_back({ { 0, 0 }, 0.5f });
    }
    GuiColor::constant_t getOuterBackgroundColorFromState(int32_t stateflags) const override {
        return GuiColor::COL_BG_DRKER2;
    }
    void render(NVGcontext* vg) override {
        drawBackground(vg, theme, getPosContent(), getSizeContent(), margin, isBackgroundRenderedInset());

        if (!setScissorTransform(vg)) {
            return;
        }
        int32_t gridStepsH = math::clamp<int32_t>(this->gridStepsH, 1, 128);
        int32_t gridStepsV = math::clamp<int32_t>(this->gridStepsV, 1, 128);
        auto cs = getSizeContent();
        auto gridStep = vec2(cs) / vec2(gridStepsH, gridStepsV);

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
        float x = 0;
        for (int32_t i = 0; i < steps_bg; i += 2) {
            nvgBatchedRect(vg, x, 0, gridStep.x, size.y);
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
                nvgBatchedRect(vg, gridStep.x * i - lineThickness * 0.5f, 0, lineThickness, size.y);
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
                nvgBatchedRect(vg, 0, gridStep.y * i - lineThickness * 0.5f, size.x, lineThickness);
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
        const auto posIn = ivec2(parentCtrl->m_mousePos);
        const auto relMousepos = toControlsObjectSpace(posIn, this);
        const auto mouseLocal = screenToCtrl(relMousepos);
        auto higlightHit = curve->getMouseHit(mouseLocal);
        if (dragged.type != shape_t::hittype::HIT_NONE) {
            higlightHit = dragged;
        }
        DrawShape(*curve, vg, theme, vec2(0), cs, mouseLocal, higlightHit);
    }

    void handleRightClick(MouseEvent& evt) override;

    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        return guictr_base::mouseHitTest(mpos, evt);
    }
    bool hasControlHandles() {
        return !curve->pts.empty();
    }

    void handleDraggedBegin(MouseEvent& evt) override {
        if (hasControlHandles()) {
            vec2 local   = screenToCtrl(evt.relMousepos);
            dragged = {};
            if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
                curve->pts.push_back({ { local.x, local.y }, 0.5f });
                curve->sort();
            } else {
                curveTmp = *curve;
                curveBegin = *curve;
                dragged = curve->getMouseHit(local);
                dragBeginPos = local;
            }
            wasAltBegin = isAlt(evt.kbmods);
            wasShiftBegin = isShift(evt.kbmods);
        }
    }

    void handleDraggedMove(MouseEvent& evt) override {
        if (hasControlHandles()) {
            vec2 local = screenToCtrl(evt.relMousepos);
            if (wasShiftBegin) {
                if (bIsGridEnabledH) {
                    local.x = math::floorfS32(local.x * this->gridStepsH) / float(this->gridStepsH);
                }
                if (bIsGridEnabledV && !isAlt(evt.kbmods)) {
                    local.y = snapV(local.y);
                }
                local.x = math::clamp(local.x, 0.0f, 1.0f);
                local.y = math::clamp(local.y, 0.0f, 1.0f);
                float beginRange = local.x;
                float endRange = beginRange + 1.0f / math::max<float>(1.0f, gridStepsH);
                curveTmp = curveBegin;
                float leftVal = curveTmp.sampleCurve(beginRange, false);
                float rightVal = curveTmp.sampleCurve(endRange, true);
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
                if (beginRange >= 0.0f && beginRange <= 1.0f && endRange >= 0.0f && endRange <= 1.0f) {
                    curveTmp.pts.insert(curveTmp.pts.begin() + idxInsert++,{ { beginRange, leftVal }, 0.5f });
                    curveTmp.pts.insert(curveTmp.pts.begin() + idxInsert++,{ { beginRange, local.y }, 0.5f });
                    curveTmp.pts.insert(curveTmp.pts.begin() + idxInsert++,{ { endRange, local.y }, 0.5f });
                    curveTmp.pts.insert(curveTmp.pts.begin() + idxInsert++,{ { endRange, rightVal }, 0.5f });
                    curveTmp.sort();
                }
                *curve = curveTmp;
                if (callback)
                    callback(*curve);
                return;
            }
            if (dragged.type == shape_t::hittype::HIT_NODE && dragged.idx < CtrSize(curveTmp.pts)) {
                if (bIsGridEnabledH && !isAlt(evt.kbmods)) {
                    local.x = snapH(local.x);
                }
                if (bIsGridEnabledV && !isAlt(evt.kbmods)) {
                    local.y = snapV(local.y);
                }
                while (local.x < 0.0f) {
                    local.x += 1.0f;
                }
                local.x = math::clamp(modf(local.x, nullptr), 0.0f, 1.0f);
                local.y = math::clamp(local.y, 0.0f, 1.0f);
                curveTmp = curveBegin;
                auto& pt   = curveTmp.pts[dragged.idx];
                pt.pos = local;
                curveTmp.sort();
                *curve = curveTmp;
                if (callback)
                    callback(*curve);
                return;
            }
            if (dragged.type == shape_t::hittype::HIT_EDGE && dragged.idx < CtrSize(curveTmp.pts)) {
                *curve = curveTmp;
                auto& pt   = curve->pts[dragged.idx];
                auto& ptNext = curve->getPointAfterIdx(dragged.idx);
                float fDist = (local - dragBeginPos).y;
                *evt.dragDistance = ivec2(0);
                if (wasAltBegin) {
                    float fSign = ptNext.pos.y>pt.pos.y ? 1.0f : -1.0f;
                    pt.shape = math::clamp(pt.shape - fSign*fDist, 0.0f, 1.0f);
                } else {
                    if (bIsGridEnabledV && !isAlt(evt.kbmods)) {
                        local.y = snapV(local.y);
                        fDist = local.y - pt.pos.y;
                    }
                    pt.pos.y = math::clamp(pt.pos.y + fDist, 0.0f, 1.0f);
                    if (&ptNext != &pt) {
                        ptNext.pos.y = math::clamp(ptNext.pos.y + fDist, 0.0f, 1.0f);
                    }
                }
                if (callback)
                    callback(*curve);
                return;
            }
        }
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        if (hasControlHandles()) {
            dragged = {};
            curve->eraseDuplicates();
            if (callback)
                callback(*curve);
        }
    }
private:
    vec2 toScaledPt(const vec2& ctrlPt) {
        return {ctrlPt.x, 1.0 - ctrlPt.y};
    }
    vec2 ctrlPtToView(const vec2& ctrlPt) {
        auto scaledPt = toScaledPt(ctrlPt);//(vec2(ctrlPt) * aspectView) + 1.0f;
        return vec2(getSizeContent()) * scaledPt + vec2(INSET_INNER);
    }
    vec2 ctrlPtToScreen(const vec2& ctrlPt) {
        return ctrlPtToView(ctrlPt) + vec2(INSET_OUTER);
    }
    vec2 viewToCtrlPt(const vec2& pt) {
        vec2 relPt  = pt - vec2(INSET_INNER);
        vec2 ctrlPt = vec2(relPt / vec2(getSizeContent()));
        ctrlPt.y = 1.0f - ctrlPt.y;
        return ctrlPt;
    }
    vec2 screenToCtrl(const vec2& pt) {
        return viewToCtrlPt(pt - vec2(INSET_OUTER));
    }
    float snapH(float x) {
        return math::roundfS32(x * this->gridStepsH) / float(this->gridStepsH);
    }
    float snapV(float y) {
        return math::roundfS32(y * this->gridStepsV) / float(this->gridStepsV);
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
        parent->buttonClicked(button);
        if (&buttonGrid == button) {
            buttonGrid.setText(axes[axis] + " Grid: " + String(buttonGrid.getState() ? "On" : "Off"));
        }
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
    void buttonClicked(guibase* button) override {
        parent->buttonClicked(button);
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
        guiType = CTR_TYPE_PROPERTIES;
        getContainerLabel(guiType, this->label);
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
                    shape_t tmp{shapeLoaded.curve.pts};
                    tmp.sort();
                    *shape.curve = tmp;
                    controls.selectPreset.setString(shapeLoaded.name);
                    if (shape.callback)
                        shape.callback(*shape.curve);
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
    void setShapeEditorCallback(std::function<void(const DAW::Shape::shape_base_t&)> callback) override {
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
        controls.size.y = math::min<int32_t>(inputHeight, size.y/8);
        shape.pos.y = controls.bottom() + padding;
        shape.size.y = cs.y - controls.size.y;
        guictr_base::layout();
    }

    void buttonClicked(guibase* button) override {
        if (&controls.gridControlH.buttonGrid == button) {
            shape.bIsGridEnabledH = !shape.bIsGridEnabledH;
        }
        if (&controls.gridControlV.buttonGrid == button) {
            shape.bIsGridEnabledV = !shape.bIsGridEnabledV;
        }
        if (&controls.buttonSave == button) {
            shape_preset_t shapePreset { 1, "test", shape_base_t{shape.curve->pts} };
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
                shapePreset.name = name;
                controls.selectPreset.setString(shapePreset.name);
                saveShapePresetFile(shapePreset, path);
            }
        }
    }
};

void guictr_curve_shape::handleRightClick(MouseEvent& evt) {
    if (hasControlHandles()) {
        vec2 local = screenToCtrl(evt.relMousepos);
        float minDist = 0.0f;
        int32_t minPt  = curve->getMinPt(local, &minDist);
        if (minPt > -1) {
            curve->pts.erase(curve->pts.begin() + minPt);
            curve->sort();
            return;
        }
    }
}

} // namespace DAW::Shape


i_ctr_shape_editor* makeShapeEditor() {
    return new DAW::Shape::guictr_curve_editor();
}

// void setShapeEditorCallback(guictr_base* curveEditor, std::function<void(const DAW::Shape::shape_base_t&)> callback) {
//     auto editor = dynamic_cast<DAW::Shape::guictr_curve_editor*>(curveEditor);
//     if (editor) {
//         editor->setShapeEditorCallback(std::move(callback));
//     }
// }

// void setShapeEditorShape(guictr_base* curveEditor, const DAW::Shape::shape_base_t& shape) {
//     auto editor = dynamic_cast<DAW::Shape::guictr_curve_editor*>(curveEditor);
//     if (editor) {
//         editor->setShapeEditorShape(shape);
//     }
// }