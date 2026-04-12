#include <ctime>
#include <memory>
#include "appsettings.hpp"
#if defined(__linux__) || defined(__APPLE__)
#include "platform/linux/windowsize.hpp"
#endif
#ifdef _WIN32
#include "platform/win/windowsize.hpp"
#endif
#include "seq_datetime.hpp"

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

    int64_t timestamp = GetInt64GMTDate();
    String  isoDate   = GetInt64DateAsLocalizedTimeStr(timestamp);

    recentFilesMeta[path] = recentfilelistentry{ path, timestamp, isoDate };

}
