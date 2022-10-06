#include "keyboard.h"
#include "assert_dbg.h"
#include "commands.h"
#include "event.h"
#include "logging.h"
#include "renderresources.h"
#include "seq_util.h"
#include <algorithm>
#include <array>

KeyCombo KC_UNDO                = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "z" };
KeyCombo KC_REDO                = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "y" };
KeyCombo KC_COPY                = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "c" };
KeyCombo KC_CONSOLIDATE         = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "j" };
KeyCombo KC_QUANTIZE            = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "q" };
KeyCombo KC_PASTE               = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "v" };
KeyCombo KC_PASTE_NO_AUTOMATION = { KB_MOD_CTRL | KB_MOD_ALT, KeyboardKey::DAW_KB_UNKNOWN, "v" };
KeyCombo KC_CUT                 = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "x" };
KeyCombo KC_DUPLICATE           = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "d" };
KeyCombo KC_DELETE              = { 0, KeyboardKey::DAW_KB_DELETE, "" };
KeyCombo KC_SELECTALL           = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "a" };
KeyCombo KC_MUTE                = { 0, KeyboardKey::DAW_KB_KP_0, "" };
KeyCombo KC_REFRESH             = { 0, KeyboardKey::DAW_KB_F5, "" };
KeyCombo KC_SAVE                = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "s" };
KeyCombo KC_SAVEAS              = { KB_MOD_CTRL | KB_MOD_ALT, KeyboardKey::DAW_KB_UNKNOWN, "s" };
KeyCombo KC_OPEN                = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "o" };
KeyCombo KC_NEW                 = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "n" };
KeyCombo KC_ZOOM_IN             = { 0, KeyboardKey::DAW_KB_KP_ADD, "" };
KeyCombo KC_ZOOM_OUT            = { 0, KeyboardKey::DAW_KB_KP_SUBTRACT, "" };

String KeyEvent::toString() const {
    return StringFormat("KeyEvent: type=%d, keyCode=%d, scancode=%d, mods=%d, keyname=%s", type, keyCode, scancode, mods, keyname);
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
    } else {
        return modKeys + StringFormat("Keycode %d", keyCode);
    }
    return modKeys;
}

bool KeyEvent::isCommand(GlobalCommandType cmd) const {
    return this->cmd && this->cmd->type == cmd;
}

