#include "keyboard.h"
#include "assert_dbg.h"
#include "commands.h"
#include "event.h"

String KeyEvent::toString() const {
    return StringFormat("KeyEvent: type=%d, keyCode=%d, scancode=%d, mods=%d, keyname=%s", type, static_cast<int32_t>(keyCode), scancode, mods, keyname);
}

/* static */ KeyCombo KeyCombo::FromKeyEvent(const KeyEvent& kevt) {
    return { kevt.mods, kevt.keyCode, kevt.keyname?kevt.keyname:"" };
}

String KeyCombo::toString() const {
    if (keyChar.empty() && static_cast<int32_t>(keyCode) < 0) {
        return "<Not Set>";
    }
    String modKeys;
    if (keyMod & KB_MOD_SHIFT)
        modKeys += "Shift + ";
    if (keyMod & KB_MOD_CTRL)
        modKeys += "Ctrl + ";
    if (keyMod & KB_MOD_ALT)
        modKeys += "Alt + ";
    if (keyMod & KB_MOD_SUPER)  // Windows key
        modKeys += "Super + ";
    if (!keyChar.empty()) {
        return modKeys + keyChar;
    }
    return modKeys + StringFormat("Keycode %d", static_cast<int32_t>(keyCode));
}

