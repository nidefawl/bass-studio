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

        auto minInset = math::min(size.y, size.x) / 8;
        float strWidth = 0;
        bool insetText = drawFn && str.length();
        ivec2 renderFrame = size;
        ivec2 renderPos(0);
        if (insetText) {
            renderFrame.x -= minInset*2;
            renderPos.x += minInset*2;
        }
        if (str.length() > 0) {
            auto fontScale = math::clamp(math::min(size.y, size.x), 4, 48) * FONT_AUTOSCALE;
            strWidth = renderCenteredMultilineText(vg, theme, str, fontScale, getLabelColor(), renderPos, renderFrame);
            renderFrame = vec2(size.x * 0.5f - strWidth * 0.5f - renderPos.x, size.y);
        }
        if (drawFn) {
            renderPos.x = strWidth > 0 ? minInset : 0;
            int drawParm2 = isFlag(FLG_RENDER_BUTTON_WITH_LED) ? (getState() ? IMG_LED : IMG_LED_OFF) : -1;
            drawFn(vg, renderPos, renderFrame, theme->getColor(getBackgroundColor()), drawParm, drawParm2);
        }
        nvgRestore(vg);
    }
}
guibutton::guibutton() {
    setCanMouseHit(true);
    setFlag(FLG_RENDER_BACKGROUND_INSET, true);
    setFlag(FLG_BG_SHADING, false);
}