namespace DAW::UI {
    std::array<const char*, 512> GlobalKeyNames;
    void InitKeynames() {
        std::memset(GlobalKeyNames.data(), 0, GlobalKeyNames.size() * sizeof(char*));
        auto SetKeyname = [](KeyboardKey key, const char* name) {
            GlobalKeyNames[static_cast<int>(key)] = name;
        };
        SetKeyname(KeyboardKey::DAW_KB_ENTER, "Enter");
        SetKeyname(KeyboardKey::DAW_KB_ESCAPE, "Escape");
        SetKeyname(KeyboardKey::DAW_KB_TAB, "Tab");
        SetKeyname(KeyboardKey::DAW_KB_SPACE, "Space");
        SetKeyname(KeyboardKey::DAW_KB_BACKSPACE, "Backspace");
        SetKeyname(KeyboardKey::DAW_KB_INSERT, "Insert");
        SetKeyname(KeyboardKey::DAW_KB_DELETE, "Delete");
        SetKeyname(KeyboardKey::DAW_KB_HOME, "Home");
        SetKeyname(KeyboardKey::DAW_KB_END, "End");
        SetKeyname(KeyboardKey::DAW_KB_PAGE_DOWN, "Page Down");
        SetKeyname(KeyboardKey::DAW_KB_PAGE_UP, "Page Up");
        SetKeyname(KeyboardKey::DAW_KB_KP_0, "Numpad 0");
        SetKeyname(KeyboardKey::DAW_KB_KP_1, "Numpad 1");
        SetKeyname(KeyboardKey::DAW_KB_KP_2, "Numpad 2");
        SetKeyname(KeyboardKey::DAW_KB_KP_3, "Numpad 3");
        SetKeyname(KeyboardKey::DAW_KB_KP_4, "Numpad 4");
        SetKeyname(KeyboardKey::DAW_KB_KP_5, "Numpad 5");
        SetKeyname(KeyboardKey::DAW_KB_KP_6, "Numpad 6");
        SetKeyname(KeyboardKey::DAW_KB_KP_7, "Numpad 7");
        SetKeyname(KeyboardKey::DAW_KB_KP_8, "Numpad 8");
        SetKeyname(KeyboardKey::DAW_KB_KP_9, "Numpad 9");
        SetKeyname(KeyboardKey::DAW_KB_KP_ADD, "Numpad Add");
        SetKeyname(KeyboardKey::DAW_KB_KP_DECIMAL, "Numpad Decimal");
        SetKeyname(KeyboardKey::DAW_KB_KP_DIVIDE, "Numpad Divide");
        SetKeyname(KeyboardKey::DAW_KB_KP_ENTER, "Numpad Enter");
        SetKeyname(KeyboardKey::DAW_KB_KP_EQUAL, "Numpad Equal");
        SetKeyname(KeyboardKey::DAW_KB_KP_MULTIPLY, "Numpad Multiply");
        SetKeyname(KeyboardKey::DAW_KB_KP_SUBTRACT, "Numpad Subtract");
        SetKeyname(KeyboardKey::DAW_KB_F1, "F1");
        SetKeyname(KeyboardKey::DAW_KB_F2, "F2");
        SetKeyname(KeyboardKey::DAW_KB_F3, "F3");
        SetKeyname(KeyboardKey::DAW_KB_F4, "F4");
        SetKeyname(KeyboardKey::DAW_KB_F5, "F5");
        SetKeyname(KeyboardKey::DAW_KB_F6, "F6");
        SetKeyname(KeyboardKey::DAW_KB_F7, "F7");
        SetKeyname(KeyboardKey::DAW_KB_F8, "F8");
        SetKeyname(KeyboardKey::DAW_KB_F9, "F9");
        SetKeyname(KeyboardKey::DAW_KB_F10, "F10");
        SetKeyname(KeyboardKey::DAW_KB_F11, "F11");
        SetKeyname(KeyboardKey::DAW_KB_F12, "F12");
        SetKeyname(KeyboardKey::DAW_KB_LEFT, "Left");
        SetKeyname(KeyboardKey::DAW_KB_RIGHT, "Right");
        SetKeyname(KeyboardKey::DAW_KB_UP, "Up");
        SetKeyname(KeyboardKey::DAW_KB_DOWN, "Down");
        SetKeyname(KeyboardKey::DAW_KB_MENU, "Menu");
        SetKeyname(KeyboardKey::DAW_KB_CAPS_LOCK, "Caps Lock");
        SetKeyname(KeyboardKey::DAW_KB_NUM_LOCK, "Num Lock");
        SetKeyname(KeyboardKey::DAW_KB_SCROLL_LOCK, "Scroll Lock");
        SetKeyname(KeyboardKey::DAW_KB_PAUSE, "Pause");
        SetKeyname(KeyboardKey::DAW_KB_PRINT_SCREEN, "Print Screen");

    }

    void CommandManager::updateKeybinds() {
        std::sort(commands.begin(), commands.end(), [](const Command& a, const Command& b) {
            return a.type < b.type;
        });
        for (int32_t cmdType = CMD_EXIT; cmdType < NUM_COMMANDS; ++cmdType) {
            dbgassert(std::find_if(commands.cbegin(), commands.cend(), [cmdType](auto& cmd) { return cmd.type == cmdType;}) != commands.cend());
        }
        for (auto& cmd : commands) {
            if (cmd.keyCombos.empty()) {
                cmd.desc.shortcut = "";
            } else {
                cmd.desc.shortcut = cmd.keyCombos[0].toString();
            }
            for (auto& kc : cmd.keyCombos) {
                auto ptr = GlfwKeycodeToString(kc.keyCode, 0);
                if (ptr) {
                    kc.keyChar = ptr;
                }
            }
        }
    }

