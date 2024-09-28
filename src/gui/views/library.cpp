#include "event.h"
#include "fileio.h"
#include "gui/container/container.h"
#include "gui/container/container_builder.h"
#include "gui/container/container_layout.h"
#include "gui/controls/list.h"
#include "gui/controls/filebrowser.hpp"
#include "gui/controls/textfield.h"
#include "gui/plugin/pluginctr.h"
#include "gui/views/pluginlist.h"
#include "host/daw/mainctrl.h"
#include "host/plugin/base/base-plugin.h"
#include "host/track/track.h"
#include "host/track/trackctr_types.h"
#include "host/track/track_impl.h"
#include "logging.h"
#include "platform.h"
#include "renderresources.h"
#include "str_util.h"
#include "tls.h"
#include "appsettings.h"
#include "gui/track/trackcontent.h"

static const std::vector<String> supportedExtensions = { "project", "tracks", "preset", SUPPORTED_AUDIO_FILE_TYPES, "mid" };

int32_t GetIconFromExtension(const String& path) {
    static const std::map<String, int> extensionToIcon = {
        { "preset", ICON_EFFECT },
        { "tracks", ICON_SYNTH_SMALL },
        { "project", ICON_DAW_EXE },
        { "mp3", ICON_FILE_AUDIO },
        { "flac", ICON_FILE_AUDIO },
        { "mid", ICON_FILE_MIDI },
        { "wav", ICON_FILE_AUDIO },
        { "aif", ICON_FILE_AUDIO },
        { "aiff", ICON_FILE_AUDIO },
        { "ogg", ICON_FILE_AUDIO },
        { "m4a", ICON_FILE_AUDIO },
        { "wma", ICON_FILE_AUDIO },
        { "aac", ICON_FILE_AUDIO },
        { "ac3", ICON_FILE_AUDIO },
        { "amr", ICON_FILE_AUDIO },
        { "au", ICON_FILE_AUDIO },
        { "flac", ICON_FILE_AUDIO },
        { "mka", ICON_FILE_AUDIO },
        { "mp2", ICON_FILE_AUDIO },
        { "mp3", ICON_FILE_AUDIO },
        { "mpc", ICON_FILE_AUDIO },
        { "ogg", ICON_FILE_AUDIO },
        { "ra", ICON_FILE_AUDIO },
        { "tta", ICON_FILE_AUDIO },
        { "wav", ICON_FILE_AUDIO },
        { "wma", ICON_FILE_AUDIO },
    };
    String ext;
    SplitPath(path, nullptr, nullptr, &ext);
    auto it = extensionToIcon.find(ext);
    if (it != extensionToIcon.end()) {
        return it->second;
    }
    return ICON_FILE;
}

String DirNameFromPath(String in) {
    App::Platform::sanitizePathToDirectory(in);
    String pathDirecotry;
    SplitPath(in, &pathDirecotry, nullptr, nullptr);
    String dirNameOnly;
    SplitPath(pathDirecotry, nullptr, &dirNameOnly, nullptr);
    return dirNameOnly;
}

namespace {
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

