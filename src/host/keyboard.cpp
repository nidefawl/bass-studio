#include "keyboard.h"

KeyCombo KC_UNDO = { KB_MOD_CTRL, 0, "z" };
KeyCombo KC_REDO = { KB_MOD_CTRL, 0, "y" };
KeyCombo KC_COPY = { KB_MOD_CTRL, 0, "c" };
KeyCombo KC_PASTE = { KB_MOD_CTRL, 0, "v"};
KeyCombo KC_CUT = { KB_MOD_CTRL, 0, "x" };
KeyCombo KC_DUPLICATE = { KB_MOD_CTRL, 0, "d"};
KeyCombo KC_DELETE = { 0, KEY_DELETE, 0 };
KeyCombo KC_SELECTALL = { KB_MOD_CTRL, 0, "a" };


KeyCombo KC_SAVE = { KB_MOD_CTRL, 0, "s" };
KeyCombo KC_SAVEAS = { KB_MOD_CTRL|KB_MOD_ALT, 0, "s" };
KeyCombo KC_OPEN = { KB_MOD_CTRL, 0, "o" };
KeyCombo KC_NEW = { KB_MOD_CTRL, 0, "n" };
