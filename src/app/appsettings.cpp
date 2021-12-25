#include "appsettings.h"

#ifdef _WIN32
#include "platform/win/windowsize.h"

void saveWindowPos(HWND hwnd, windowsize* size) {
    size->valid = GetWindowPlacement(hwnd, &(size->p)) != 0;
}

bool restoreWindowPos(HWND hwnd, windowsize* size) {
    if (size->valid) {
        return SetWindowPlacement(hwnd, &(size->p)) != 0;
    }
    return false;
}
#endif


appwindowsettings::appwindowsettings() noexcept
#ifdef _WIN32
    : size(new windowsize)
#endif
{
}

appwindowsettings::appwindowsettings(const appwindowsettings& other)
#ifdef _WIN32
    : size(new windowsize)
#endif
{
    *this = other;
}

appwindowsettings& appwindowsettings::operator=(const appwindowsettings& other) {
#ifdef _WIN32
    *this->size = *other.size;
#endif
    this->dens = other.dens;
    return *this;
}

#if HAS_APP_SETTINGS
namespace DAW {
    appsettings settings;
}
#endif