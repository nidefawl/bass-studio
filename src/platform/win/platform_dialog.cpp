#include "str_util.h"
#ifdef _WIN32
#include "fileio.h"
#include "exceptions.h"
#include "types.h"
#include <windows.h>
#include <vector>
#include <limits>
#include <stdexcept>
#include "assert_dbg.h"
#include "platform.h"
#include "platform_win.h"
#include <shlobj.h>

INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM, LPARAM pData) {
    if (uMsg == BFFM_INITIALIZED) SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
    return 0;
}

int browseForFolder(const String& title, const String& pathStart, String& _out) {
    HWND hwnd = getMainHWND();

    BROWSEINFO br;
    ZeroMemory(&br, sizeof(BROWSEINFO));
    br.lpfn      = BrowseCallbackProc;
    br.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    br.hwndOwner = hwnd;
    br.lpszTitle = StringAsCStr(title);
    br.lParam    = (LPARAM) StringAsCStr(pathStart);

    LPITEMIDLIST pidl = SHBrowseForFolderA(&br);
    if (pidl != nullptr) {
        std::vector<char> localAppData;
        localAppData.resize(MAX_PATH);
        if (SHGetPathFromIDListA(pidl, localAppData.data())) {
            _out = String(localAppData.data());
            return 0;
        }
    }
    return 1;
}
int promptUserFilePath(window_base* w, int mode, SupportedFileTypes fileTypes, String& _out, String _defaultPath, String _defaultName) {

    std::vector<std::pair<String, String>> filterItems;
    filterItems.reserve(fileTypes.types.size() + 1);
    String multiFilter = "";
    String desc       = "";
    for (auto& fileType : fileTypes.types) {
        String wildExt = "*." + fileType.ext;
        String entryName = fileType.desc + " (" + wildExt + ")";
        desc += entryName + "|";
        filterItems.push_back({entryName, wildExt});
        multiFilter += wildExt + ";";
    }
    if (fileTypes.types.size() > 1 && multiFilter.size() > 0) {
        multiFilter.pop_back();
        desc.pop_back();
        filterItems.insert(filterItems.begin(), {StringAsCStr(desc), StringAsCStr(multiFilter)});
    }
    filterItems.push_back({ "All Files", "*" });
    std::vector<char> supportedFiles;
    // calculate length of null terminated string, so we can allocate the correct amount of memory
    size_t slen = 0;
    for (auto& filterItem : filterItems) {
        slen += filterItem.first.size() + 1;
        slen += filterItem.second.size() + 1;
    }
    slen += 1; // for the last null terminator
    if (slen >= MAX_PATH - 2) {
        dbgassert(0);
        return 1;
    }
    supportedFiles.resize(slen);
    size_t offset = 0;
    for (auto& filterItem : filterItems) {
        memcpy(supportedFiles.data() + offset, filterItem.first.c_str(), filterItem.first.size() + 1);
        offset += filterItem.first.size() + 1;
        memcpy(supportedFiles.data() + offset, filterItem.second.c_str(), filterItem.second.size() + 1);
        offset += filterItem.second.size() + 1;
    }
    supportedFiles[slen - 1] = 0;
    // assert that last two bytes are null
    dbgassert((slen <= 1 || supportedFiles[slen - 2] == 0) && supportedFiles[slen - 1] == 0);
    const char* filter = supportedFiles.data();
    if (mode == 0) {

        OPENFILENAME ofn;
        char szFileName[MAX_PATH] = "";

        ZeroMemory(&ofn, sizeof(ofn));

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = getMainHWND();
        ofn.lpstrFilter = filter;
        ofn.lpstrFile   = szFileName;
        ofn.nMaxFile    = MAX_PATH;
        ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = fileTypes.types.empty() ? "" : fileTypes.types[0].ext.c_str();
        if (_defaultPath.length())
            ofn.lpstrInitialDir = StringAsCStr(_defaultPath);

        if (GetOpenFileName(&ofn)) {
            _out = szFileName;
            return 1;
        }
    }
    if (mode == 1) {

        OPENFILENAME ofn;
        char szFileName[MAX_PATH] = "";
        char szFileTitle[MAX_PATH] = "";
        safe_strcpy(szFileTitle, _defaultName);

        ZeroMemory(&ofn, sizeof(ofn));

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = getMainHWND();
        ofn.lpstrFilter = filter;
        ofn.lpstrFile   = szFileName;
        ofn.nMaxFile    = MAX_PATH;
        ofn.Flags       = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = fileTypes.types.empty() ? "" : fileTypes.types[0].ext.c_str();
        if (_defaultPath.length())
            ofn.lpstrInitialDir = StringAsCStr(_defaultPath);
        ofn.lpstrFileTitle = szFileTitle;

        if (GetSaveFileName(&ofn)) {
            _out = szFileName;
            return 1;
        }
    }
    return 0;
}

#endif