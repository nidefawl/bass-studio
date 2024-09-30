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
    explicit guictxtmenu_filebrowser_base(DawCtrl* _dawCtrl, String _path)
        : guictxtmenu(), path(std::move(_path)) {
        this->size.x    = 260;
        this->maxHeight = 0;
        this->dawCtrl   = _dawCtrl;
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_REVEAL_IN_EXPLORER));
    }
    ~guictxtmenu_filebrowser_base() override = default;
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (e && e->commandtype == GlobalCommandType::CMD_REVEAL_IN_EXPLORER) {
            auto ctxt    = DAW::UI::CommandContext{ e->commandtype };
            ctxt.argStr0 = path;
            closeContextMenu();
            dawCtrl->handleGlobalCommand(ctxt);
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
        if (dawCtrl) {
            auto ctxt = new guictxtmenu_filebrowser_base(dawCtrl, pathAbs);
            parentCtrl->openContextMenu(ctxt, evt.mousepos);
        }
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
        auto daw        = dawCtrl->getDaw();
        auto& clipboard = daw->getDragDropClip();
        bool bDragging  = glm::length(vec2(*evt.dragDistance)) > 2.0f;
        if (!bTriedLoading && bDragging) {
            bTriedLoading = true;
            if (clipboard.state == dragdrop_file::STATE_NONE) {
                dawCtrl->filesDropBegin({ getPathAbs() }, evt.mousepos, evt.kbmods, false);
                // after this point the mouse events are delegated to gui_dragged_files
            }
        }
    }

    void handleDraggedBegin(MouseEvent& evt) override {
        bTriedLoading = false;
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
    void buttonClicked(guibase* button) override {
        gui_list::buttonClicked(button);
        selectedFileAbsPath   = "";
        selectedFolderAbsPath = "";
        auto folder           = gui_cast<gui_filebrowser_folder_entry, gui_type::GUI_TYPE_LIST_USER_LIBRARY_FOLDER>(button);
        if (folder) {
            bool bIsOpened = !folder->isOpened();
            folder->setIsOpened(bIsOpened);
            openedFoldersAbsPaths.erase(std::remove(openedFoldersAbsPaths.begin(), openedFoldersAbsPaths.end(), folder->getPathAbs()), openedFoldersAbsPaths.end());
            if (bIsOpened) {
                openedFoldersAbsPaths.push_back(folder->getPathAbs());
            }
            selectedFolderAbsPath = folder->getPathAbs();
            updateList();
        } else {
            auto file = gui_cast<gui_filebrowser_file_entry, gui_type::GUI_TYPE_LIST_USER_LIBRARY_FILE>(button);
            if (file) {
                selectedFileAbsPath = file->getPathAbs();
            }
        }
    }

    void updateList() {
        std::vector<gui_list_entry*> _newList;
        std::vector<String> dirsVisited;// to avoid infinite recursion
        std::vector<FileFound> files;
        dirsVisited.push_back(workingDirAbsPath);
        updateListRecursive(workingDirAbsPath, fileExtensions, dirsVisited, 0, files);

        // store selected entry, so we can restore it after updating the list
        // TODO: restoring the focused entry should be handled by the list internally
        String pathAbsSelectedEntry;
        bool bRestoreFocused = false;
        auto entry = getSelectedEntry();
        if (entry) {
            pathAbsSelectedEntry = static_cast<gui_filebrowser_entry_base*>(entry)->getPathAbs();
            auto focusedGui = parentCtrl->getGuiFocused();
            if (focusedGui == entry) {
                bRestoreFocused = true;
            }
        }
        for (auto& f : files) {
            if (f.bIsDir) {
                auto isOpenFolder = std::find(openedFoldersAbsPaths.cbegin(), openedFoldersAbsPaths.cend(), f.path) != openedFoldersAbsPaths.cend();
                _newList.push_back(createFileBrowserFolderEntry(f, isOpenFolder));
            } else {
                _newList.push_back(createFileBrowserFileEntry(f));
            }
            _newList.back()->setDepth(f.depth);
            _newList.back()->setTooltipText(f.path);
        }
        setList(_newList);
        if (!pathAbsSelectedEntry.empty()) {
            auto it = std::find_if(_newList.begin(), _newList.end(), [pathAbsSelectedEntry](gui_list_entry* e) {
                return static_cast<gui_filebrowser_entry_base*>(e)->getPathAbs() == pathAbsSelectedEntry;
            });
            if (it != _newList.end()) {
                auto idx = int32_t(it - _newList.begin());
                setSelectedIdx(idx);
                if (bRestoreFocused) {
                    parentCtrl->focusGui(*it);
                }
            }
        }
    }

    virtual gui_filebrowser_folder_entry* createFileBrowserFolderEntry(const FileFound& f, bool bIsOpened) {
        return new gui_filebrowser_folder_entry(f, bIsOpened);
    }

    virtual gui_filebrowser_file_entry* createFileBrowserFileEntry(const FileFound& f) {
        return new gui_filebrowser_file_entry(f);
    }

    void updateListRecursive(const String& path, const std::vector<String>& fileExtensions, std::vector<String>& dirsVisited, int32_t depth, std::vector<FileFound>& outFiles) {
        std::vector<FileFound> files;
        listDirectoryFiles(path, fileExtensions, files);
        for (auto& f : files) {
            auto fClone  = f;
            fClone.depth = depth;
            if (fClone.bIsDir) {
                if (std::find(dirsVisited.cbegin(), dirsVisited.cend(), fClone.path) == dirsVisited.cend()) {
                    dirsVisited.push_back(fClone.path);
                    auto isOpenFolder = std::find(openedFoldersAbsPaths.cbegin(), openedFoldersAbsPaths.cend(), fClone.path) != openedFoldersAbsPaths.cend();
                    outFiles.push_back(fClone);
                    if (isOpenFolder) {
                        updateListRecursive(fClone.path, fileExtensions, dirsVisited, depth + 1, outFiles);
                    }
                }
            } else {
                outFiles.push_back(fClone);
            }
        }
    }
};

