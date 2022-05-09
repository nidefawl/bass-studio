#include <ctime>
#include <memory>
#include "appsettings.h"


#if WINDOW_RESTORE_POS
#ifdef __linux__
#include "platform/linux/windowsize.h"
#endif
#ifdef _WIN32
#include "platform/win/windowsize.h"
#endif
#endif

appwindowsettings::appwindowsettings() noexcept

#if WINDOW_RESTORE_POS
    : size(std::make_unique<windowsize>())
#endif
{
}

appwindowsettings::appwindowsettings(const appwindowsettings& other)

#if WINDOW_RESTORE_POS
    : size(std::make_unique<windowsize>())
#endif
{
    *this = other;
}

appwindowsettings& appwindowsettings::operator=(const appwindowsettings& other) {
    if (this != &other) {

#if WINDOW_RESTORE_POS
        *this->size = *other.size;
#endif
        this->dens = other.dens;
    }
    return *this;
}

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
                   "-" + std::to_string(now->tm_mday) + "\n";
    recentFilesMeta[path] = recentfilelistentry{ path, strDate };
}
