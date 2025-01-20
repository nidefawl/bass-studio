#include <ctime>
#include <memory>
#include "appsettings.hpp"
#if defined(__linux__) || defined(__APPLE__)
#include "platform/linux/windowsize.hpp"
#endif
#ifdef _WIN32
#include "platform/win/windowsize.hpp"
#endif

void recentfilelist::add(const String& path) {
    while (sortedEntries.size() > 31) {
        String& s = sortedEntries.back();
        auto it   = recentFilesMeta.find(s);
        if (it != recentFilesMeta.end()) {
            recentFilesMeta.erase(it);
        }
        sortedEntries.pop_back();
    }
    auto it = sortedEntries.begin();
    while (it != sortedEntries.end()) {
        String& s = *it;
        if (s == path) {
            it = sortedEntries.erase(it);
        } else {
            it++;
        }
    }

    sortedEntries.insert(sortedEntries.begin(), path);

    std::time_t t = std::time(nullptr);// get time now
    std::tm* now  = std::localtime(&t);
    auto strDate  = std::to_string(now->tm_year + 1900) +
                   "-" + std::to_string(now->tm_mon + 1) +
                   "-" + std::to_string(now->tm_mday);
    recentFilesMeta[path] = recentfilelistentry{ path, strDate };
}
