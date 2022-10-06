#pragma once
#include "str_util.h"
#include "event.h"

struct KeyCombo {
    int keyMod          = 0;
    KeyboardKey keyCode = KeyboardKey::DAW_KB_INVALID;
    String keyChar;
    bool match(const KeyEvent& kevt) const {
        if (kevt.mods != keyMod) {
            return false;
        }
        if (!keyChar.empty()) {
            return kevt.keyname && keyChar == kevt.keyname;
        } else {
            return kevt.keyCode == keyCode;
        }
    }
    String toString() const;
    static KeyCombo FromKeyEvent(const KeyEvent& kevt);
};

inline bool isKC(KeyCombo c, KeyEvent& kevt) {
    return c.match(kevt);
}
inline bool isArrowKey(KeyboardKey key) {
    return key == KeyboardKey::DAW_KB_UP || key == KeyboardKey::DAW_KB_DOWN || key == KeyboardKey::DAW_KB_LEFT || key == KeyboardKey::DAW_KB_RIGHT;
}
inline bool isNumericInput(KeyboardKey key) {
    return (key >= KeyboardKey::DAW_KB_0 && key <= KeyboardKey::DAW_KB_9) || (key >= KeyboardKey::DAW_KB_KP_0 && key <= KeyboardKey::DAW_KB_KP_9) || key == KeyboardKey::DAW_KB_PERIOD || key == KeyboardKey::DAW_KB_KP_DECIMAL;
}
inline bool isShift(KeyboardMods mods) {
    return (mods & KB_MOD_SHIFT);
}
inline bool isCtrl(KeyboardMods mods) {
    return (mods & KB_MOD_SYSTEM);
}
inline bool isShiftKey(KeyboardKey key) {
    return key == KeyboardKey::DAW_KB_LEFT_SHIFT || key == KeyboardKey::DAW_KB_RIGHT_SHIFT;
}
inline bool isCtrlKey(KeyboardKey key) {
#if defined(__APPLE__) || defined(DOXYGEN_DOCUMENTATION_BUILD)
    return key == KEY_LEFT_SUPER || key == KEY_RIGHT_SUPER;
#else
    return key == KeyboardKey::DAW_KB_LEFT_CONTROL || key == KeyboardKey::DAW_KB_RIGHT_CONTROL;
#endif
}
inline bool isAltKey(KeyboardKey key) {
    return key == KeyboardKey::DAW_KB_LEFT_ALT || key == KeyboardKey::DAW_KB_RIGHT_ALT;
}
inline bool isAlt(KeyboardMods mods) {
    return (mods & KB_MOD_ALT);
}
inline void arrowKeyToXY(KeyboardKey key, int& x, int& y) {
    x = 0;
    y = 0;
    if (key == KeyboardKey::DAW_KB_UP) {
        y = 1;
    }
    if (key == KeyboardKey::DAW_KB_DOWN) {
        y = -1;
    }
    if (key == KeyboardKey::DAW_KB_RIGHT) {
        x = 1;
    }
    if (key == KeyboardKey::DAW_KB_LEFT) {
        x = -1;
    }
}

const char* GlfwKeycodeToString(KeyboardKey keyCode, int scancode);