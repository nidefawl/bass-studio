#pragma once
#include "dialog.hpp"
#include "gui/controls/button.hpp"
#include "math/seq_math.hpp"
#include "platform.hpp"
#include "rand.hpp"

#include <functional>
#include <utility>
#include <utility>
#include <nanovg.h>

class guidialog_cb_yes_no final : public guidialog_base {
    guibutton btnYes;
    guibutton btnNo;

public:
    std::function<void(int)> cb;
    String strMessage;
public:
    guidialog_cb_yes_no(String title, String message) 
    : guidialog_base(ivec2(360, 140), true), strMessage(std::move(message)) {
        btnYes.setText("Yes");
        btnNo.setText("No");
        add(&btnYes);
        add(&btnNo);
        setBackgroundRendered(true);
        setLabel(std::move(title));
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
            prefSize.x = math::clamp<int32_t>(math::roundfS32(strw.getStringWidth(strMessage, htt)*1.5f), prefSize.x, 640);
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
        const int htt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
        const auto cs = vec2(getSizeContent()) * vec2(1, 0.6f);
        renderCenteredMultilineText(vg, theme, strMessage, htt, getLabelColor(), vec2(0), cs);
        for (auto c : guis) {
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
    }
};


class guidialog_message_box final : public guidialog_base {
    guibutton btnOk;
    String strMessage;
public:
    guidialog_message_box(String title, String message)
    : guidialog_base(ivec2(720, 200), true), strMessage(std::move(message)) {
        setLabel(std::move(title));
        btnOk.setText("Ok");
        add(&btnOk);
        setBackgroundRendered(true);
    }
    ~guidialog_message_box()
    {
        remove(&btnOk);
    }

    void layout() override {
        const auto cs = getSizeContent();
        const int htt = theme->get(GuiConstant::CONST_ROW_HEIGHT);
        determine_string_width strw(parentCtrl, theme);
        btnOk.size = {htt*4, htt};
        btnOk.pos  = {cs.x / 2 - btnOk.size.x/2, cs.y * 3/4 - btnOk.size.y / 2};
    }

    void buttonClicked(guibase* button) override {
        if (!parentCtrl) return;
        auto parentParent = parentCtrl->getParentCtrl();
        if (!parentParent) return;
        parentParent->closeDialogs();
    }

    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        const auto cs = vec2(getSizeContent()) * vec2(1, 0.6f);
        float fFontScale = 1.0f;
        ivec2 size       = this->size;
        int fontScale    = math::roundfS32((this->fontSize > 0 ? this->fontSize : math::min(size.y, size.x)) * fFontScale);
        renderCenteredMultilineText(vg, theme, strMessage, fontScale, getLabelColor(), vec2(0), cs);
        for (auto c : guis) {
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
    }
};