namespace DAW {

    class SearchFileTask final : public WorkerThread::ThreadTask {
        std::atomic<bool> m_cancelled{};

        std::vector<String> directories;
        std::vector<String> fileExtensions;
        std::vector<String> searchTerms;

        std::vector<FileFound> filesFound;
        size_t maxFiles = 1000;

    public:
        ~SearchFileTask() {
        }
        void setMaxFiles(size_t _maxFiles) {
            maxFiles = _maxFiles;
        }
        void setSearchOptions(const std::vector<String>& _directories, const std::vector<String>& _fileExtensions, const std::vector<String>& _searchTerms) {
            directories    = _directories;
            fileExtensions = _fileExtensions;
            searchTerms    = _searchTerms;
        }
        void run() override {
            std::vector<String> dirsVisited;// to avoid infinite recursion
            for (auto& dir : directories) {
                dirsVisited.push_back(dir);
                updateListRecursive(dir, dirsVisited, 0);
            }
        }

        void setCancelled() {
            m_cancelled = true;
            setError();
        }

        bool isCancelled() const {
            return m_cancelled;
        }

        void updateListRecursive(const String& path, std::vector<String>& dirsVisited, int32_t depth) {
            if (isCancelled()) {
                return;
            }
            std::vector<FileFound> files;
            listDirectoryFiles(path, fileExtensions, files);
            for (auto& f : files) {
                auto fClone = f;
                // fClone.depth = depth;
                if (fClone.bIsDir) {
                    if (std::find(dirsVisited.cbegin(), dirsVisited.cend(), fClone.path) == dirsVisited.cend()) {
                        dirsVisited.push_back(fClone.path);
                        updateListRecursive(fClone.path, dirsVisited, depth + 1);
                    }
                } else {
                    bool bMatch = true;
                    for (auto& term : searchTerms) {
                        if (fClone.name.find(term) == String::npos) {
                            bMatch = false;
                            break;
                        }
                    }
                    if (bMatch) {
                        this->filesFound.push_back(fClone);
                        if (this->filesFound.size() > maxFiles) {
                            break;
                        }
                    }
                }
                if (isCancelled()) {
                    break;
                }
            }
        }

        std::vector<FileFound>& getFilesFound() {
            return filesFound;
        }
    };

}// namespace DAW

class guictr_filesearch final : public gui_list {
    std::shared_ptr<DAW::SearchFileTask> searchFileTask;// TODO: make sure lifetime extends this object
    std::vector<std::shared_ptr<DAW::SearchFileTask>> previousSearchFileTasks;
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
        if (searchFileTask && !searchFileTask->isCompleted()) {
            searchFileTask->setCancelled();
            previousSearchFileTasks.push_back(searchFileTask);
        }
        for (auto it = previousSearchFileTasks.begin(); it != previousSearchFileTasks.end();) {
            if ((*it)->isCompleted()) {
                it = previousSearchFileTasks.erase(it);
            } else {
                ++it;
            }
        }
        searchFileTask = std::make_shared<DAW::SearchFileTask>();
        searchFileTask->setSearchOptions(_directories, _fileExtensions, _searchTerms);
        dawCtrl->getDaw()->getWorkerThread()->pushTask(searchFileTask.get());
    }

    void onTick(AppCtrl* ctrl) override {
        gui_list::onTick(ctrl);
        if (searchFileTask && searchFileTask->isCompleted()) {
            onCompletedSearch(searchFileTask.get());
            searchFileTask.reset();
        }
        for (auto it = previousSearchFileTasks.begin(); it != previousSearchFileTasks.end();) {
            if ((*it)->isCompleted()) {
                it = previousSearchFileTasks.erase(it);
            } else {
                ++it;
            }
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
