#if defined(__linux__)
#include "msgbox.h"
#include "str_util.h"

int DBus_DesktopNotification(const String& source, const String& title, const String& body, int timeoutMilliseconds);

namespace ngui {

    void showNotification(Style style, const char* title, const char* message) {
        DBus_DesktopNotification("DAW", title, message, 5000);
    }

}// namespace ngui
#endif//__linux__
