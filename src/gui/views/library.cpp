#include "gui/container/container.h"
#include "gui/container/container_builder.h"
#include "gui/views/pluginlist.h"


class gui_library : public guictr_base {
public:
    gui_library() : guictr_base() {
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

namespace DAW::UI {
    guictr_base* makeGuiLibrary(create_ctr_t ctxt) {
        return new gui_library();
    }
}

class guictr_effectlibrary : public guictr_base {
public:
    guictr_pluginlibrary ctr_pluginlist;
    guictr_modulelibrary ctr_effectlist;
    bool initialized = false;
    int revision     = -1;
    guictr_effectlibrary() : guictr_base() {
        setGuiType(gui_type::CTR_TYPE_EFFECTLIBRARY);
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        setBackgroundRendered(false);
        padding = 0;
        margin  = 0;
        add(&ctr_pluginlist);
        add(&ctr_effectlist);
    }

    ~guictr_effectlibrary() override {
        removeGuis();
    }

    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
        if (parent && !initialized) {
            initialized = true;
            update();
        }
        if (dawCtrl && dawCtrl->getDaw()->getPluginDatabase().getRevision() != this->revision) {
            update();
        }
    }

    void update() {
        ctr_pluginlist.update();
        ctr_effectlist.update();
        if (dawCtrl) {
            this->revision = dawCtrl->getDaw()->getPluginDatabase().getRevision();
        }
    }
};

namespace DAW::UI {
guictr_base* makeGuiEffectLibrary(create_ctr_t ctxt) {
    return new guictr_effectlibrary();
}
}