    void CommandManager::resetKeybinds() {
        commands.clear();
        commands.push_back({CMD_DELETE, {"Delete", "Delete selected items", ""}, {KC_DELETE}});
        commands.push_back({CMD_FILE_NEW, {"New", "Create a new project", "", ICON_FILE}, {KC_NEW}});
        commands.push_back({CMD_FILE_OPEN, {"Open", "Open a project", "", ICON_FOLDER}, {KC_OPEN}});
        commands.push_back({CMD_FILE_SAVE, {"Save", "Save the project", "", ICON_SAVE}, {KC_SAVE}});
        commands.push_back({CMD_FILE_SAVEAS, {"Save As", "Save the project as a new file", "", ICON_SAVE}, {KC_SAVEAS}});
        commands.push_back({CMD_FILE_CLOSE, {"Close", "Close the project", "", ICON_CLOSE}, {}});
        commands.push_back({CMD_EXIT, {"Exit", "Exit the application", "", ICON_CLOSE}, {}});
        commands.push_back({CMD_UNDO, {"Undo", "Undo last action", ""}, {KC_UNDO}});
        commands.push_back({CMD_REDO, {"Redo", "Redo last action", ""}, {KC_REDO}});
        commands.push_back({CMD_GUI_GLOBAL_ZOOM_DECREASE, {"Zoom Out", "Decrease the global zoom level", "", ICON_MINUS}, {KC_ZOOM_OUT}});
        commands.push_back({CMD_GUI_GLOBAL_ZOOM_INCREASE, {"Zoom In", "Increase the global zoom level", "", ICON_PLUS}, {KC_ZOOM_IN}});
        commands.push_back({CMD_CUT, {"Cut", "Cut selected items", "", ICON_CUT}, {KC_CUT}});
        commands.push_back({CMD_COPY, {"Copy", "Copy selected items", "", ICON_COPY}, {KC_COPY}});
        commands.push_back({CMD_PASTE, {"Paste", "Paste items", "", ICON_PASTE}, {KC_PASTE}});
        commands.push_back({CMD_PASTE_NO_AUTOMATION, {"Paste (no automation)", "Paste items without automation", "", ICON_PASTE}, {KC_PASTE_NO_AUTOMATION}});
        commands.push_back({CMD_REACTIVATE_AUTOMATION, {"Reactivate Automation", "Reactivate automation for all tracks", ""}, {}});
        commands.push_back({CMD_SELECT_ALL, {"Select All", "Select all items", ""}, {KC_SELECTALL}});
        commands.push_back({CMD_DUPLICATE, {"Duplicate", "Duplicate selected items", "", ICON_DUPLICATE}, {KC_DUPLICATE}});
        commands.push_back({CMD_PREFERENCES, {"Preferences", "Open the preferences window", ""}, {}});
        commands.push_back({CMD_ABOUT, {"About", "Open the about window", ""}, {}});
        commands.push_back({CMD_SHOW_DEBUG_WINDOW, {"Debug", "Open the debug window", ""}, {}});
        commands.push_back({CMD_INSERT_AUDIO_TRACK, {"Insert Audio Track", "Insert a new audio track", "", ICON_PLUS}, {}});
        commands.push_back({CMD_INSERT_MIDI_TRACK, {"Insert MIDI Track", "Insert a new MIDI track", "", ICON_PLUS}, {}});
        commands.push_back({CMD_INSERT_RETURN_TRACK, {"Insert Return Track", "Insert a new return track", "", ICON_PLUS}, {}});
        commands.push_back({CMD_INSERT_MASTER_TRACK, {"Insert Master Track", "Insert a new master track", "", ICON_PLUS}, {}});
        commands.push_back({CMD_OPEN_SECOND_WINDOW, {"Open Second Window", "Open a second window", ""}, {}});
        commands.push_back({CMD_OPEN_VIEW, {"Open View", "Open a new view", ""}, {}});
        commands.push_back({CMD_SET_STARTUP_PROJECT, {"Set Startup Project", "Set the current project as the startup project", ""}, {}});
        commands.push_back({CMD_CREATE_EMPTY_CLIP, {"Create Empty Clip", "Create an empty clip at the cursor position", ""}, {}});
        commands.push_back({CMD_CONSOLIDATE, {"Consolidate", "Consolidate selected items", ""}, {KC_CONSOLIDATE}});
        commands.push_back({CMD_QUANTIZE, {"Quantize", "Quantize selected items", ""}, {KC_QUANTIZE}});
        commands.push_back({CMD_MUTE, {"Mute", "Mute selected items", ""}, {KC_MUTE}});
        commands.push_back({CMD_SOLO, {"Solo", "Solo selected items", ""}, {}});
        commands.push_back({CMD_SWITCH_VIEW, {"Switch View", "Switch to the next view", ""}, {{ KB_MODS_NONE, KeyboardKey::DAW_KB_TAB, "" }}});
        commands.push_back({CMD_STARTSTOP_PLAYBOCK, {"Start/Stop Playback", "Start or stop playback", ""}, {{ KB_MODS_NONE, KeyboardKey::DAW_KB_SPACE, "" }}});

        /* for (int32_t i = 0; i < (int)KeyboardKey::DAW_NUM_KNOWN_KEYBOARD_KEYS; ++i) {
            auto ptr = GlfwKeycodeToString((KeyboardKey)i, 0);
            log_printf("%04d\t%s\n", i, ptr);

        } */
        for (auto& cmd : commands) {
            for (auto& kc : cmd.keyCombos) {
                auto ptr = GlfwKeycodeToString(kc.keyCode, 0);
                if (ptr) {
                    kc.keyChar = ptr;
                }
            }
            if (cmd.keyCombos.empty()) {
                cmd.keyCombos.push_back({});
            }
            cmd.defaultKeyCombo = cmd.keyCombos[0];
        }
    }

