#if defined(__linux__)
#include "msgbox.h"
#include "str_util.h"
#include "buildinfo.h"

int DBus_DesktopNotification(const String& source, const String& title, const String& body, int timeoutMilliseconds);

namespace ngui {

    void showNotification(Style style, const char* title, const char* message) {
        DBus_DesktopNotification(BuildInfo::PRODUCT_NAME_UPPER, title, message, 5000);
    }

}// namespace ngui
#endif//__linux__
