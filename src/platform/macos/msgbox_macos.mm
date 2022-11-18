#if defined (__APPLE__)
#include "msgbox.h"
#import <Cocoa/Cocoa.h>


namespace ngui {

namespace {


NSAlertStyle getAlertStyle(Style style) {
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12
   switch (style) {
      case Style::Info:
         return NSAlertStyleInformational;
      case Style::Warning:
         return NSAlertStyleWarning;
      case Style::Error:
         return NSAlertStyleCritical;
      default:
         return NSAlertStyleInformational;
   }
#else
   switch (style) {
      case Style::Info:
         return NSInformationalAlertStyle;
      case Style::Warning:
         return NSWarningAlertStyle;
      case Style::Error:
         return NSCriticalAlertStyle;
      case Style::Question:
         return NSWarningAlertStyle;
      default:
         return NSInformationalAlertStyle;
   }
#endif
}

} // namespace

void showNotification(Style style, const char* title, const char* message) {
   NSAlert *alert = [[NSAlert alloc] init];

   [alert setMessageText:[NSString stringWithCString:title
                                   encoding:[NSString defaultCStringEncoding]]];
   [alert setInformativeText:[NSString stringWithCString:message
                                       encoding:[NSString defaultCStringEncoding]]];

   [alert setAlertStyle:getAlertStyle(style)];
   NSString* const kOkStr = @"OK";
   [alert addButtonWithTitle:kOkStr];
   // [alert addButtonWithTitle:kYesStr];
   // [alert addButtonWithTitle:kNoStr];
   // [alert addButtonWithTitle:kCancelStr];
   // [alert addButtonWithTitle:kQuitStr];

   // Force the alert to appear on top of any other windows
   [[alert window] setLevel:NSModalPanelWindowLevel];
   auto ret = [alert runModal];
   (void)ret;
   [alert release];
}

} // namespace ngui
#endif //__APPLE__
