#include "gui/container/container.h"


class gui_samplesloaded : public guictr_base {
public:
    gui_samplesloaded() : guictr_base() {
        setBackgroundRendered(false);
        padding = 0;
        margin  = 0;
    }
    ~gui_samplesloaded() override {
        removeGuis();
    }
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
    }
    void layout() override {
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto* g : guis) {
            nvgSave(vg);
            g->render(vg);
            nvgRestore(vg);
        }
    }
};


guictr_base* makeGuiSamplesLoaded() {
    return new gui_samplesloaded();
}
