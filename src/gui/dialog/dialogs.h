#pragma once
#include "dialog.h"
#include "gui/controls/button.h"
#include "math/seq_math.h"
#include "platform.h"
#include "rand.h"

#include <functional>
#include <nanovg.h>

class guidialog_cb_yes_no : public guidialog_base {
    guibutton btnYes;
    guibutton btnNo;

public:
    std::function<void(int)> cb;
    String message;
    // int64_t tmCreatedMillis = 0;
    // void onTick(AppCtrl* ctrl) override {
    //     if (getTimeMillis() - tmCreatedMillis > 555) {
    //         // seq_rand rnd;
    //         // rnd.rng_seed(static_cast<uint64_t>(tmCreatedMillis));
    //         // cb(rnd.rng_bits(1) != 0);
    //         cb(1);
    //     }
    // }
public:
    guidialog_cb_yes_no() : guidialog_base(ivec2(360, 140), true) {
        btnYes.setText("Yes");
        btnNo.setText("No");
        add(&btnYes);
        add(&btnNo);
        setBackgroundRendered(true);
        // tmCreatedMillis = getTimeMillis();
    }
    ~guidialog_cb_yes_no() override {
        remove(&btnNo);
        remove(&btnYes);
    }
    void determineSize(ivec2& prefSize) override {
        if (prefSize == ivec2(0)) {
            const int htt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
            determine_string_width strw(parentCtrl, theme);
            prefSize = dialogSize;
            prefSize.x = math::clamp<int32_t>(math::roundfS32(strw.getStringWidth(message, htt)*1.5f), prefSize.x, 640);
        }
    }

    void layout() override {
        const auto cs = getSizeContent();
        const int htt = theme->get(GuiConstant::CONST_ROW_HEIGHT);
        int32_t px = cs.x / 3;
        determine_string_width strw(parentCtrl, theme);
        for (auto* btn : {&btnYes, &btnNo}) {
            btn->size = {htt*4, htt};
            btn->pos  = {px - btn->size.x/2, cs.y * 3/4 - btn->size.y / 2};
            px += cs.x/3;
        }
    }
    void buttonClicked(guibase* button) override {
        int n = button == &btnYes ? 1 : 0;
        cb(n);
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto c : guis) {
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
        const int htt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
        const auto cs = getSizeContent();
        renderText(vg, vec2(cs.x / 2, cs.y / 4), vec2(cs.x-htt*4, cs.y / 2), message, htt, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
};
