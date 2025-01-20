#include "controls.hpp"
#include "gui/gui.hpp"
#include "gui/container/container.hpp"
#include "gui/container/container_builder.hpp"
#include "gui/controls/button.hpp"
#include "host/daw/mainctrl.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/track/track_impl.hpp"
#include "seq_time.hpp"


namespace DAW {
    struct export_project_task final : public async_task_t {
        DawInstance* daw;
        export_settings_t settings;
        export_project_task(DawInstance* daw)
            : daw(daw)
            , settings(daw->getExportSettings()) {
        }
        String getTaskName() const override {
            return "Rendering Audio";
        }
        String getProgressDesc() const override {
            return "";
        }
        void run() override {
            switch (m_state) {
                case state::idle:
                    m_state = state::running;
                    break;
                case state::running:
                    if (daw->getPlayThread()->getState() != playback_state::status_render) {
                        m_state = state::finished;
                    }
                    break;
                case state::error:
                case state::finished:
                case state::cancelled:
                    break;
            }
            requestFrame();
        }
        void cancel() override {
            canceled = true;
            daw->stopPlaying();
        }
        void getPreciseProgress(double& progressOverall, double& progressDetail) override {
            progressDetail  = -1;
            auto tick = daw->getPlaybackPos() - settings.exportPos;
            progressOverall = tick / double(settings.exportLen);
        }
    };
}

namespace {
    constexpr int TEXT_FONT_SIZE = 20;
    SupportedFileTypes FILE_TYPES_EXPORT = SupportedFileTypes{"Wave File", { SupportedFileType{ "*.wav", "wav" } } };
}// namespace
class gui_export;
class guictr_timeframe final : public guictr_base {
    friend class gui_export;
    gui_timeinput tmTickStart;
    gui_timeinput tmTickLen;
    bool* const pIsLocked;
    guibutton btnLock;

public:
    guictr_timeframe(tick_t* s, tick_t* d, bool* l)
        : guictr_base(),
          tmTickStart(false),
          tmTickLen(true),
          pIsLocked(l) {
        tmTickStart.setRef(toRef(), s);
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
        }
        if (button == &tmTickStart || button == &tmTickLen) {
            *pIsLocked = true;
        }
        btnLock.drawParm = *pIsLocked ? ICON_OPT_LOCKED : ICON_OPT_UNLOCKED;
    }
    bool isLocked() const {
        return *pIsLocked;
    }
};
class gui_export final : public guictr_base {
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

        btnExport.size = ivec2(math::min(cs.x / 2, closeSize * 3), closeSize);
        btnExport.pos  = ivec2(cs.x - inset - btnExport.size.x, cs.y - inset - btnExport.size.y);


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
        if (promptUserFilePath(window, 1, FILE_TYPES_EXPORT, path)) {
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
                // TODO: handle this inside export_project_task
                dawCtrl->getDaw()->setAudioThreadState(playback_state::status_no_process);
                for (auto* trackMaster : dawCtrl->getDaw()->getTracks().getMasterTracksFlatVecRef()) {
                    trackMaster->getStage()->flags |= audiostageflags_t::RECORD_OUTPUT;
                }
                dawCtrl->getDaw()->setAsyncTask(new DAW::export_project_task(dawCtrl->getDaw()));
                dawCtrl->getDaw()->startExport();
                if (parent) {
                    parent->buttonClicked(button);
                }
            }
        }
    }
};

class guidialog_export final : public guidialog_base {
    guictr_base* ctrExport;
public:
    guidialog_export(DawInstance* daw) 
        : guidialog_base(ivec2{520, 200}),
        ctrExport(new gui_export(daw->getExportSettings()))
    {
        padding = 2;
        margin = 0;
        setLabel(ctrExport->getLabel());
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        add(ctrExport);
    }
    ~guidialog_export() override {
        removeGuis();
        delete ctrExport;
    }
    void buttonClicked(guibase* button) override {
        if (button->id == 0x20) {
            closeContextMenu();
        }
    }
};


namespace DAW::UI {
    guidialog_base* makeGuiExportDialog(create_ctr_t ctxt) {
        return new guidialog_export(ctxt.daw);
    }
    guictr_base* makeGuiExport(create_ctr_t ctxt) {
        dbgassert(ctxt.daw);
        auto& settings = ctxt.daw->getExportSettings();
        return new gui_export(settings);
    }
}
