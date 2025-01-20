#include "button.hpp"
#include "gui/gui.hpp"
#include "gui/container/container.hpp"
#include "renderresources.hpp"

GuiColor::constant_t guibutton::getBackgroundColorFromState(int32_t stateflags) const {
    if ((stateflags & FLG_HAS_COLOR_BG) && getState()) {
        return buttonColor;
    }
    return guibase::getBackgroundColorFromState(stateflags);
}

void guibutton::renderButtonLabel(NVGcontext* vg, int32_t stateFlags) {
    if ((drawFn || str.length()) && size.y > 10 && size.x > 10) {
        nvgSave(vg);
        setScissorTransform(vg);

        auto minInset = math::min(size.y, size.x) / 4;
        float strWidth = 0;
        bool insetText = drawFn && str.length();
        ivec2 renderFrame = size;
        ivec2 renderPos(0);
        if (insetText) {
            renderFrame.x -= minInset*2;
            renderPos.x += minInset*2;
        }
        if (str.length() > 0 && (renderFrame.x > (drawFn ? 20 : 10))) {
            auto fontScale = math::clamp(math::min(size.y, size.x), 4, 48) * FONT_AUTOSCALE;
            strWidth = renderCenteredMultilineText(vg, theme, str, fontScale, getLabelColor(), renderPos, renderFrame);
            renderFrame.x = renderPos.x + (renderFrame.x - strWidth) * 0.5f;
            renderPos.x = 0;
        }
        if (drawFn && renderFrame.x > 5) {
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
