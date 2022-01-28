#include "guicontainer.h"


class gui_samplesloaded : public guictr_base {
public:
    gui_samplesloaded() : guictr_base() {
        ctrType = CTR_TYPE_EXPORT;
        setBackgroundRendered(false);
        padding = 0;
        margin  = 0;
    }
    ~gui_samplesloaded() {
        removeGuis();
    }
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
    }
    void layout() {
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    virtual void render(NVGcontext* vg) {
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
