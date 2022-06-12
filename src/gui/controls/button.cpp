#include "button.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "renderresources.h"

GuiColor::constant_t guibutton::getBackgroundColorFromState(int32_t stateflags) const {
    if ((stateflags & FLG_HAS_COLOR_BG) && getState()) {
        return buttonColor;
    }
    return guibase::getBackgroundColorFromState(stateflags);
}

void guibutton::renderButtonLabel(NVGcontext* vg, int32_t stateFlags) {
    if (drawFn || str.length()) {
        nvgSave(vg);
        setScissorTransform(vg);

        ivec2 renderPos(0);
        if (str.length() > 0) {
            auto fontScale = math::clamp(math::min(size.y, size.x), 4, 48) * FONT_AUTOSCALE;
            renderCenteredMultilineText(vg, theme, str, fontScale, getLabelColor(), renderPos, size);
        }
        if (drawFn) {
            int drawParm2 = isFlag(FLG_RENDER_BUTTON_WITH_LED) ? (getState() ? IMG_LED : IMG_LED_OFF) : -1;
            drawFn(vg, renderPos, size, theme->getColor(getBackgroundColor()), drawParm, drawParm2);
        }
        nvgRestore(vg);
    }
}
