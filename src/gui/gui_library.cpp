#include "guicontainer.h"


class gui_library : public guictr_base {
public:
    gui_library() : guictr_base() {
        ctrType = CTR_TYPE_EXPORT;
        setBackgroundRendered(false);
        padding = 0;
        margin  = 0;
    }
    ~gui_library() override {
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


guictr_base* makeGuiLibrary() {
    return new gui_library();
}