    bool IsHandledDragDropMouseEvent(const MouseHitEvt& evt, DawCtrl* dawCtrl) {
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT && evt.getDraggedThing()) {
            auto type = evt.getDraggedThing()->getGuiType();
            switch (type) {
                case gui_type::CTR_TYPE_PLUGINS_DRAGGED:
                case gui_type::CTR_TYPE_PLUGIN:
                case gui_type::CTR_TYPE_TRACK_TITLE:
                    return true;
                default:
                    break;
            }
            return false;
        }
        if (evt.type == MOUSE_DRAGDROP_CLIP) {
            auto clipboard = dawCtrl->getDaw()->getDragDropClip();
            switch (clipboard.type) {
                case dragdrop_file_clipboard::TYPE_PLUGIN_PRESET:
                case dragdrop_file_clipboard::TYPE_TRACK_CONTAINER:
                    return true;
                default:
                    break;
            }
        }
        return false;
    }

    bool ExportTrackToFile(DawInstance* daw, track_t* track, const String& exportDir, String& outFilename) {
        track_snapshot_t snapshot(track, tracksnapshot_store_opts_t::All());
        trackcontainer_snapshot_t trackContainerSnapshot;
        trackContainerSnapshot.tracks.push_back(snapshot);
        auto exportFilename = track->name + "." + FILE_TYPES_TRACKSNAPSHOT.types.front().ext;
        String path = exportDir + FILE_PATHSEP_STR + exportFilename;
        auto [pathFile, nameFile] = daw->createUniqueNonExistingFilename(path);
        outFilename = pathFile;
        bool bStatus = saveTrackContainer(trackContainerSnapshot, pathFile);
        return bStatus;
    }

} // namespace

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
    const String& getPathAbs() const {
        return path;
    }
    const String& getDisplayName() const {
        return displayName;
    }
    String getText() override {
        return displayName;
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (this->contains(mpos) && IsHandledDragDropMouseEvent(evt, dawCtrl)) {
            evt.requestFocus(this);
            return true;
        }
        return gui_list_entry::mouseHitTest(mpos, evt);
    }

    void trackEntryDragMove(gui_track* g, ivec2 mousepos) override {
        auto clipboard = dawCtrl->getDaw()->getDragDropClip();
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{
            dragdrop_target_indicator_t::target_area,
            -2,
            this,
            { -1, -1 }
        };
        clipboard.isValidTarget = true;
        clipboard.target = makeSafeRef();
    }
    void trackEntryDragRelease(gui_track* g, ivec2 mousepos) override;
    void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override {
    }
    void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override {
    }
    void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) override {
    }
    void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) override {
    }
    bool clipDropMove(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return true;
    }
    bool clipDropFinal(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return true;
    }
};

class gui_user_library_browser_filebrowser_folder_entry : public gui_filebrowser_folder_entry {
public:
    explicit gui_user_library_browser_filebrowser_folder_entry(const FileFound& f, bool bIsOpened)
        : gui_filebrowser_folder_entry(f, bIsOpened) {
        icon = ICON_FOLDER;
    }
    void trackEntryDragMove(gui_track* g, ivec2 mousepos) override {
        auto clipboard = dawCtrl->getDaw()->getDragDropClip();
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{
            dragdrop_target_indicator_t::target_area,
            -2,
            this,
            { -1, -1 }
        };
        clipboard.isValidTarget = true;
        clipboard.target = makeSafeRef();
    }
    void trackEntryDragRelease(gui_track* g, ivec2 mousepos) override {
        String pathFile;
        if (ExportTrackToFile(dawCtrl->getDaw(), g->getTrack(), getPathAbs(), pathFile)) {
            if (!pathFile.empty()) {
                auto popupPos = this->toScreenSpace(ivec2(0));
                auto popupSize = this->size;
                auto dawCtrl = this->dawCtrl;
                auto daw = dawCtrl->getDaw();
                daw->refreshAllUserlibraryBrowsers(); // this call may delete this instance
                DAW::OpenRenameAbsoluteFilePopup(dawCtrl, popupPos, popupSize, pathFile, [daw](const String& path) {
                    daw->refreshAllUserlibraryBrowsers();
                    return true;
                });
            }
        } else {
            log_lf(Log::L_WARN, "Failed to export track to file %s\n", StringAsCStr(pathFile));
        }
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (this->contains(mpos) && IsHandledDragDropMouseEvent(evt, dawCtrl)) {
            evt.requestFocus(this);
            return true;
        }
        return gui_filebrowser_folder_entry::mouseHitTest(mpos, evt);
    }
};

namespace DAW {
    void UserLibraryAddPath(const String& path) {
        String pathSanitized = path;
        App::Platform::sanitizePathToDirectory(pathSanitized);
        auto& tls = daw_tls::getTls();
        auto& settings = *tls.settings;
        if (std::find(settings.userLibraryPaths.begin(), settings.userLibraryPaths.end(), pathSanitized) == settings.userLibraryPaths.end()) {
            settings.userLibraryPaths.push_back(pathSanitized);
            saveSettings(settings);
        }
    }
    void UserLibraryRemovePath(const String& path) {
        auto& tls = daw_tls::getTls();
        auto& settings = *tls.settings;
        settings.userLibraryPaths.erase(std::remove(settings.userLibraryPaths.begin(), settings.userLibraryPaths.end(), path), settings.userLibraryPaths.end());
        saveSettings(settings);
    }
} // namespace DAW

