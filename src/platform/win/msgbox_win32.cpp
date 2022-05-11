#ifdef _WIN32
#include "msgbox.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ngui {

    namespace /*anonymous*/ {

        UINT getIcon(Style style) {
            switch (style) {
                case Style::Info:
                    return MB_ICONINFORMATION;
                case Style::Warning:
                    return MB_ICONWARNING;
                case Style::Error:
                    return MB_ICONERROR;
                default:
                    break;
            }
            return MB_ICONINFORMATION;
        }

    }// namespace

    void showNotification(Style style, const char* title, const char* message) {
        UINT flags = MB_TASKMODAL | MB_OK;
        flags |= getIcon(style);
        MessageBoxA(nullptr, message, title, flags);
    }

}// namespace ngui
#endif
