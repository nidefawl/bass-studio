#if defined(__linux__) || defined(__APPLE__)
#include <exception>
#include <vector>
#include "assert_dbg.h"
#include "fileio.hpp"
#include "hires_timer.hpp"
#include "logging.hpp"
#include "platform.hpp"
#include "platform/linux/nfd/nfd.hpp"
#include "str_util.hpp"
#include "window.hpp"


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
                       SupportedFileTypes fileTypes,
                       String& _out,
                       String _defaultPath,
                       String _defaultName) {
    std::vector<nfdfilteritem_t> filterItems;
    filterItems.reserve(fileTypes.types.size() + 1);
    String multiFilter = "";
    String desc       = "";
    for (auto& fileType : fileTypes.types) {
        filterItems.push_back({fileType.desc, fileType.ext});
        desc += fileType.desc;
        desc += ",";
        multiFilter += fileType.ext;
        multiFilter += ",";
    }
    if (fileTypes.types.size() > 1 && multiFilter.size() > 0) {
        multiFilter.pop_back();
        desc.pop_back();
        filterItems.insert(filterItems.begin(), {StringAsCStr(desc), StringAsCStr(multiFilter)});
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