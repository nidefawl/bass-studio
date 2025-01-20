
#include "gui/views/controls.hpp"
#include "platform.hpp"
#include "types.hpp"
#include "str_util.hpp"
#include "basectrl.hpp"
#include "gui/container/container.hpp"
#include "logging.hpp"
#include <nanovg.h>

namespace {
class guictr_test_inner : public guictr_base {
public:
    guictr_test_inner()
        : guictr_base()
    {
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
    }
    ~guictr_test_inner() override {
        removeGuis();
    }
    void render(NVGcontext* vg) override {
        if (!setScissorTransform(vg)) {
            return;
        }
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, size.x, size.y);
        nvgFillColor(vg, nvgRGBA(0, 255, 0, 128));
        nvgFill(vg);
        // translate to center
        nvgSave(vg);
        nvgTranslate(vg, size.x * 0.5f, size.y * 0.5f);
        auto fs = 64.0f;
        auto fontScale = fs * theme->getFloat(GuiConstant::CONST_FONT_SCALE);
        nvgFontSize(vg, fontScale);
        theme->bindFont(vg, UIFont::FONT_TEXTFIELD);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        auto aOrB = (getTimeMillis()/1000) % 3;
        String tmpStr = "";
        switch (aOrB) {
            default:
            case 0:
                tmpStr = " ";
                break;
            case 1:
                tmpStr = " a";
                break;
            case 2:
                tmpStr = " a ";
                break;
        }

        float textBounds[4]{ 0 };
        float textWidth = nvgTextBounds(vg, 0, 0, tmpStr.c_str(), nullptr, textBounds);

        // draw text bounds
        nvgBeginPath(vg);
        nvgRect(vg, textBounds[0], textBounds[1], textBounds[2] - textBounds[0], textBounds[3] - textBounds[1]);
        nvgFillColor(vg, nvgRGBA(255, 0, 0, 128));
        nvgFill(vg);
        // draw text
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
        float xret = nvgText(vg, 0, 0, tmpStr.c_str(), nullptr);

        // draw understrike using textWidth
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, 0);
        nvgLineTo(vg, textWidth, 0);
        nvgStrokeColor(vg, nvgRGBA(255, 255, 0, 255));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        // second understrike using xret
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, 4);
        nvgLineTo(vg, xret, 4);
        nvgStrokeColor(vg, nvgRGBA(0, 255, 255, 255));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        nvgRestore(vg);
    }
};
}
class guictr_test_outer : public guictr_base {
    guictr_test_inner ctr;
public:
    guictr_test_outer();
    ~guictr_test_outer() override;
    void layout() override;
};

guictr_test_outer::guictr_test_outer() : guictr_base() {
    setBackgroundRendered(true);
    add(&ctr);
}
guictr_test_outer::~guictr_test_outer() {
    removeGuis();
}

void guictr_test_outer::layout() {
    ivec2 cs          = getSizeContent();
    ctr.pos = {};
    ctr.size = cs;
    for (auto* gui : guis) {
        gui->layout();
    }
}

guictr_base* makeGuiTestCtr2() {
    return new guictr_test_outer();
}
