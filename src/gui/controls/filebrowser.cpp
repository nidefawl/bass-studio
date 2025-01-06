#include "filebrowser.hpp"

void guictr_filebrowser::buttonClicked(guibase* button) {
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

void guictr_filebrowser::updateListRecursive(const String& path, const std::vector<String>& fileExtensions, std::vector<String>& dirsVisited, int32_t depth, std::vector<FileFound>& outFiles) {
    std::vector<FileFound> files;
    listFilesystemNonRecursive(path, fileExtensions, files);
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

void guictr_filebrowser::updateList() {
    std::vector<gui_list_entry*> _newList;
    std::vector<String> dirsVisited;// to avoid infinite recursion
    std::vector<FileFound> files;
    dirsVisited.push_back(workingDirAbsPath);
    updateListRecursive(workingDirAbsPath, fileExtensions, dirsVisited, 0, files);

    // store selected entry, so we can restore it after updating the list
    // TODO: restoring the focused entry should be handled by the list internally
    String pathAbsSelectedEntry;
    bool bRestoreFocused = false;
    auto entry           = getSelectedEntry();
    if (entry) {
        pathAbsSelectedEntry = static_cast<gui_filebrowser_entry_base*>(entry)->getPathAbs();
        auto focusedGui      = parentCtrl->getGuiFocused();
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

void guictr_filebrowser::handleRightClick(MouseEvent& evt) {
    auto* ctxt     = new guictxtmenu_colorpalette();
    ctxt->callback = [](uint32_t val) {
    };
    parentCtrl->openContextMenu(ctxt, evt.mousepos);
}
