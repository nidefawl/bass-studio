#pragma once
#include <glm/geometric.hpp>
#include <utility>

#include "assert_dbg.h"
#include "fileio.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/controls/list.h"
#include "gui/gui.h"
#include "gui/track/trackcontrols.h"
#include "gui/track/trackctr.h"
#include "host/daw/mainctrl.h"
#include "host/track/track_impl.h"
#include "host/track/trackctr_types.h"
#include "logging.h"
#include "renderresources.h"
#include "str_util.h"


int32_t GetIconFromExtension(const String& path);

class guictxtmenu_filebrowser_base final : public guictxtmenu {
    const String path;

public:
    explicit guictxtmenu_filebrowser_base(BaseCtrl* _parentCtrl, String _path)
        : guictxtmenu(), path(std::move(_path)) {
        this->size.x    = 260;
        this->maxHeight = 0;
        this->parentCtrl = _parentCtrl;
        this->dawCtrl   = dynamic_cast<DawCtrl*>(_parentCtrl);
        addEntry(new ctxtmenu_entry(this->parentCtrl, GlobalCommandType::CMD_REVEAL_IN_EXPLORER));
    }
    ~guictxtmenu_filebrowser_base() override = default;
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (e && e->commandtype == GlobalCommandType::CMD_REVEAL_IN_EXPLORER) {
            auto ctxt    = DAW::UI::CommandContext{ e->commandtype };
            ctxt.argStr0 = path;
            auto parent = parentCtrl;
            closeContextMenu();
            parent->handleGlobalCommand(ctxt);
            return true;
        }
        return false;
    }
};

class gui_filebrowser_entry_base : public gui_list_entry {
protected:
    const String name;
    const String pathAbs;

public:
    gui_filebrowser_entry_base(String _name, String _pathAbs) : gui_list_entry(), name(std::move(_name)), pathAbs(std::move(_pathAbs)) {
        setLabel(name);
        setTooltipText(pathAbs);
    }
    ~gui_filebrowser_entry_base() override = default;

    String getText() override {
        return name;
    }
    String getPathAbs() const {
        return pathAbs;
    }
    void handleRightClick(MouseEvent& evt) override {
        auto ctxt = new guictxtmenu_filebrowser_base(parentCtrl, pathAbs);
        parentCtrl->openContextMenu(ctxt, evt.mousepos);
    }
};

class gui_filebrowser_folder_entry : public gui_filebrowser_entry_base {
    bool bIsOpened = false;

public:
    explicit gui_filebrowser_folder_entry(const FileFound& f, bool _bIsOpened = false)
        : gui_filebrowser_entry_base(f.name, f.path), bIsOpened(_bIsOpened) {
        setGuiType(gui_type::GUI_TYPE_LIST_USER_LIBRARY_FOLDER);
        icon = bIsOpened ? ICON_FOLDER_OPEN : ICON_FOLDER;
    }
    bool isOpened() const {
        return bIsOpened;
    }
    void setIsOpened(const bool opened) {
        bIsOpened = opened;
        icon      = opened ? ICON_FOLDER_OPEN : ICON_FOLDER;
    }

    void handleDragDropHover(MouseHitEvt& mouseHit) override {
        bIsOpened = false;
        parent->buttonClicked(this);
    }
};

class gui_filebrowser_file_entry final : public gui_filebrowser_entry_base {
    bool bTriedLoading = false;
public:
    explicit gui_filebrowser_file_entry(const FileFound& f)
        : gui_filebrowser_entry_base(f.name, f.path) {
        setGuiType(gui_type::GUI_TYPE_LIST_USER_LIBRARY_FILE);
        icon = GetIconFromExtension(f.name);
    }

    bool& isDragging() {
        static bool bDragging = false;
        return bDragging;
    }

