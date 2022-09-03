#pragma once
#include <vector>
#include <memory>
#include <functional>
#include "keyboard.h"
#include "math/vec.h"
#include "str_util.h"
#include "color_util.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/controls/knob.h"


class guiknob_labeled_base : public guiknob {
public:
    struct knob_layout {
        float scaleLabel;
        float scaleValue;
        float fontScaleLabel;
        float fontScaleValue;
        int labelHeight;
        int valueHeight;
        ivec2 pLabel;
        ivec2 pValue;
        ivec2 sLabel;
        ivec2 sValue;
        ivec2 pKnob;
        ivec2 sKnob;
        int inset = 2;
        bool renderLabelBorder = true;
    };
    std::function<String(float)> fnGetDisplayValue;
    String valueDisplay = "  ";

protected:
    knob_layout m_layout{};
public:
    explicit guiknob_labeled_base(knobtype knobType)
        : guiknob(knobType)
    {
        m_layout.fontScaleLabel = 1.0f;
        m_layout.fontScaleValue = 1.0f;
        m_layout.scaleLabel = 0.12f;
        m_layout.scaleValue = 0.12f;
    }
    ~guiknob_labeled_base() override = default;
    void setLabelsScale(float scaleLabel, float scaleValue) {
        m_layout.scaleLabel = scaleLabel;
        m_layout.scaleValue = scaleValue;
    }
    void setLabelsFontScale(float fontScaleLabel, float fontScaleValue) {
        m_layout.fontScaleLabel = fontScaleLabel;
        m_layout.fontScaleValue = fontScaleValue;
    }
    knob_layout getLayout() const { return m_layout; }
    void layout() override;
    void render(NVGcontext* vg) override;
    void handleDraggedBegin(MouseEvent& evt) override {
        bool isTopLabelClick = m_layout.labelHeight > 0 && evt.relMousepos.y < m_layout.labelHeight;
        bool isValueClick = m_layout.valueHeight > 0 && evt.relMousepos.y > size.y - m_layout.valueHeight;
        if ((isTopLabelClick || isValueClick) && (isCtrl(evt.kbmods) || (evt.type == MouseEventType::M_EVT_DOUBLECLICK))) {
            parent->buttonClicked(this);
            return;
        }
        guiknob::handleDraggedBegin(evt);
    }
};
