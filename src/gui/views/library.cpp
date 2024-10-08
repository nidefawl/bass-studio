#include "basectrl.h"
#include "event.h"
#include "fileio.h"
#include "fileloader.h"
#include "gui/container/container.h"
#include "gui/container/container_builder.h"
#include "gui/container/container_layout.h"
#include "gui/controls/list.h"
#include "gui/controls/button.h"
#include "gui/controls/filebrowser.hpp"
#include "gui/controls/textfield.h"
#include "gui/plugin/pluginctr.h"
#include "gui/views/controls.h"
#include "gui/views/pluginlist.h"
#include "host/clip/clip.h"
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
static const std::vector<String> supportedExtensions = { "project", "tracks", "preset", SUPPORTED_AUDIO_FILE_TYPES, "mid" };

int32_t GetIconFromExtension(const String& path) {
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
                case gui_type::CTR_TYPE_TRACK_TITLE:
                case gui_type::GUI_TYPE_CLIP:
                    return true;
                default:
                    break;
            }
            return false;
        }
        if (evt.type == MOUSE_DRAGDROP_FILE) {
            auto clipboard = dawCtrl->getDaw()->getDragDropClip();
            switch (clipboard.type) {
                case dragdrop_file::TYPE_PLUGIN_PRESET:
                case dragdrop_file::TYPE_TRACK_CONTAINER:
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

    bool ExportPluginSnapshotToFile(DawInstance* daw, effectbase* effect, const String& exportDir, String& outFilename) {
        ThreadLock lock = daw->lockPlayThread();
        plugin_snapshot_t ps;
        effect->makeSnapshot(ps, tracksnapshot_store_opts_t::All());
        String presetName = ps.currentProgramName;
        if (presetName.empty()) {
            presetName = effect->getName();
        }
        String path = exportDir + FILE_PATHSEP_STR + presetName;
        String ext;
        SplitPath(path, nullptr, nullptr, &ext);
        if (ext.empty()) {
            path += ".";
            path += FILE_TYPES_PLUGINSNAPSHOT.types.front().ext;
        }
        auto [pathFile, nameFile] = daw->createUniqueNonExistingFilename(path);
        // save file first, then spawn popup to rename
        bool bStatus = savePluginSnapshot(ps, pathFile);
        outFilename = pathFile;
        return bStatus;
    }

    bool ExportClipToFile(DawInstance* daw, clip_t* clip, const String& exportDir, String& outFilename) {
        dbgassert(clip->clipType == CLIP_MIDI || clip->clipType == CLIP_AUDIO);
        ThreadLock lock = daw->lockPlayThread();
        String presetName = clip->name;
        if (presetName.empty()) {
            presetName = clip->clipType == CLIP_MIDI ? "Midi" : "Audio";
        }
        String path = exportDir + FILE_PATHSEP_STR + presetName;
        String ext;
        SplitPath(path, nullptr, nullptr, &ext);
        if (ext.empty()) {
            if (clip->clipType == CLIP_MIDI) {
                path += ".mid";
            } else if (clip->clipType == CLIP_AUDIO) {
                path += ".wav";
            }
        }

        auto [pathFile, nameFile] = daw->createUniqueNonExistingFilename(path);
        // save file first, then spawn popup to rename
        outFilename = pathFile;
        if (clip->clipType == CLIP_MIDI) {
            auto track = std::make_shared<track_clipboard_t>();
            track->clips.push_back(std::make_shared<clip_t>(*clip));
            clip_clipboard clipboard;
            clipboard.tracks.push_back(track);
            return DAW::SaveMidiFile(pathFile, clipboard);
        } else {
            // save audio clip
            auto cache = daw->getAudioCache();
            audiofile_t* c = cache->getDerivedSample(clip->audio);
            return c && DAW::SaveSampleToFile(*c, pathFile) > 0;
        }
        return false;
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
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
    }
    void trackEntryDragRelease(gui_track* g, ivec2 mousepos) override;
    void clipDragMove(gui_clip* g, ivec2 mousepos) override {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
    }
    void clipDragRelease(gui_clip* g, ivec2 mousepos) override {
    }
    void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
    }
    void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override {
    }
    void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) override {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
    }
    void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) override {
        auto effect = g->effects.empty() ? nullptr : g->effects.front();
        String pathFile;
        if (effect && ExportPluginSnapshotToFile(dawCtrl->getDaw(), effect, getPathAbs(), pathFile)) {
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
            log_lf(Log::L_WARN, "Failed to export plugin to file %s\n", StringAsCStr(pathFile));
        }
    }
    bool fileDropMove(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return true;
    }
    bool fileDropRelease(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return true;
    }

    void handleDragDropHover(MouseHitEvt& mouseHit) override {
        parent->buttonClicked(this);
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
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
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
    void clipDragMove(gui_clip* g, ivec2 mousepos) override {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
    }
    void clipDragRelease(gui_clip* g, ivec2 mousepos) override {
    }
    void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
    }
    void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override {
    }
    void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) override {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
    }
    void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) override {
        auto effect = g->effects.empty() ? nullptr : g->effects.front();
        String pathFile;
        if (effect && ExportPluginSnapshotToFile(dawCtrl->getDaw(), effect, getPathAbs(), pathFile)) {
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
            log_lf(Log::L_WARN, "Failed to export plugin to file %s\n", StringAsCStr(pathFile));
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
    bool fileDropMove(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return clip.type == dragdrop_file::TYPE_DIRECTORY;
    }
    bool fileDropRelease(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) override;
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
            if (evt.type == MOUSE_DRAGDROP_FILE) {
                auto clipboard = dawCtrl->getDaw()->getDragDropClip();
                if (clipboard.type == dragdrop_file::TYPE_DIRECTORY) {
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

    gui_filebrowser_file_entry* createFileBrowserFileEntry(const FileFound& f) override {
        return new gui_filebrowser_file_entry(f);
    }

    void trackEntryDragMove(gui_track* g, ivec2 mousepos) override {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
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
    void clipDragMove(gui_clip* g, ivec2 mousepos) override {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
    }
    void clipDragRelease(gui_clip* g, ivec2 mousepos) override {
        String pathFile;
        if (ExportClipToFile(dawCtrl->getDaw(), g->m_clip, getWorkingDirAbsPath(), pathFile)) {
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
            log_lf(Log::L_WARN, "Failed to export clip to file %s\n", StringAsCStr(pathFile));
        }
    }
    void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
    }
    void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override {
    }
    void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) override {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t::TargetArea(this);
    }
    void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) override {
        auto effect = g->effects.empty() ? nullptr : g->effects.front();
        String pathFile;
        if (effect && ExportPluginSnapshotToFile(dawCtrl->getDaw(), effect, getWorkingDirAbsPath(), pathFile)) {
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
            log_lf(Log::L_WARN, "Failed to export plugin to file %s\n", StringAsCStr(pathFile));
        }
    }
    bool fileDropMove(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return true;
    }
    bool fileDropRelease(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        return true;
    }
};

enum class filter_type {
    ANY, // all files aka .*
    ALL, // all other enum entries after this one
    AUDIO,
    MIDI,
    PROJECT,
    TRACKS,
    PRESET,
};

class guictr_filebrowser_filetype_filter final : public guictr_base {
    class guibutton_select_filter : public guibutton {
    public:
        bool bIsSelected = false;
        int32_t btnIndex = 0;
        int32_t getIndex() const {
            return btnIndex;
        }
        guibutton_select_filter() = default;
        bool getState() const override {
            return bIsSelected;
        }
        void setState(bool b) {
            bIsSelected = b;
        }
    };
    std::array<guibutton_select_filter, 7> btnViews;
    int32_t selectedFilter = 0;
public:
    guictr_filebrowser_filetype_filter() {
        setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        padding = 6;
        margin  = 6;
        snapSides.y = snapSides.w = 1;
        snapSides.x = snapSides.z = 0;
        for (auto& btn : btnViews) {
            btn.btnIndex = static_cast<int32_t>(&btn - btnViews.data());
            btn.drawFn   = drawTextureSymbol;
            String text;
            switch (btn.btnIndex) {
                case 0:
                    text = "Any";
                    btn.drawFn   = nullptr;
                    btn.setText("*");
                    // btn.drawParm = ICON_FILE;
                    break;
                case 1:
                    text = "All";
                    btn.drawParm = ICON_FILE;
                    break;
                case 2:
                    text = "Audio";
                    btn.drawParm = ICON_FILE_AUDIO;
                    break;
                case 3:
                    text = "Midi";
                    btn.drawParm = ICON_FILE_MIDI;
                    break;
                case 4:
                    text = "Project";
                    btn.drawParm = ICON_DAW_EXE;
                    break;
                case 5:
                    text = "Tracks";
                    btn.drawParm = ICON_SYNTH_SMALL;
                    break;
                case 6:
                    text = "Preset";
                    btn.drawParm = ICON_EFFECT;
                    break;
                default:
                    break;
            }
            btn.setTooltipText(text);
            btn.setButtonColor(GuiColor::COL_BTN_BG_SHOW_ACTIVE);
            add(&btn);
        }
        // move "*" to the end
        remove(&btnViews[0]);
        add(&btnViews[0]);
        setActiveFilter(filter_type::ALL);
    }
    ~guictr_filebrowser_filetype_filter() override {
        removeGuis();
    }
    void layout() override {
        auto pos = ivec2(0, 0);
        for (auto& gui : btnViews) {
            gui.pos = pos;
            gui.size = ivec2(size.x / btnViews.size(), size.y);
        }
        guictr_base::layout();
    }
    int32_t getNumButtons() const {
        return CtrSize(btnViews);
    }
    void setActiveFilter(filter_type filter) {
        for (auto& btn : btnViews) {
            if (btn.btnIndex == static_cast<int32_t>(filter)) {
                btn.setState(true);
                selectedFilter = btn.btnIndex;
            } else {
                btn.setState(false);
            }
        }
    }
    void buttonClicked(guibase* button) override {
        for (auto& btn : btnViews) {
            if (&btn == button) {
                btn.setState(true);
                selectedFilter = btn.btnIndex;
            } else {
                btn.setState(false);
            }
        }
        if (parent) {
            parent->buttonClicked(this);
        }
    }
    filter_type getSelectedFilter() const {
        switch (selectedFilter) {
            case 0:
                return filter_type::ANY;
            case 1:
                return filter_type::ALL;
            case 2:
                return filter_type::AUDIO;
            case 3:
                return filter_type::MIDI;
            case 4:
                return filter_type::PROJECT;
            case 5:
                return filter_type::TRACKS;
            case 6:
                return filter_type::PRESET;
            default:
                return filter_type::ALL;
        }
    }
};

class gui_user_library_browser final : public guictr_stacked {
    gui_user_library_path_list ctr_folders_list;
    guictr_filebrowser_filetype_filter ctr_fileTypeFilter;
    gui_user_library_browser_filebrowser ctr_filebrowser;
    bool bRefresh = false;
public:
    gui_user_library_browser() : guictr_stacked() {
        setGuiType(gui_type::CTR_TYPE_USERLIBRARY_BROWSER);
        setVerticalLayout(true);
        addEntry(&ctr_folders_list);
        addEntry(&ctr_fileTypeFilter);
        addEntry(&ctr_filebrowser);
        setSplitters({ 0.25f, 0.285f });
    }
    ~gui_user_library_browser() override {
        removeGuis();
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        bRefresh = true;
    }
    void buttonClicked(guibase* button) override {
        if (button == &ctr_fileTypeFilter || button->parent == &ctr_folders_list) {
            updateList();
        }
    }
    void flagRefresh() {
        bRefresh = true;
    }
    void onTick(AppCtrl* appCtrl) override {
        if (bRefresh) {
            bRefresh = false;
            updateList();
        }
    }
    void layout() override {
        auto rowHeight = theme->get(GuiConstant::CONST_ROW_HEIGHT);
        setFixedHeight(1, rowHeight + 2);
        ctr_folders_list.setRowHeight(rowHeight);
        ctr_filebrowser.setRowHeight(rowHeight);
        guictr_stacked::layout();
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
            std::vector<String> selectedExtensions = {};
            auto filter = ctr_fileTypeFilter.getSelectedFilter();
            auto addFileTypes = [](auto& selectedExtensions, auto filter){
                switch (filter) {
                    case filter_type::ANY:
                        break;
                    case filter_type::AUDIO:
                        for (auto& ext : std::array{SUPPORTED_AUDIO_FILE_TYPES}) {
                            selectedExtensions.emplace_back(ext);
                        }
                        break;
                    case filter_type::MIDI:
                        selectedExtensions.emplace_back("mid");
                        break;
                    case filter_type::PROJECT:
                        selectedExtensions.emplace_back("project");
                        break;
                    case filter_type::TRACKS:
                        selectedExtensions.emplace_back("tracks");
                        break;
                    case filter_type::PRESET:
                        selectedExtensions.emplace_back("preset");
                        break;
                    case filter_type::ALL:
                    default:
                        break;
                }
            };
            if (filter == filter_type::ALL) {
                addFileTypes(selectedExtensions, filter_type::AUDIO);
                addFileTypes(selectedExtensions, filter_type::MIDI);
                addFileTypes(selectedExtensions, filter_type::PROJECT);
                addFileTypes(selectedExtensions, filter_type::TRACKS);
                addFileTypes(selectedExtensions, filter_type::PRESET);
            } else {
                addFileTypes(selectedExtensions, filter);
            }
            ctr_filebrowser.setFileExtensions(selectedExtensions);
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
        setBackgroundRendered(true);
        ctr_filesearch.padding = 0;
        ctr_filesearch.setBackgroundRendered(false);
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
        ctr_filesearch.pos  = ivec2(0, textField.bottom() + padding/2);
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
            parent->buttonClicked(this);  // this call may delete this instance
            DAW::OpenRenameAbsoluteFilePopup(dawCtrl, popupPos, popupSize, pathFile, [daw](const String& path) {
                daw->refreshAllUserlibraryBrowsers();
                return true;
            });
        }
    } else {
        log_lf(Log::L_WARN, "Failed to export track to file %s\n", StringAsCStr(pathFile));
    }
}

bool gui_user_library_path_list::fileDropRelease(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) {
    auto parent = guiParentType<gui_user_library_browser, gui_type::CTR_TYPE_USERLIBRARY_BROWSER>(this);
    if (clip.type == dragdrop_file::TYPE_DIRECTORY && parent) {
        DAW::UserLibraryAddPath(clip.path);
        dawCtrl->refreshAllUserlibraryBrowsers();
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
                        dawCtrl->refreshAllUserlibraryBrowsers();
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
            list->flagRefresh();
        }
    });
}