class gui_user_library_path_list final : public gui_list {
public:
    gui_user_library_path_list() : gui_list() {
        setGuiType(gui_type::CTR_TYPE_USERLIBRARY_BROWSER_PATH_LIST);
    }
    bool clipDropMove(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return clip.type == dragdrop_file_clipboard::TYPE_DIRECTORY;
    }
    bool clipDropFinal(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override;
    bool handleKeyInput(KeyEvent& kevt) override {
        if (kevt.cmd) {
            auto temp = kevt.cmd->getKeybindContextData(kevt);
            if (handleEditorCommand(temp)) {
                return true;
            }
            return false;
        }
        return gui_list::handleKeyInput(kevt);
    }
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);
    void buttonClicked(guibase* button) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (this->contains(mpos)) {
            if (evt.type == MOUSE_DRAGDROP_CLIP) {
                auto clipboard = dawCtrl->getDaw()->getDragDropClip();
                if (clipboard.type == dragdrop_file_clipboard::TYPE_DIRECTORY) {
                    evt.requestFocus(this);
                    return true;
                }
            }
        }
        return gui_list::mouseHitTest(mpos, evt);
    }
};

class gui_user_library_browser_filebrowser final : public guictr_filebrowser {
public:
    gui_user_library_browser_filebrowser() : guictr_filebrowser() {
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        bool bHit = guictr_filebrowser::mouseHitTest(mpos, evt);
        if (!bHit && this->contains(mpos) && IsHandledDragDropMouseEvent(evt, dawCtrl)) {
            evt.requestFocus(this);
            return true;
        }
        return bHit;
    }
    gui_filebrowser_folder_entry* createFileBrowserFolderEntry(const FileFound& f, bool _bIsOpened) override {
        return new gui_user_library_browser_filebrowser_folder_entry(f, _bIsOpened);
    }

    gui_filebrowser_file_entry_t* createFileBrowserFileEntry(const FileFound& f) override {
        return new gui_filebrowser_file_entry_t(f);
    }

    void trackEntryDragMove(gui_track* g, ivec2 mousepos) override {
        auto clipboard = dawCtrl->getDaw()->getDragDropClip();
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{
            dragdrop_target_indicator_t::target_area,
            -2,
            this,
            { -1, -1 }
        };
        clipboard.isValidTarget = true;
        clipboard.target = makeSafeRef();
    }
    void trackEntryDragRelease(gui_track* g, ivec2 mousepos) override {
        String pathFile;
        if (ExportTrackToFile(dawCtrl->getDaw(), g->getTrack(), getWorkingDirAbsPath(), pathFile)) {
            if (!pathFile.empty()) {
                auto popupPos = this->toScreenSpace(ivec2(0));
                auto popupSize = this->size;
                auto dawCtrl = this->dawCtrl;
                auto daw = dawCtrl->getDaw();
                daw->refreshAllUserlibraryBrowsers(); // this call may delete this instance
                DAW::OpenRenameAbsoluteFilePopup(dawCtrl, popupPos, popupSize, pathFile, [daw](const String& path) {
                    daw->refreshAllUserlibraryBrowsers();
                    return true;
                });
            }
        } else {
            log_lf(Log::L_WARN, "Failed to export track to file %s\n", StringAsCStr(pathFile));
        }
    }
    void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override {
    }
    void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override {
    }
    void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) override {
    }
    void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) override {
    }
    bool clipDropMove(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return true;
    }
    bool clipDropFinal(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return true;
    }
};

class gui_user_library_browser final : public guictr_stacked {
    gui_user_library_path_list ctr_folders_list;
    gui_user_library_browser_filebrowser ctr_filebrowser;

public:
    gui_user_library_browser() : guictr_stacked() {
        setGuiType(gui_type::CTR_TYPE_USERLIBRARY_BROWSER);
        setVerticalLayout(true);
        addEntry(&ctr_folders_list);
        addEntry(&ctr_filebrowser);
        setSplitters({ 0.25f });
    }
    ~gui_user_library_browser() override {
        removeGuis();
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        updateList();
    }
    void buttonClicked(guibase* button) override {
        if (button->parent == &ctr_folders_list) {
            updateList();
        }
    }
    void layout() override {
        ctr_folders_list.setRowHeight(theme->get(GuiConstant::CONST_ROW_HEIGHT));
        ctr_filebrowser.setRowHeight(theme->get(GuiConstant::CONST_ROW_HEIGHT));
        guictr_stacked::layout();
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        bool bHit = guictr_stacked::mouseHitTest(mpos, evt);
        if (!bHit && this->contains(mpos) && IsHandledDragDropMouseEvent(evt, dawCtrl)) {
            evt.requestFocus(this);
            return true;
        }
        return bHit;
    }

