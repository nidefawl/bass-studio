#pragma once
#include <utility>

#include "fileio.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/controls/list.h"
#include "host/daw/mainctrl.h"
#include "str_util.h"

class guictxtmenu_filebrowser_base final : public guictxtmenu {
    const String path;
public:
    explicit guictxtmenu_filebrowser_base(DawCtrl* _dawCtrl, String  _path) 
        : guictxtmenu(), path(std::move(_path)) {
        this->size.x  = 260;
        this->maxHeight = 0;
        this->dawCtrl = _dawCtrl;
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_REVEAL_IN_EXPLORER));
    }
    ~guictxtmenu_filebrowser_base() override = default;
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (e && e->commandtype == GlobalCommandType::CMD_REVEAL_IN_EXPLORER) {
            auto ctxt = DAW::UI::CommandContext{e->commandtype};
            ctxt.argStr0 = path;
            closeContextMenu();
            dawCtrl->handleGlobalCommand(ctxt);
            return true;
        }
        return false;
    }
};

class gui_filebrowser_entry_base : public gui_list_entry {
    const String name;
    const String pathAbs;
public:
    gui_filebrowser_entry_base(String _name, String _pathAbs) : gui_list_entry(), name(std::move(_name)), pathAbs(std::move(_pathAbs)) {
        setDragRendered(true);
        setLabel(name);
        setTooltipText(pathAbs);
    }
    ~gui_filebrowser_entry_base() override = default;
    void dragMoveOn(guibase* target, ivec2 mousepos) override {
        dawCtrl->onFileBrowserEntryDragMove(pathAbs, name, mousepos);
    }
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
        dawCtrl->onFileBrowserEntryDragRelease(pathAbs, name, mousepos);
    }
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
    gui_filebrowser_folder_entry(String _name, String _pathAbs, bool _bIsOpened = false)
        : gui_filebrowser_entry_base(std::move(_name), std::move(_pathAbs)), bIsOpened(_bIsOpened) {
        setGuiType(gui_type::GUI_TYPE_LIST_USER_LIBRARY_FOLDER);
        icon = bIsOpened ? ICON_FOLDER_OPEN : ICON_FOLDER;
    }
    bool isOpened() const {
        return bIsOpened;
    }
    void setIsOpened(const bool opened) {
        bIsOpened = opened;
        icon = opened ? ICON_FOLDER_OPEN : ICON_FOLDER;
    }
};

class gui_filebrowser_file_entry_t final : public gui_filebrowser_entry_base {
public:
    explicit gui_filebrowser_file_entry_t(const FileFound& f)
        : gui_filebrowser_entry_base(f.name, f.path) {
        setGuiType(gui_type::GUI_TYPE_LIST_USER_LIBRARY_FILE);
        icon = ICON_FILE;
    }
};
class guictr_filebrowser final : public gui_list {
    String workingDirAbsPath;
    String selectedFileAbsPath;
    String selectedFolderAbsPath;
    std::vector<String> fileExtensions;
    std::vector<String> openedFoldersAbsPaths;
public:
    guictr_filebrowser() : gui_list() {
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
        selectedFileAbsPath = "";
        selectedFolderAbsPath = "";
        auto folder = gui_cast<gui_filebrowser_folder_entry, gui_type::GUI_TYPE_LIST_USER_LIBRARY_FOLDER>(button);
        if (folder) {
            bool bIsOpened = folder->isOpened();
            folder->setIsOpened(!bIsOpened);
            if (bIsOpened) {
                openedFoldersAbsPaths.erase(std::remove(openedFoldersAbsPaths.begin(), openedFoldersAbsPaths.end(), folder->getPathAbs()), openedFoldersAbsPaths.end());
            } else {
                openedFoldersAbsPaths.push_back(folder->getPathAbs());
            }
            updateList();
            selectedFolderAbsPath = folder->getPathAbs();
        } else {
            auto file = gui_cast<gui_filebrowser_file_entry_t, gui_type::GUI_TYPE_LIST_USER_LIBRARY_FILE>(button);
            if (file) {
                selectedFileAbsPath = file->getPathAbs();
            }
        }
    }

    void updateList() {
        std::vector<gui_list_entry*> _newList;
        std::vector<String> dirsVisited; // to avoid infinite recursion
        std::vector<FileFound> files;
        dirsVisited.push_back(workingDirAbsPath);
        updateListRecursive(workingDirAbsPath, fileExtensions, dirsVisited, 0, files);
        for (auto& f : files) {
            if (f.bIsDir) {
                auto isOpenFolder = std::find(openedFoldersAbsPaths.cbegin(), openedFoldersAbsPaths.cend(), f.path) != openedFoldersAbsPaths.cend();
                _newList.push_back(new gui_filebrowser_folder_entry(f.name, f.path, isOpenFolder));
            } else {
                _newList.push_back(new gui_filebrowser_file_entry_t(f));
            }
            _newList.back()->setDepth(f.depth);
            _newList.back()->setTooltipText(f.path);
        }
        setList(_newList);
    }

    void updateListRecursive(const String& path, const std::vector<String>& fileExtensions, std::vector<String>& dirsVisited, int32_t depth, std::vector<FileFound>& outFiles) {
        std::vector<FileFound> files;
        listDirectoryFiles(path, fileExtensions, files);
        for (auto& f : files) {
            auto fClone = f;
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

