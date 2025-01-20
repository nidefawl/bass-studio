#pragma once
#include <nanovg_min.h>
#include <vector>
#include <memory>
#include <functional>
#include "keyboard.hpp"
#include "math/seq_math.hpp"
#include "math/vec.hpp"
#include "str_util.hpp"
#include "color_util.hpp"
#include "gui/gui.hpp"
#include "gui/container/container.hpp"
#include "gui/plugin/pluginviewcontainers.hpp"
#include "gui/controls/knob.hpp"
#include "theme.hpp"


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
    std::function<String(float)> fnOverrideGetDisplay;

protected:
    knob_layout m_layout{};
private:
    textlabel_dynamic_t m_textLabelParamName;
    textlabel_dynamic_t m_textLabelParamValue;
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
    void onTick(AppCtrl*) override;
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
