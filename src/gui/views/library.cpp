#include "gui/container/container.h"
#include "gui/container/container_builder.h"
#include "gui/controls/list.h"
#include "gui/controls/filebrowser.hpp"
#include "gui/plugin/pluginctr.h"
#include "gui/views/pluginlist.h"
#include "host/plugin/base/base-plugin.h"
#include "host/track/trackctr_types.h"
#include "host/track/track_impl.h"
#include "renderresources.h"
#include "str_util.h"
#include "tls.h"
#include "appsettings.h"

void setupUserDefaultLibrary() {
    auto& tls = daw_tls::getTls();
    if (!assert_expr(tls.settings)) {
        return;
    }
    auto& settings = *tls.settings;
    if (settings.userLibraryPaths.empty()) {
        String path = App::Platform::toUserdataPath("");
        if (!FileExists(path)) {
            CreateDirectoryIfNotExists(path);
        }
        settings.userLibraryPaths.push_back(path);
        saveSettings(settings);
    }
}

class gui_userlibrary_list_entry_t final : public gui_list_entry {
public:
    String path;
    String displayName;
    explicit gui_userlibrary_list_entry_t(const FileFound& f)
        : gui_list_entry(), path(f.path), displayName(f.name) {
        icon = ICON_FOLDER;
        setTooltipText(f.path);
    }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {
    }
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
    }
    String getText() override {
        return displayName;
    }
};

class gui_user_library final : public guictr_base {
    gui_list ctr_folders_list;
    guictr_filebrowser ctr_filebrowser;

public:
    gui_user_library() : guictr_base() {
        setGuiType(gui_type::CTR_TYPE_USERLIBRARY);
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        setBackgroundRendered(false);
        padding = 0;
        margin  = 0;
        add(&ctr_folders_list);
        add(&ctr_filebrowser);
    }
    ~gui_user_library() override {
        removeGuis();
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        updateList();
    }
    void buttonClicked(guibase* button) override {
        guictr_base::buttonClicked(button);
    }
    void updateList() {
        setupUserDefaultLibrary();
        auto& tls          = daw_tls::getTls();
        auto& userLibPaths = tls.settings->userLibraryPaths;
        std::vector<gui_list_entry*> _newList;
        _newList.reserve(userLibPaths.size());
        for (auto& path : userLibPaths) {
            _newList.push_back(new gui_userlibrary_list_entry_t({ path, FileNameFromPath(path), "" }));
        }
        ctr_folders_list.setList(_newList);
        if (ctr_folders_list.getSelectedIdx() < 0) {
            ctr_folders_list.setSelectedIdx(0);
        }
        // get selected folder
        const auto selectedEntry = dynamic_cast<gui_userlibrary_list_entry_t*>(ctr_folders_list.getSelectedEntry());
        if (selectedEntry) {

            const std::vector<String> supportedExtensions = { "project", "track", "preset", SUPPORTED_AUDIO_FILE_TYPES, "mid" };
            ctr_filebrowser.setWorkingDir(selectedEntry->path);
            ctr_filebrowser.setFileExtensions(supportedExtensions);
            ctr_filebrowser.updateList();
        }
    }
};

namespace DAW::UI {
    guictr_base* makeGuiUserLibrary(create_ctr_t ctxt) {
        return new gui_user_library();
    }
}// namespace DAW::UI

class guictr_effectlibrary final : public guictr_base {
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
    void buttonClicked(guibase* button) override {
        if (parentCtrl->lastMouseEvent.type == MouseEventType::M_EVT_DOUBLECLICK) {
            auto track = dawCtrl->getSelectedTrack();
            if (!track)
                return;
            auto guiListEntry = gui_cast<gui_pluginlist_entry, gui_type::CTR_TYPE_PLUGINS_LIST_ENTRY>(button);
            if (!guiListEntry)
                return;
            auto daw = dawCtrl->getDaw();
            dawCtrl->showPluginView();
            auto& dragDropTarget = dawCtrl->getDragDropTarget();
            dragDropTarget.reset();
            auto dstStage      = track->getStage();
            effectbase* effect = guiListEntry->makeInstance();
            if (effect) {
                ThreadLock lock = daw->lockPlayThread();
                int32_t dstSlot = effect->isSynth ? 0 : CtrSize(dstStage->effects);
                daw->getPluginManager()->insertNewPlugin(dstStage, effect, dstSlot);
                effect->onEnable();
                daw->pushHist(new action_insert_effect("Insert plugin", effect, dstStage->toRef(), dstSlot));
                daw->onPluginsChanged();
                for (auto& gui : dstStage->gui) {
                    gui->scrollToPluginGui(effect);
                }
            }
            return;
        }
        guictr_base::buttonClicked(button);
    }
};

namespace DAW::UI {
    guictr_base* makeGuiEffectLibrary(create_ctr_t ctxt) {
        return new guictr_effectlibrary();
    }
}// namespace DAW::UI
