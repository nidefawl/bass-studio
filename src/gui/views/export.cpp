#include "gui/container/container_builder.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "guicolors.h"
#include "gui/controls/button.h"
#include "controls.h"
#include "host/daw/mainctrl.h"
#include "host/host_pluginmanager.h"
#include "host/audiohost/audio_host.h"

namespace {
    constexpr int TEXT_FONT_SIZE = 20;
    const SupportedFileType FILE_TYPE_EXPORT{ "*.wav", "wav" };
    std::vector<SupportedFileType> vFILE_TYPE_EXPORT = { FILE_TYPE_EXPORT };
}// namespace
class gui_export;
class guictr_timeframe : public guictr_base {
    friend class gui_export;
    gui_timeinput tmTickStart;
    gui_timeinput tmTickLen;
    bool* const pIsLocked;
    guibutton btnLock;

public:
    guictr_timeframe(tick_t* s, tick_t* d, bool* l)
        : guictr_base(),
          tmTickStart(s),
          tmTickLen(true),
          pIsLocked(l) {
        tmTickLen.setRef(toRef(), d);
        padding = 0;
        margin  = 0;
        tmTickStart.setLabel("Start");
        tmTickLen.setLabel("Length");

        btnLock.setLabel("Lock Start and Length");
        btnLock.drawFn   = drawTextureSymbol;
        btnLock.drawParm = ICON_OPT_UNLOCKED;
        btnLock.pos      = ivec2(INSET_CTR_SPACING, INSET_CTR_SPACING);
        setCanMouseHit(true);
        add(&tmTickStart);
        add(&tmTickLen);
        add(&btnLock);
    }
    ~guictr_timeframe() override {
        removeGuis();
    }
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
        btnLock.drawParm = *pIsLocked ? ICON_OPT_LOCKED : ICON_OPT_UNLOCKED;
    }
    void layout() override {

        ivec2 cs      = getSizeContent();
        int32_t inset = INSET_CTR_SPACING;
        inset         = 5;

        int32_t heightEntry      = 20;
        int32_t widthLock        = heightEntry;
        int32_t widthStartAndLen = cs.x - inset * 2 - widthLock;
        int32_t widthSingle      = widthStartAndLen / 2;

        tmTickStart.pos  = ivec2(widthSingle * 1 / 3, 0) + ivec2(inset);
        tmTickLen.pos    = ivec2(widthSingle + widthSingle * 1 / 3, 0) + ivec2(inset);
        tmTickStart.size = ivec2(widthSingle - tmTickStart.left(), heightEntry);
        tmTickLen.size   = tmTickStart.size;
        btnLock.pos      = ivec2(widthStartAndLen, 0) + ivec2(inset);
        btnLock.size     = ivec2(widthLock);
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

        setFont(vg, TEXT_FONT_SIZE, THEMECOL_TEXT, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
        nvgText(vg, 0, this->tmTickStart.bottom(), StringAsCStr(this->tmTickStart.getLabel()), NULL);
        nvgText(vg, this->tmTickStart.right() + 10, this->tmTickLen.bottom(), StringAsCStr(this->tmTickLen.getLabel()), NULL);


        for (auto* g : guis) {
            nvgSave(vg);
            g->render(vg);
            nvgRestore(vg);
        }
    }

    void buttonClicked(guibase* button) override {
        if (button == &btnLock) {
            *pIsLocked       = !*pIsLocked;
            btnLock.drawParm = *pIsLocked ? ICON_OPT_LOCKED : ICON_OPT_UNLOCKED;
        }
    }
    bool isLocked() const {
        return *pIsLocked;
    }
};
class gui_export : public guictr_base {
    export_settings_t& settings;
    guictr_timeframe tmFrameExport;
    guibutton btnExport;
    guibutton selectFolder;

public:
    explicit gui_export(export_settings_t& _settings)
        : guictr_base(),
          settings(_settings), tmFrameExport(&settings.exportPos, &settings.exportLen, &settings.isLocked) {
        setGuiType(CTR_TYPE_EXPORT);
        setBackgroundRendered(true);
        selectFolder.id = 0x10;
        selectFolder.setText(settings.exportPath);
        selectFolder.setTooltipText(settings.exportPath);
        selectFolder.setLabel("Path");
        btnExport.id = 0x20;
        btnExport.setLabel("Export");
        btnExport.setText(btnExport.getLabel());


        add(&tmFrameExport);
        add(&selectFolder);
        add(&btnExport);
    }
    ~gui_export() override {
        removeGuis();
    }
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
        selectFolder.setText(settings.exportPath);
        selectFolder.setTooltipText(settings.exportPath);
        if (!tmFrameExport.isLocked()) {
            auto& globals = dawCtrl->getDaw()->getGlobals();
            if (globals.cursor.getRange()) {
                settings.exportPos = globals.cursor.getTickBegin();
                settings.exportLen = globals.cursor.getRange();
            } else if (globals.loopEnabled) {
                settings.exportPos = globals.loopStart;
                settings.exportLen = globals.loopLen;
            }
        }
    }
    void layout() override {
        int32_t inset = INSET_CTR_SPACING;
        ivec2 cs      = getSizeContent();

        int32_t closeSize = 32;

        btnExport.size = ivec2(math::min(cs.x / 2 - inset * 2, closeSize * 3), closeSize);
        btnExport.pos  = ivec2(cs.x - btnExport.size.x + inset, cs.y - inset - btnExport.size.y);


        inset               = 5;
        int32_t buttonW     = math::max(120, cs.x * 2 / 3);
        int32_t height      = 20;
        selectFolder.size   = ivec2(buttonW, height);
        selectFolder.pos    = ivec2(cs.x - inset * 2 - buttonW, inset);
        tmFrameExport.size  = ivec2(cs.x, height * 3);
        tmFrameExport.pos   = ivec2(0, selectFolder.bottom() + inset);
        selectFolder.pos.x  = tmFrameExport.tmTickStart.pos.x;
        selectFolder.size.x = cs.x - selectFolder.pos.x;
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

        float lineh;
        setFont(vg, TEXT_FONT_SIZE, THEMECOL_TEXT, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
        nvgTextMetrics(vg, NULL, NULL, &lineh);
        nvgText(vg, 5, this->selectFolder.bottom(), StringAsCStr(this->selectFolder.getLabel()), NULL);

        for (auto* g : guis) {
            nvgSave(vg);
            g->render(vg);
            nvgRestore(vg);
        }
    }
    void promptExportPath() {
        selectFolder.setText(settings.exportPath);
        //select folder
        String lastPath = settings.exportPath;
        App::Platform::sanitizePathToDirectory(lastPath);
        auto window = parentCtrl->window;

        String path = lastPath;
        if (promptUserFilePath(window, 1, vFILE_TYPE_EXPORT, path)) {
            settings.exportPath = path;
        } else {
            settings.exportPath = "";
        }
        selectFolder.setText(settings.exportPath);
        selectFolder.setTooltipText(settings.exportPath);
    }
    void buttonClicked(guibase* button) override {

        if (button->id == 0x10) {
            promptExportPath();
            return;
        }


        if (button->id == 0x20) {
            if (settings.exportPath.empty()) {
                promptExportPath();
            }
            if (!settings.exportPath.empty()) {
                dawCtrl->getDaw()->startExport();
            }
            return;
        }
    }
};


namespace DAW::UI {
    guictr_base* makeGuiExport(create_ctr_t ctxt) {
        dbgassert(ctxt.daw);
        auto& settings = ctxt.daw->getExportSettings();
        return new gui_export(settings);
    }
}
