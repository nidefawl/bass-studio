
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
#include "host/shape/shape.h"
#include "file/shapefile.h"
#include "shape-render.hpp"

namespace DAW::Shape {

class guictr_curve_preset_control final : public guictr_base {
    friend class guictr_curve_controls;
    friend class guictr_curve_editor;
    guidropdown_select_preset selectPreset;
    guibutton buttonSave;
public:
    guictr_curve_preset_control() {
        padding = 0;
        margin = 0;
        buttonSave.drawFn   = drawTextureSymbol;
        buttonSave.drawParm = ICON_SAVE;
        buttonSave.setText("Save");
        add(&selectPreset);
        add(&buttonSave);
    }
    ~guictr_curve_preset_control() {
        removeGuis();
    }
    void layout() override {
        auto cs = getSizeContent();
        vec2 sizePadded = vec2(cs.x - 2, cs.y);
        float btnWidth = 0.3f;
        buttonSave.size = vec2(sizePadded.x * btnWidth, sizePadded.y);
        if (buttonSave.size.x < 40) {
            buttonSave.size.x = buttonSave.size.y;
            buttonSave.setText("");
        } else {
            buttonSave.setText("Save");
        }
        float presetWidth = sizePadded.x - buttonSave.size.x - 2;
        selectPreset.size = vec2(presetWidth, sizePadded.y);
        selectPreset.pos = vec2(0, 0);
        buttonSave.pos = vec2(selectPreset.right() + 2, 0);
        guictr_base::layout();
    }
};

class guictr_curve_grid_control final : public guictr_base {
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
        // setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
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
        guictr_base::buttonClicked(button);
        if (&buttonGrid == button) {
            buttonGrid.setText(axes[axis] + " Grid: " + String(buttonGrid.getState() ? "On" : "Off"));
        }
    }

    void layout() override {
        auto cs = getSizeContent();
        vec2 sizePadded = vec2(cs.x - 2, cs.y);
        float stepsWidth = 0.3f;
        inputGridSteps.size = vec2(sizePadded.x * stepsWidth, sizePadded.y);
        if (inputGridSteps.size.x < 40) {
            inputGridSteps.size.x = inputGridSteps.size.y;
            inputGridSteps.setLabel("");
        } else {
            inputGridSteps.setLabel("Steps");
        }
        float btnWidth = sizePadded.x - inputGridSteps.size.x - 2;
        buttonGrid.size = vec2(btnWidth, sizePadded.y);
        buttonGrid.pos = vec2(0, 0);
        inputGridSteps.pos = vec2(buttonGrid.right() + 2, 0);
        guictr_base::layout();
    }
};

class guictr_curve_controls final : public guictr_base {
    friend class guictr_curve_editor;
    guictr_curve_preset_control presetControl;
    guictr_curve_grid_control gridControlH;
    guictr_curve_grid_control gridControlV;

public:
    guictr_curve_controls()
        : gridControlH(0),
        gridControlV(1)
    {
        padding = 2;
        margin = 4;
        setCanMouseHit(true);
        setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        add(&presetControl);
        add(&gridControlH);
        add(&gridControlV);
    }
    ~guictr_curve_controls() {
        removeGuis();
    }
};
class guictr_curve_editor final : public guictr_base, public i_ctr_shape_editor {
    seq_rand rand;
    guictr_curve_shape shape;
    guictr_curve_controls controls;
    PresetManager presetManager;
    int32_t inputHeight = HEIGHT_DEFAULT_INPUT;
    bool bScaleInputHeight = true;
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
        controls.presetControl.selectPreset.setPresetManager(presetManager);
        controls.presetControl.selectPreset.setCallback([this](const PresetManager::Preset& path) {
            shape_preset_t shapeLoaded{};
            if (loadShapePresetFile(path.path, shapeLoaded)) {
                if (shapeLoaded.version) {
                    auto& presetShape = shapeLoaded.curve;
                    shape_t tmp{presetShape.flags, std::move(presetShape.pts), presetShape.name, 0.0f };
                    // tmp.sort();
                    controls.presetControl.selectPreset.setString(tmp.name);
                    *shape.curve = tmp;
                    if (shape.callback)
                        shape.callback(*shape.curve, false);
                }
            }
        });
        controls.presetControl.selectPreset.setString("Empty");
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
    void setInputHeight(int32_t height) override {
        inputHeight = height;
        bScaleInputHeight = false;
    }

    void layout() override {
        auto cs = getSizeContent();
        shape.pos = controls.pos = {0,0};
        shape.size = controls.size = cs;
        auto controlsHeight = inputHeight;
        if (bScaleInputHeight && inputHeight > 12) {
            controlsHeight = math::clamp<int32_t>(size.y/8, 12, inputHeight);
        };
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
        if (&controls.presetControl.buttonSave == button) {
            shape_preset_t shapePreset { 1, *shape.curve };
            String defaultPresetPath = "";
            auto& paths = presetManager.getPresetPaths();
            if (!paths.empty()) {
                defaultPresetPath = paths.front();
            }
            CreateDirectoryIfNotExists(defaultPresetPath);
            String path;
            auto window = parentCtrl->window;
            if (promptUserFilePath(window, 1, FILE_TYPES_SHAPEPRESET, path, defaultPresetPath)) {
                String ext;
                String name;
                SplitPath(path, nullptr, &name, &ext);
                if (ext.empty()) {
                    path += ".";
                    path += FILE_TYPES_SHAPEPRESET.types.front().ext;
                }
                shapePreset.curve.name = name;
                controls.presetControl.selectPreset.setString(name);
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
                double _unused = 0.0;
                snapped.x      = math::clamp<float>(std::modf(fModfInput, &_unused), 0.0f, 1.0f);
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
        DrawGrid(vg, theme, pos, editorScale, gridStepsH, gridStepsV, false, false);
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

void guictr_curve_shape::render(NVGcontext* vg) {
    auto cs = getSizeContent();
    if (isBackgroundRendered())
        drawBackground(vg, theme, getPosContent(), cs, margin, isBackgroundRenderedInset());

    if (!setScissorTransform(vg)) {
        return;
    }
    auto relMousePos = toControlsObjectSpace(parentCtrl->m_mousePos, this);
    renderEditor(vg, { 0, 0 }, theme, relMousePos, isBackgroundRendered());
}

DAW::Shape::guictr_curve_shape* makeShapeCurveView() {
    return new DAW::Shape::guictr_curve_shape();
}

}// namespace DAW::Shape


i_ctr_shape_editor* makeShapeEditor() {
    return new DAW::Shape::guictr_curve_editor();
}