    void handleDraggedMove(MouseEvent& evt) override {
        bool bDragging  = glm::length(vec2(*evt.dragDistance)) > 2.0f;
        if (!bTriedLoading && bDragging) {
            bTriedLoading = true;
            if (dawCtrl) {
                auto daw        = dawCtrl->getDaw();
                auto& clipboard = daw->getDragDropClip();
                if (clipboard.state == dragdrop_file::STATE_NONE) {
                    dawCtrl->filesDropBegin({ getPathAbs() }, evt.mousepos, evt.kbmods, false);
                    // after this point the mouse events are delegated to gui_dragged_files
                }
            }
        }
    }

    void handleDraggedBegin(MouseEvent& evt) override {
        bTriedLoading = false;
        if (dawCtrl)
            dawCtrl->getDaw()->resetDragDropClipboards();
    }

    void handleDraggedRelease(MouseEvent& evt) override {
        if (!bTriedLoading) {
            if (parent) parent->buttonClicked(this);
        }
    }
};

class guictr_filebrowser : public gui_list {
protected:
    String workingDirAbsPath;
    String selectedFileAbsPath;
    String selectedFolderAbsPath;
    std::vector<String> fileExtensions;
    std::vector<String> openedFoldersAbsPaths;

    void updateListRecursive(const String& path, const std::vector<String>& fileExtensions, std::vector<String>& dirsVisited, int32_t depth, std::vector<FileFound>& outFiles);
public:
    guictr_filebrowser() : gui_list() {
        setGuiType(gui_type::CTR_TYPE_FILEBROWSER);
    }
    ~guictr_filebrowser() override = default;
    void setWorkingDir(const String& path) {
        workingDirAbsPath = path;
    }
    void setFileExtensions(const std::vector<String>& ext) {
        fileExtensions = ext;
    }
    String getSelectedFileAbsPath() const {
        return selectedFileAbsPath;
    }
    String getWorkingDirAbsPath() const {
        return workingDirAbsPath;
    }
    void buttonClicked(guibase* button) override;

    void updateList();

    virtual gui_filebrowser_folder_entry* createFileBrowserFolderEntry(const FileFound& f, bool bIsOpened) {
        return new gui_filebrowser_folder_entry(f, bIsOpened);
    }

    virtual gui_filebrowser_file_entry* createFileBrowserFileEntry(const FileFound& f) {
        return new gui_filebrowser_file_entry(f);
    }
    void handleRightClick(MouseEvent& evt) override;
};

class guictr_filesearch final : public gui_list {
    std::shared_ptr<DAW::SearchFileTask> searchFileTask;// TODO: make sure lifetime extends this object
    std::vector<FileFound> filesFound;

public:
    guictr_filesearch() : gui_list() {
    }
    ~guictr_filesearch() override = default;
    void resetResults() {
        filesFound.clear();
        setList({});
    }
    void search(const std::vector<String>& _directories, const std::vector<String>& _fileExtensions, const std::vector<String>& _searchTerms) {
        filesFound.clear();
        dawCtrl->getDaw()->fileSearchCancel();
        dawCtrl->getDaw()->fileSearchUpdate();
        searchFileTask = std::make_shared<DAW::SearchFileTask>();
        searchFileTask->setSearchOptions(_directories, _fileExtensions, _searchTerms);
        dawCtrl->getDaw()->fileSeachStart(searchFileTask);
    }

    void onTick(AppCtrl* ctrl) override {
        gui_list::onTick(ctrl);
        if (searchFileTask && searchFileTask->isCompleted()) {
            onCompletedSearch(searchFileTask.get());
            searchFileTask.reset();
        }
    }

    void onCompletedSearch(DAW::SearchFileTask* task) {
        if (task == searchFileTask.get()) {
            filesFound = task->getFilesFound();
            std::vector<gui_list_entry*> _newList;
            for (auto& f : filesFound) {
                _newList.push_back(new gui_filebrowser_file_entry(f));
                _newList.back()->setDepth(f.depth);
                _newList.back()->setTooltipText(f.path);
            }
            setList(_newList);
        }
    }
};
