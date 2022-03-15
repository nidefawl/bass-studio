#ifdef _WIN32
#include "fileio.h"
#include "exceptions.h"
#include "types.h"
#include <Windows.h>
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

int promptUserFilePath(window_base*, int mode, std::vector<SupportedFileType> fileTypes, String& _out) {
    char supportedFiles[MAX_PATH] = "";

    int offset  = 0;
    fileTypes.push_back(SupportedFileType{ "All Files", "*" });
    for (SupportedFileType& type : fileTypes) {
        //This doesn't look safe, truncating requires extra attention. Watch out!
        int val = _snprintf(supportedFiles + offset, size_t(MAX_PATH) - offset, "%s (*.%s)", StringAsCStr(type.desc), StringAsCStr(type.ext));
        if (val > 0) {
            offset += val;
            supportedFiles[offset] = 0;
            offset++;
        }
        val = _snprintf(supportedFiles + offset, size_t(MAX_PATH) - offset, "*.%s", StringAsCStr(type.ext));
        if (val > 0) {
            offset += val;
            supportedFiles[offset] = 0;
            offset++;
        }
    }
    supportedFiles[offset] = 0;

    if (mode == 0) {

        OPENFILENAME ofn;
        char szFileName[MAX_PATH] = "";

        ZeroMemory(&ofn, sizeof(ofn));

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = getMainHWND();
        ofn.lpstrFilter = supportedFiles;
        ofn.lpstrFile   = szFileName;
        ofn.nMaxFile    = MAX_PATH;
        ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = StringAsCStr(fileTypes[0].ext);

        if (GetOpenFileName(&ofn)) {
            _out = szFileName;
            return 1;
        }
    }
    if (mode == 1) {

        OPENFILENAME ofn;
        char szFileName[MAX_PATH] = "";

        ZeroMemory(&ofn, sizeof(ofn));

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = getMainHWND();
        ofn.lpstrFilter = supportedFiles;
        ofn.lpstrFile   = szFileName;
        ofn.nMaxFile    = MAX_PATH;
        ofn.Flags       = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = StringAsCStr(fileTypes[0].ext);

        if (GetSaveFileName(&ofn)) {
            _out = szFileName;
            return 1;
        }
    }
    return 0;
}

#endif