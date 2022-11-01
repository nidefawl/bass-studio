#pragma once
#include "asynctask.h"
#include "gui/container/container.h"
#include "host/daw/mainctrl.h"
#include "str_util.h"

class async_test_task_impl : public DAW::async_task_t {
    static constexpr int MAX = 100;
    int32_t current = 0;
    String getDesc() const override {
        return "Test task";
    }
    void run() override {
        switch (m_state) {
        case state::idle:
            m_state = state::running;
            break;
        case state::running:
            if (current < MAX) {
                current++;
            } else {
                m_state = state::finished;
            }
            break;
        case state::error:
        case state::finished:
        case state::cancelled:
            break;
        }
    }
    void getPreciseProgress(double& progress, String& text) override {
        progress = current / double(MAX);
        text     = StringFormat("%d/%d", current, MAX);
    }
};
DAW::async_task_t* createTestTask() {
    return new async_test_task_impl();
}
gui_asyc_progress::gui_asyc_progress() : guidialog_base(ivec2(360, 140), true) {
    btnCancel.setText("Cancel");
    add(&btnCancel);
    setBackgroundRendered(true);
}

gui_asyc_progress::~gui_asyc_progress() {
    removeGuis();
}

void gui_asyc_progress::onTick(AppCtrl* ctrl) {
    auto task = dawCtrl->getDaw()->getAsyncTask();
    if (task) {
        if (task->isFinished() || task->isError() || task->isCancelled()) {
            btnCancel.setText("Close");
        }
        label = task->getDesc();
    }
    guidialog_base::onTick(ctrl);
}

void gui_asyc_progress::render(NVGcontext* vg) {
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
    renderText(vg, vec2(cs.x / 2, cs.y / 6), vec2(cs.x - htt * 4, cs.y / 3), this->label, htt, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    auto task = dawCtrl->getDaw()->getAsyncTask();
    if (task) {
        double progress = 0;
        String text;
        task->getPreciseProgress(progress, text);

        const int h = htt / 2;
        const int w = cs.x - htt * 4;
        const int x = cs.x / 2 - w / 2;
        const int y = cs.y / 2 - h / 2;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, w, h, 2);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 64));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, w * progress, h, 2);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 64));
        nvgFill(vg);
        renderText(vg, vec2(x + w / 2, y + h / 2), vec2(w, h), text, h, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    }
}

void gui_asyc_progress::buttonClicked(guibase* button) {
    if (button == &btnCancel) {
        auto task = dawCtrl->getDaw()->getAsyncTask();
        if (task) {
            task->cancel();
        }
        dawCtrl->getDaw()->setAsyncTask(nullptr);
        delete task;
    }
}

void gui_asyc_progress::layout() {
    const auto cs = getSizeContent();
    const int htt = theme->get(GuiConstant::CONST_ROW_HEIGHT);
    determine_string_width strw(parentCtrl, theme);
    const int h = htt;
    const int w = cs.x / 3;
    const int x = cs.x / 2 - w / 2;
    const int y = cs.y - cs.y/6 - h / 2;
    btnCancel.size = { w, h };
    btnCancel.pos  = { x, y };
    for (auto* g: guis) {
        g->layout();
    }
}

void gui_asyc_progress::determineSize(ivec2& prefSize) {
    if (prefSize == ivec2(0)) {
        const int htt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
        determine_string_width strw(parentCtrl, theme);
        prefSize   = dialogSize;
        prefSize.x = math::clamp<int32_t>(math::roundfS32(strw.getStringWidth(this->label, htt) * 1.5f), prefSize.x, 640);
    }
}