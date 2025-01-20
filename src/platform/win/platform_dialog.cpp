#include "str_util.hpp"
#include <cstring>
#include <string>
#ifdef _WIN32
#include "fileio.hpp"
#include "assert_dbg.h"
#include "platform_win.hpp"
#include "str_win32.hpp"
#include <shlobj.h>
#include <windows.h>
#include <vector>
#include <utfconv/utf.hpp>

namespace {
    INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM, LPARAM pData) {
        if (uMsg == BFFM_INITIALIZED) SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
        return 0;
    }
}

int browseForFolder(const String& title, const String& pathStart, String& _out) {
    HWND hwnd = getMainHWND();
    auto titleW = StringU8ToW(title);
    auto pathStartW = StringU8ToW(pathStart);
    BROWSEINFOW br;
    ZeroMemory(&br, sizeof(BROWSEINFO));
    br.lpfn      = BrowseCallbackProc;
    br.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    br.hwndOwner = hwnd;
    br.lpszTitle = StringAsCStr(titleW);
    br.lParam    = (LPARAM) StringAsCStr(pathStartW);

    LPITEMIDLIST pidl = SHBrowseForFolderW(&br);
    if (pidl != nullptr) {
        WString localAppData;
        localAppData.resize(MAX_PATH);
        if (SHGetPathFromIDListW(pidl, localAppData.data())) {
            _out = StringWToU8(localAppData);
            return 0;
        }
    }
    return 1;
}

int promptUserFilePath(window_base* w, int mode, SupportedFileTypes fileTypes, String& _out, String _defaultPath, String _defaultName) {
    std::vector<std::pair<WString, WString>> filterItems;
    filterItems.reserve(fileTypes.types.size() + 1);
    WString multiFilter = L"";
    WString desc       = L"";
    for (auto& fileType : fileTypes.types) {
        WString wildExt = L"*.";
        wildExt += StringU8ToW(fileType.ext);
        WString entryName = StringU8ToW(fileType.desc);
        entryName += L" (" + wildExt + L")";
        desc += entryName + L"|";
        filterItems.emplace_back(entryName, wildExt);
        multiFilter += wildExt + L";";
    }
    if (fileTypes.types.size() > 1 && multiFilter.size() > 0) {
        multiFilter.pop_back();
        desc.pop_back();
        filterItems.insert(filterItems.begin(), {StringAsCStr(desc), StringAsCStr(multiFilter)});
    }
    filterItems.emplace_back( L"All Files", L"*" );

    WString supportedFiles;
    for (auto& filterItem : filterItems) {
        supportedFiles += filterItem.first;
        supportedFiles += L'\0';
        supportedFiles += filterItem.second;
        supportedFiles += L'\0';
    }
    supportedFiles += L'\0';

    if (supportedFiles.length() >= MAX_PATH - 2) {
        dbgassert(0);
        return 1;
    }

    // assert that last two bytes are null
    dbgassert((supportedFiles.length() <= 1 || supportedFiles[supportedFiles.length() - 2] == 0) && supportedFiles[supportedFiles.length() - 1] == 0);

    const wchar_t* filter = supportedFiles.data();
    if (mode == 0) {
        OPENFILENAMEW ofn;
        wchar_t szFileName[MAX_PATH] = L"";
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = getMainHWND();
        ofn.lpstrFilter = filter;
        ofn.lpstrFile   = szFileName;
        ofn.nMaxFile    = MAX_PATH;
        ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = L"";
        auto defaultExtStr = StringU8ToW(fileTypes.types.empty() ? "" : fileTypes.types[0].ext);
        if (defaultExtStr.length()) {
            ofn.lpstrDefExt = defaultExtStr.c_str();
        }
        auto initialDirStr = StringU8ToW(_defaultPath);
        if (initialDirStr.length()) {
            ofn.lpstrInitialDir = initialDirStr.c_str();
        }
        if (GetOpenFileNameW(&ofn)) {
            _out = StringWToU8(szFileName);
            return _out.empty() ? 0 : 1;
        }
        return 0;
    }

    if (mode == 1) {
        OPENFILENAMEW ofn;
        wchar_t szFileName[MAX_PATH] = L"";
        std::vector<wchar_t> szFileTitle;
        szFileTitle.resize(MAX_PATH);
        szFileTitle[0] = 0;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = getMainHWND();
        ofn.lpstrFilter = filter;
        ofn.lpstrFile   = szFileName;
        ofn.nMaxFile    = MAX_PATH;
        ofn.Flags       = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = L"";
        auto defaultExtStr = StringU8ToW(fileTypes.types.empty() ? "" : fileTypes.types[0].ext);
        if (defaultExtStr.length()) {
            ofn.lpstrDefExt = defaultExtStr.c_str();
        }
        auto initialDirStr = StringU8ToW(_defaultPath);
        if (initialDirStr.length()) {
            ofn.lpstrInitialDir = initialDirStr.c_str();
        }
        auto defaultNameStr = StringU8ToW(_defaultName);
        if (defaultNameStr.length()) {
            ofn.lpstrFileTitle = defaultNameStr.data();
        }
        if (GetSaveFileNameW(&ofn)) {
            _out = StringWToU8(szFileName);
            return _out.empty() ? 0 : 1;
        }
        return 0;
    }
    return 0;
}
#endif