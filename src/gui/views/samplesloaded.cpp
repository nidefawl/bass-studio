#include "gui/container/container.hpp"
#include "gui/container/container_builder.hpp"


class gui_samplesloaded final : public guictr_base {
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

namespace DAW::UI {
    guictr_base* makeGuiSamplesLoaded(create_ctr_t ctxt) {
        return new gui_samplesloaded();
    }
}