    void trackEntryDragMove(gui_track* g, ivec2 mousepos) override {
    }
    void trackEntryDragRelease(gui_track* g, ivec2 mousepos) override {
    }
    void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override {
    }
    void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override {
    }
    void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) override {
    }
    void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) override {
    }
    bool clipDropMove(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return true;
    }
    bool clipDropFinal(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return true;
    }

    void updateList() {
        setupUserDefaultLibrary();
        auto& tls          = daw_tls::getTls();
        auto& userLibPaths = tls.settings->userLibraryPaths;

        // store selected entry, so we can restore it after updating the list
        // TODO: restoring the focused entry should be handled by the list internally
        String pathAbsSelectedEntry;
        bool bRestoreFocused = false;
        auto entry = ctr_folders_list.getSelectedEntry();
        if (entry) {
            pathAbsSelectedEntry = static_cast<gui_userlibrary_list_entry_t*>(entry)->getPathAbs();
            auto focusedGui = parentCtrl->getGuiFocused();
            if (focusedGui == entry) {
                bRestoreFocused = true;
            }
        }
        std::vector<gui_list_entry*> _newList;
        _newList.reserve(userLibPaths.size());
        for (auto& path : userLibPaths) {
            _newList.push_back(new gui_userlibrary_list_entry_t({ path, DirNameFromPath(path), "" }));
        }
        ctr_folders_list.setList(_newList);
        // restore selected entry
        if (!pathAbsSelectedEntry.empty()) {
            auto it = std::find_if(_newList.begin(), _newList.end(), [pathAbsSelectedEntry](gui_list_entry* e) {
                return static_cast<gui_userlibrary_list_entry_t*>(e)->getPathAbs() == pathAbsSelectedEntry;
            });
            if (it != _newList.end()) {
                auto idx = int32_t(it - _newList.begin());
                ctr_folders_list.setSelectedIdx(idx);
                if (bRestoreFocused) {
                    parentCtrl->focusGui(*it);
                }
            }
        }
        if (ctr_folders_list.getSelectedIdx() < 0) {
            ctr_folders_list.setSelectedIdx(0);
        }
        // get selected folder
        const auto selectedEntry = dynamic_cast<gui_userlibrary_list_entry_t*>(ctr_folders_list.getSelectedEntry());
        if (selectedEntry) {
            ctr_filebrowser.setWorkingDir(selectedEntry->path);
            ctr_filebrowser.setFileExtensions(supportedExtensions);
            ctr_filebrowser.updateList();
        } else {
            ctr_filebrowser.setList({});
        }
    }
};

class gui_user_library_search final : public guictr_base {
    gui_textfield textField;
    guictr_filesearch ctr_filesearch;
    String curquery = "";
public:
    gui_user_library_search() : guictr_base() {
        setGuiType(gui_type::CTR_TYPE_USERLIBRARY_SEARCH);
        padding = 4;
        margin = 2;
        // setVerticalLayout(true);
        add(&textField);
        add(&ctr_filesearch);
        gui_list pluginListCtr;
        textField.setChangeCallback([this](const std::string& str) {
            curquery = str;
            update();
            return true;
        });
        textField.setPlaceholder("Search");
    }
    ~gui_user_library_search() override {
        removeGuis();
    }
    void layout() override {
        auto rowHeight = theme->get(GuiConstant::CONST_ROW_HEIGHT);
        ctr_filesearch.setRowHeight(rowHeight);
        ivec2 cs           = getSizeContent();
        textField.size     = ivec2(cs.x, rowHeight);
        textField.pos      = ivec2(0, 0);
        ctr_filesearch.pos  = ivec2(0, textField.bottom()+padding);
        ctr_filesearch.size = ivec2(cs.x, cs.y - ctr_filesearch.top());
        for (guibase* gui : guis) {
            gui->layout();
        }
    }

