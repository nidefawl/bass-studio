#pragma once
#include "asynctask.h"
#include "gui/container/container.h"
#include "guicolors.h"
#include "host/daw/mainctrl.h"
#include "str_util.h"
#include <nanovg.h>

class async_test_task_impl : public DAW::async_task_t {
    static constexpr int MAX = 100;
    int32_t step = 0;
    int32_t step2 = 0;
    String progressDesc = "testing";
    // virtual String getTaskName() const = 0;
    // virtual String getProgressDesc() const = 0;
    // virtual void getPreciseProgress(double& progressOverall, double& progressDetail) {
    String getTaskName() const override {
        return "Test task";
    }
    String getProgressDesc() const override {
        return progressDesc;
    }
    void run() override {
        switch (m_state) {
        case state::idle:
            m_state = state::running;
            break;
        case state::running:
            if (step2 < MAX) {
                step2++;
                progressDesc = StringFormat("testing %d/%d", step2, MAX);
            } else {
                if (step++>4) {
                    m_state = state::finished;
                } else {
                    step2 = 0;
                }
            }
            break;
        case state::error:
        case state::finished:
        case state::cancelled:
            break;
        }
    }
    void getPreciseProgress(double& progressOverall, double& progressDetail) override {
        progressOverall = step / 5.0;
        progressDetail = step2 / (double)MAX;
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
        label = task->getTaskName();
        desc = task->getProgressDesc();
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

    auto task = dawCtrl->getDaw()->getAsyncTask();
    if (task) {
        double progressOverall = 0;
        double progressDetail = 0;
        task->getPreciseProgress(progressOverall, progressDetail);
        for (int i = 0; i < 2; ++i) {
            const int h = htt;
            const int w = cs.x - htt * 1;
            const int x = cs.x / 2 - w / 2;
            const int y = (i==1?(cs.y / 2):(cs.y/6)) - h / 2;
            double progress = i == 0 ? progressOverall : progressDetail;
            nvgBeginPath(vg);
            nvgRect(vg, x, y, w, h);
            nvgFillColor(vg, theme->getColor(getBackgroundColorFromState(getStateFlags())));
            nvgFillCustomPar(vg, -2);
            nvgFill(vg);
            nvgBeginPath(vg);
            nvgRect(vg, x, y, w * progress, h);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_KNOB));
            nvgFillCustomPar(vg, -3);
            nvgFill(vg);
            float fontHeight = h * 0.5f;
            renderText(vg, vec2(x + w/2, y + h / 2), vec2(w*0.5f, h), StringFormat("%.0f%%", progress * 100), fontHeight, NVG_ALIGN_CENTER| NVG_ALIGN_MIDDLE);
            renderText(vg, vec2(x + fontHeight, y + h / 2), vec2(w-fontHeight, h), i == 0 ? label : desc, h * 0.65f, NVG_ALIGN_LEFT| NVG_ALIGN_MIDDLE);
        }

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