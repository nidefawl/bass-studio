#include "dropdown.h"
#include "guiglobals.h"
#include "str_util.h"

bool guidropdownbase::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    if (-1 != getSelectIndex()) {
        bool isUp = yoffset > 0;
        if (isUp) {
            select(dropdown_field_selectitem::SELECT_PREVIOUS, 1);
        } else {
            select(dropdown_field_selectitem::SELECT_NEXT, 1);
        }
        return true;
    }
    return false;
}

bool guidropdownbase::handleKeyInput(KeyEvent& kevt) {
    if (kevt.type == KeyboardState::K_PRESS || kevt.type == KeyboardState::K_REPEAT) {
        if (isArrowKey(kevt.keyCode)) {
            ivec2 dir;
            arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
            if (dir.y) {
                if (dir.y > 0) {
                    select(dropdown_field_selectitem::SELECT_PREVIOUS, 1);
                } else {
                    select(dropdown_field_selectitem::SELECT_NEXT, 1);
                }
            }
        }
    }
    return false;
}

void guidropdownbase::select(dropdown_field_selectitem req, int32_t idxOffset) {
    auto index = getSelectIndex();
    if (index == -1)
        return;

    switch (req) {
        case SELECT_IDX:
            setSelectedIndex(idxOffset);
            break;
        case SELECT_NEXT:
            setSelectedIndex(math::min<int32_t>(getLastIndex(), index + idxOffset));
            break;
        case SELECT_PREVIOUS:
            setSelectedIndex(math::max<int32_t>(0, index - idxOffset));
            break;
        case SELECT_FIRST:
            setSelectedIndex(0);
            break;
        case SELECT_LAST:
            setSelectedIndex(getLastIndex());
            break;
    }
}

void guidropdownbase::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    const auto stateFlags = getStateFlags();
    renderWidgetBorder(vg, stateFlags);
    auto fontSizeScaled = math::clamp(size.y, 4, 48) * FONT_AUTOSCALE;

    if (this->label.length()) {
        auto posText = vec2(pos) + vec2(size.x - 3, size.y * 0.5f);
        float textWidth = renderTextLabel(vg,
                        posText,
                        vec2(size),
                        getString(),
                        theme,
                        fontSizeScaled,
                        theme->getColor(getLabelColor()),
                        NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        renderTextLabel(vg,
                        vec2(pos) + vec2(3.0f, size.y * 0.5f),
                        vec2(size.x - textWidth - 6.0f, size.y),
                        label,
                        theme,
                        fontSizeScaled,
                        theme->getColor(GuiColor::COL_LABEL_INACTIVE),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    } else {
        renderTextLabel(vg,
                        vec2(pos) + vec2(size) * 0.5f,
                        vec2(size),
                        getString(),
                        theme,
                        fontSizeScaled,
                        theme->getColor(getLabelColor()),
                        NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
}