    void update() {
        ctr_filesearch.resetResults();
        if (curquery.empty()) {
            return;
        }
        std::vector<String> directories;
        std::vector<String> searchTerms;
        searchTerms.push_back(curquery);
        auto& tls = daw_tls::getTls();
        auto& userLibPaths = tls.settings->userLibraryPaths;
        directories.reserve(userLibPaths.size());
        for (auto& path : userLibPaths) {
            directories.push_back(path);
        }
        ctr_filesearch.search(directories, supportedExtensions, searchTerms);
        layout();
    }
};

void gui_userlibrary_list_entry_t::trackEntryDragRelease(gui_track* g, ivec2 mousepos) {
    String pathFile;
    if (ExportTrackToFile(dawCtrl->getDaw(), g->getTrack(), getPathAbs(), pathFile)) {
        if (!pathFile.empty()) {
            auto popupPos = this->toScreenSpace(ivec2(0));
            auto popupSize = this->size;
            auto dawCtrl = this->dawCtrl;
            auto daw = dawCtrl->getDaw();
            parent->buttonClicked(this);
            // daw->refreshAllUserlibraryBrowsers(); // this call may delete this instance
            DAW::OpenRenameAbsoluteFilePopup(dawCtrl, popupPos, popupSize, pathFile, [daw](const String& path) {
                daw->refreshAllUserlibraryBrowsers();
                return true;
            });
        }
    } else {
        log_lf(Log::L_WARN, "Failed to export track to file %s\n", StringAsCStr(pathFile));
    }
}

bool gui_user_library_path_list::clipDropFinal(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) {
    auto parent = guiParentType<gui_user_library_browser, gui_type::CTR_TYPE_USERLIBRARY_BROWSER>(this);
    if (clip.type == dragdrop_file_clipboard::TYPE_DIRECTORY && parent) {
        DAW::UserLibraryAddPath(clip.path);
        parent->updateList();
        return true;
    }
    return false;
}

bool gui_user_library_path_list::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    auto parent = guiParentType<gui_user_library_browser, gui_type::CTR_TYPE_USERLIBRARY_BROWSER>(this);
    auto command = ctxt.type;
    auto& kevt = ctxt.kevt;
    if (parent && focused())
    {
        if (kevt.type != K_RELEASE) {
            if (kevt.type == K_PRESS) {
                if (command == CMD_DELETE) {
                    auto selectedEntry = dynamic_cast<gui_userlibrary_list_entry_t*>(getSelectedEntry());
                    if (selectedEntry) {
                        DAW::UserLibraryRemovePath(selectedEntry->path);
                        parent->updateList();
                        return true;
                    }
                }
            }
        }
        return true;
    }
    return false;
}

void gui_user_library_path_list::buttonClicked(guibase* button) {
    gui_list::buttonClicked(button);
}

namespace DAW::UI {
    guictr_base* makeGuiUserLibraryBrowser(create_ctr_t ctxt) {
        return new gui_user_library_browser();
    }
    guictr_base* makeGuiUserLibrarySearch(create_ctr_t ctxt) {
        return new gui_user_library_search();
    }
}// namespace DAW::UI

class guictr_effectlibrary final : public guictr_stacked {
public:
    guictr_pluginlibrary ctr_pluginlist;
    guictr_modulelibrary ctr_effectlist;
    bool initialized = false;
    int revision     = -1;
    guictr_effectlibrary() : guictr_stacked() {
        setGuiType(gui_type::CTR_TYPE_EFFECTLIBRARY);
        setVerticalLayout(true);
        setBackgroundRendered(false);
        padding = 0;
        margin  = 0;
        addEntry(&ctr_pluginlist);
        addEntry(&ctr_effectlist);
        setSplitters({ 0.5f });
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
        guictr_stacked::buttonClicked(button);
    }
};

namespace DAW::UI {
    guictr_base* makeGuiEffectLibrary(create_ctr_t ctxt) {
        return new guictr_effectlibrary();
    }
}// namespace DAW::UI

void DawCtrl::refreshAllUserlibraryBrowsers() {
    visitGuis(gui_type::CTR_TYPE_USERLIBRARY_BROWSER, [](guictr_base* ctr) {
        auto list = gui_cast<gui_user_library_browser, gui_type::CTR_TYPE_USERLIBRARY_BROWSER>(ctr);
        if (list) {
            list->updateList();
        }
    });
}
