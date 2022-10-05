#include "keyboard.h"
#include "assert_dbg.h"
#include "commands.h"
#include "event.h"
#include "renderresources.h"
#include "seq_util.h"
#include <algorithm>

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

String KeyEvent::toString() const {
    return StringFormat("KeyEvent: type=%d, keyCode=%d, scancode=%d, mods=%d, keyname=%s", type, keyCode, scancode, mods, keyname);
}

String KeyCombo::toString() const {
    String modKeys;
    if (keyMod & KB_MOD_SHIFT)
        modKeys += "Shift+";
    if (keyMod & KB_MOD_CTRL)
        modKeys += "Ctrl+";
    if (keyMod & KB_MOD_ALT)
        modKeys += "Alt+";
    if (keyMod & KB_MOD_SUPER)  // Windows key
        modKeys += "Super+";
    if (keyChar != nullptr) {
        return modKeys + keyChar;
    } else {
        return modKeys + StringFormat("Keycode %d", keyCode);
    }
    return modKeys;
}

void DAW::UI::CommandManager::updateCommandShortcuts() {
    for (auto& cmd : commands) {
        if (cmd.keyCombos.empty()) {
            cmd.desc.shortcut = "";
        } else {
            cmd.desc.shortcut = cmd.keyCombos[0].toString();
        }
    }
}

void DAW::UI::CommandManager::init() {
    commands.push_back({CMD_FILE_NEW, {"New", "Create a new project", "", ICON_FILE}, {KC_NEW}, 0});
    commands.push_back({CMD_FILE_OPEN, {"Open", "Open a project", "", ICON_FOLDER}, {KC_OPEN}, 0});
    commands.push_back({CMD_FILE_SAVE, {"Save", "Save the project", "", ICON_SAVE}, {KC_SAVE}, 0});
    commands.push_back({CMD_FILE_SAVEAS, {"Save As", "Save the project as a new file", "", ICON_SAVE}, {KC_SAVEAS}, 0});
    commands.push_back({CMD_FILE_CLOSE, {"Close", "Close the project", "", ICON_CLOSE}, {}, 0});
    commands.push_back({CMD_EXIT, {"Exit", "Exit the application", "", ICON_CLOSE}, {}, 0});

    commands.push_back({CMD_UNDO, {"Undo", "Undo last action", ""}, {KC_UNDO}, 0});
    commands.push_back({CMD_REDO, {"Redo", "Redo last action", ""}, {KC_REDO}, 0});
    commands.push_back({CMD_GUI_GLOBAL_ZOOM_DECREASE, {"Zoom Out", "Decrease the global zoom level", "", ICON_MINUS}, {KC_ZOOM_OUT}, 0});
    commands.push_back({CMD_GUI_GLOBAL_ZOOM_INCREASE, {"Zoom In", "Increase the global zoom level", "", ICON_PLUS}, {KC_ZOOM_IN}, 0});
    commands.push_back({CMD_CUT, {"Cut", "Cut selected items", "", ICON_CUT}, {KC_CUT}, 0});
    commands.push_back({CMD_COPY, {"Copy", "Copy selected items", "", ICON_COPY}, {KC_COPY}, 0});
    commands.push_back({CMD_PASTE, {"Paste", "Paste items", "", ICON_PASTE}, {KC_PASTE}, 0});
    commands.push_back({CMD_PASTE_NO_AUTOMATION, {"Paste (no automation)", "Paste items without automation", "", ICON_PASTE}, {KC_PASTE_NO_AUTOMATION}, 0});
    commands.push_back({CMD_REACTIVATE_AUTOMATION, {"Reactivate Automation", "Reactivate automation for all tracks", ""}, {}, 0});
    commands.push_back({CMD_DELETE, {"Delete", "Delete selected items", ""}, {KC_DELETE}, 0});
    commands.push_back({CMD_SELECT_ALL, {"Select All", "Select all items", ""}, {KC_SELECTALL}, 0});
    commands.push_back({CMD_DUPLICATE, {"Duplicate", "Duplicate selected items", "", ICON_DUPLICATE}, {KC_DUPLICATE}, 0});

    commands.push_back({CMD_PREFERENCES, {"Preferences", "Open the preferences window", ""}, {}, 0});
    commands.push_back({CMD_ABOUT, {"About", "Open the about window", ""}, {}, 0});
    commands.push_back({CMD_SHOW_DEBUG_WINDOW, {"Debug", "Open the debug window", ""}, {}, 0});
    commands.push_back({CMD_INSERT_AUDIO_TRACK, {"Insert Audio Track", "Insert a new audio track", "", ICON_PLUS}, {}, 0});
    commands.push_back({CMD_INSERT_MIDI_TRACK, {"Insert MIDI Track", "Insert a new MIDI track", "", ICON_PLUS}, {}, 0});
    commands.push_back({CMD_INSERT_RETURN_TRACK, {"Insert Return Track", "Insert a new return track", "", ICON_PLUS}, {}, 0});
    commands.push_back({CMD_INSERT_MASTER_TRACK, {"Insert Master Track", "Insert a new master track", "", ICON_PLUS}, {}, 0});
    commands.push_back({CMD_OPEN_SECOND_WINDOW, {"Open Second Window", "Open a second window", ""}, {}, 0});
    commands.push_back({CMD_OPEN_VIEW, {"Open View", "Open a new view", ""}, {}, 0});
    commands.push_back({CMD_SET_STARTUP_PROJECT, {"Set Startup Project", "Set the current project as the startup project", ""}, {}, 0});
    commands.push_back({CMD_CREATE_EMPTY_CLIP, {"Create Empty Clip", "Create an empty clip at the cursor position", ""}, {}, 0});
    commands.push_back({CMD_CONSOLIDATE, {"Consolidate", "Consolidate selected items", ""}, {KC_CONSOLIDATE}, 0});
    commands.push_back({CMD_QUANTIZE, {"Quantize", "Quantize selected items", ""}, {KC_QUANTIZE}, 0});
    commands.push_back({CMD_MUTE, {"Mute", "Mute selected items", ""}, {KC_MUTE}, 0});
    commands.push_back({CMD_SOLO, {"Solo", "Solo selected items", ""}, {}, 0});
    std::sort(commands.begin(), commands.end(), [](const Command& a, const Command& b) {
        return a.type < b.type;
    });
    for (int32_t cmdType = CMD_EXIT; cmdType < NUM_COMMANDS; ++cmdType) {
        dbgassert(std::find_if(commands.cbegin(), commands.cend(), [cmdType](auto& cmd) { return cmd.type == cmdType;}) != commands.cend());
    }
}

DAW::UI::Command* DAW::UI::CommandManager::matchKeyCombo(const KeyEvent& evt) {
    for (auto& cmd : commands) {
        for (auto& kc : cmd.keyCombos) {
            if (kc.match(evt)) {
                return &cmd;
            }
        }
    }
    return nullptr;
}
bool KeyEvent::isCommand(GlobalCommandType cmd) const {
    return this->cmd && this->cmd->type == cmd;
}
DAW::UI::Command* DAW::UI::CommandManager::getCommand(GlobalCommandType type) {
    for (auto& cmd : commands) {
        if (cmd.type == type) {
            return &cmd;
        }
    }
    return nullptr;
}