    Command* CommandManager::matchKeyCombo(const KeyEvent& evt) {
        for (auto& cmd : commands) {
            for (auto& kc : cmd.keyCombos) {
                if (kc.match(evt)) {
                    return &cmd;
                }
            }
        }
        return nullptr;
    }

    Command* CommandManager::getCommand(GlobalCommandType type) {
        for (auto& cmd : commands) {
            if (cmd.type == type) {
                return &cmd;
            }
        }
        return nullptr;
    }

    void CommandManager::storeCommandKeybinds(KeybindsSnapshot& out) {
        auto& map = out.keyCombos;
        for (const auto& cmd : commands) {
            map[cmd.desc.name] = cmd.keyCombos;
        }
    }

    void CommandManager::loadCommandKeybinds(const KeybindsSnapshot& data) {
        auto& map = data.keyCombos;
        for (auto& cmd : commands) {
            auto it = map.find(cmd.desc.name);
            if (it != map.end()) {
                cmd.keyCombos = it->second;
                if (cmd.keyCombos.empty()) {
                    cmd.keyCombos.push_back({});
                }
            }
        }
        updateKeybinds();
    }

    void CommandManager::saveKeybinds() {
        try {
            KeybindsSnapshot data;
            data.version = 1;
            storeCommandKeybinds(data);
            saveKeybindsFile(data);
        } catch (std::exception& e) {
            log_lf(Log::L_ERROR, "Failed saving keybinds file %s: %s\n", StringAsCStr(App::Platform::toUserdataPath(KEYBIND_SETTINGS_FILENAME)), e.what());
        }
    }
    void CommandManager::loadKeybinds() {
        try {
            KeybindsSnapshot data = loadKeybindsFile();
            if (data.version > 0) {
                loadCommandKeybinds(data);
                updateKeybinds();
                return;
            }
        } catch (std::exception& e) {
            log_lf(Log::L_WARN, "Using default keybinds: %s\n", e.what());
        }
        resetKeybinds();
        updateKeybinds();
    }

    void CommandManager::init() {
        resetKeybinds();
        try {
            KeybindsSnapshot data = loadKeybindsFile();
            if (data.version > 0) {
                loadCommandKeybinds(data);
            }
        } catch (std::exception& e) {
            log_lf(Log::L_WARN, "Using default keybinds: %s\n", e.what());
        }
        updateKeybinds();
    }
}// namespace DAW::UI
