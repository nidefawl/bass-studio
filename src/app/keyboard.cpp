#include "keyboard.h"

KeyCombo KC_UNDO        = {KB_MOD_CTRL, 0, "z"};
KeyCombo KC_REDO        = {KB_MOD_CTRL, 0, "y"};
KeyCombo KC_COPY        = {KB_MOD_CTRL, 0, "c"};
KeyCombo KC_CONSOLIDATE = {KB_MOD_CTRL, 0, "j"};
KeyCombo KC_QUANTIZE    = {KB_MOD_CTRL, 0, "q"};
KeyCombo KC_PASTE       = {KB_MOD_CTRL, 0, "v"};
KeyCombo KC_PASTE_NO_AUTOMATION  = {KB_MOD_CTRL|KB_MOD_ALT, 0, "v"};
KeyCombo KC_CUT         = {KB_MOD_CTRL, 0, "x"};
KeyCombo KC_DUPLICATE   = {KB_MOD_CTRL, 0, "d"};
KeyCombo KC_DELETE      = {0, KEY_DELETE, nullptr};
KeyCombo KC_SELECTALL   = {KB_MOD_CTRL, 0, "a"};
KeyCombo KC_MUTE        = {0, KEY_KP_0, nullptr};
KeyCombo KC_REFRESH     = {0, KEY_F5, nullptr};


KeyCombo KC_SAVE     = {KB_MOD_CTRL, 0, "s"};
KeyCombo KC_SAVEAS   = {KB_MOD_CTRL | KB_MOD_ALT, 0, "s"};
KeyCombo KC_OPEN     = {KB_MOD_CTRL, 0, "o"};
KeyCombo KC_NEW      = {KB_MOD_CTRL, 0, "n"};
KeyCombo KC_ZOOM_IN  = {0, KEY_KP_ADD, nullptr};
KeyCombo KC_ZOOM_OUT = {0, KEY_KP_SUBTRACT, nullptr};
