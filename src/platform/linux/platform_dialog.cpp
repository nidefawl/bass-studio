#if defined(__linux__)
#include <exception>
#include <vector>
#include "assert_dbg.h"
#include "fileio.h"
#include "hires_timer.h"
#include "logging.h"
#include "platform.h"
#include "platform/linux/nfd/nfd.h"
#include "str_util.h"
#include "window.h"


int browseForFolder(const String& title, const String& pathStart, String& _out) {
    nfdchar_t* savePath{};
    nfdresult_t result{};
    result = NFD_PickFolder(&savePath, pathStart.c_str());
    if (result == NFD_OKAY) {
        _out = savePath;
        replaceString(_out, "%20", " ");
        // remember to free the memory (since NFD_OKAY is returned)
        NFD_FreePath(savePath);
        return 0;
    } 
    if (result != NFD_CANCEL) {
        log_lf(Log::L_ERROR, "Error: %s\n", NFD_GetError());
    }
    return 1;
}
int promptUserFilePath(window_base* w,
                       int mode,
                       std::vector<SupportedFileType> fileTypes,
                       String& _out,
                       String _defaultPath,
                       String _defaultName) {
    std::vector<nfdfilteritem_t> filterItems;
    filterItems.reserve(fileTypes.size());
    for (auto& fileType : fileTypes) {
        filterItems.push_back({StringAsCStr(fileType.desc), StringAsCStr(fileType.ext)});
    }
    nfdchar_t* savePath{};
    nfdresult_t result{};
    if (mode == 0) {
        result = NFD_OpenDialog(&savePath, filterItems.data(), filterItems.size(), _defaultPath.empty() ? nullptr : _defaultPath.c_str());
    } else {
        result = NFD_SaveDialog(&savePath, filterItems.data(), filterItems.size(), _defaultPath.empty() ? nullptr : _defaultPath.c_str(), _defaultName.empty() ? nullptr : _defaultName.c_str());
    }
    if (result == NFD_OKAY) {
        _out = savePath;
        //TODO: proper url decoding
        replaceString(_out, "%20", " ");
        // remember to free the memory (since NFD_OKAY is returned)
        NFD_FreePath(savePath);
        return 1;
    } 
    if (result != NFD_CANCEL) {
        log_lf(Log::L_ERROR, "Error: %s\n", NFD_GetError());
    }
    return 0;
}

#endif