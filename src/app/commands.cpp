#include "commands.h"
#include "assert_dbg.h"
#include "event.h"
#include "keyboard.h"
#include "logging.h"
#include "renderresources.h"
#include "seq_util.h"
#include "str_util.h"
#include <algorithm>
#include <array>

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
            return a.initOrder < b.initOrder;
        });
        for (int32_t cmdType = CMD_EXIT; cmdType < NUM_COMMANDS; ++cmdType) {
            dbgassert(std::find_if(commands.cbegin(), commands.cend(), [cmdType](auto& cmd) { return cmd.type == cmdType;}) != commands.cend());
        }
        for (auto& cmd : commands) {
            if (cmd.contextMatcher.ctxtType == CommandContextType::CMD_CTXT_INTERNAL)
                cmd.keyCombos.clear();
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
        KeyCombo KC_SAVE                = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "s" };
        KeyCombo KC_SAVEAS              = { KB_MOD_CTRL | KB_MOD_ALT, KeyboardKey::DAW_KB_UNKNOWN, "s" };
        KeyCombo KC_OPEN                = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "o" };
        KeyCombo KC_NEW                 = { KB_MOD_CTRL, KeyboardKey::DAW_KB_UNKNOWN, "n" };
        KeyCombo KC_ZOOM_IN             = { 0, KeyboardKey::DAW_KB_KP_ADD, "" };
        KeyCombo KC_ZOOM_OUT            = { 0, KeyboardKey::DAW_KB_KP_SUBTRACT, "" };
        commands.clear();
        auto Add = [this](GlobalCommandType type, CommandDesc&& desc, const KeyCombo& defaultCombo) {
            this->commands.push_back({type, desc, {defaultCombo}, 0, defaultCombo, {}, -1, -1, ""});
        };
        Add(CMD_SWITCH_VIEW, {"Switch View", "Switch to the next view", ""}, { KB_MODS_NONE, KeyboardKey::DAW_KB_TAB, "" });
        Add(CMD_STARTSTOP_PLAYBOCK, {"Start/Stop Playback", "Start or stop playback", ""}, { KB_MODS_NONE, KeyboardKey::DAW_KB_SPACE, "" });
        Add(CMD_REACTIVATE_AUTOMATION, {"Reactivate Automation", "Reactivate automation for all tracks", ""}, {});
        Add(CMD_CREATE_EMPTY_CLIP, {"Create Empty Clip", "Create an empty clip at the cursor position", ""}, {});
        Add(CMD_INSERT_AUDIO_TRACK, {"Insert Audio Track", "Insert a new audio track", "", ICON_PLUS}, {});
        Add(CMD_INSERT_MIDI_TRACK, {"Insert MIDI Track", "Insert a new MIDI track", "", ICON_PLUS}, {});
        Add(CMD_INSERT_RETURN_TRACK, {"Insert Return Track", "Insert a new return track", "", ICON_PLUS}, {});
        Add(CMD_INSERT_MASTER_TRACK, {"Insert Master Track", "Insert a new master track", "", ICON_PLUS}, {});
        Add(CMD_IMPORT_TRACK, {"Import Single Track", "Import a track from a file", "", ICON_FOLDER}, {});
        Add(CMD_EXPORT_TRACK, {"Export Single Track", "Export a track to a file", "", ICON_SAVE}, {});
        Add(CMD_EXPORT_AUDIO, {"Export Audio", "Render the project to an audio file", "", ICON_SAVE}, {});
        Add(CMD_UNDO, {"Undo", "Undo last action", ""}, KC_UNDO);
        Add(CMD_REDO, {"Redo", "Redo last action", ""}, KC_REDO);
        Add(CMD_SELECT_ALL, {"Select All", "Select all items", ""}, KC_SELECTALL);
        Add(CMD_QUANTIZE, {"Quantize", "Quantize selected items", ""}, KC_QUANTIZE);
        Add(CMD_SET_COLOR, {"Set Color", "Set color on selected items", ""}, {});
        Add(CMD_SET_NAME, {"Set Name", "Set name on selected items", ""}, {});
        Add(CMD_BEGIN_RENAME, {"Rename", "Rename selected items", ""}, {});
        Add(CMD_CONSOLIDATE, {"Consolidate", "Consolidate selected items", ""}, KC_CONSOLIDATE);
        Add(CMD_MUTE, {"Mute", "Mute selected items", ""}, KC_MUTE);
        Add(CMD_SOLO, {"Solo", "Solo selected items", ""}, {});
        Add(CMD_DELETE, {"Delete", "Delete selected items", "", ICON_DELETE}, KC_DELETE);
        Add(CMD_DUPLICATE, {"Duplicate", "Duplicate selected items", "", ICON_DUPLICATE}, KC_DUPLICATE);
        Add(CMD_COPY, {"Copy", "Copy selected items", "", ICON_COPY}, KC_COPY);
        Add(CMD_CUT, {"Cut", "Cut selected items", "", ICON_CUT}, KC_CUT);
        Add(CMD_PASTE, {"Paste", "Paste items", "", ICON_PASTE}, KC_PASTE);
        Add(CMD_PASTE_NO_AUTOMATION, {"Paste (no automation)", "Paste items without automation", "", ICON_PASTE}, KC_PASTE_NO_AUTOMATION);
        Add(CMD_DELETE_TIME, {"Delete Time", "Delete time", "", ICON_DELETE}, {});
        Add(CMD_INSERT_TIME, {"Insert Time", "Insert time", "", ICON_PASTE}, {});
        Add(CMD_RENDER_TO_AUDIO, {"Render Audio", "Render to new Audio Track"}, {});

        Add(CMD_FILE_NEW, {"New", "Create a new project", "", ICON_FILE}, KC_NEW);
        Add(CMD_FILE_OPEN, {"Open", "Open a project", "", ICON_FOLDER}, KC_OPEN);
        Add(CMD_FILE_SAVE, {"Save", "Save the project", "", ICON_SAVE}, KC_SAVE);
        Add(CMD_FILE_SAVEAS, {"Save As", "Save the project as a new file", "", ICON_SAVE}, {});
        Add(CMD_BUNDLE_PROJECT_DIRECTORY, {"Bundle (Directory)", "Bundle the project to a directory", "", ICON_SAVE}, {});
        Add(CMD_BUNDLE_PROJECT_ZIP, {"Bundle (zip)", "Bundle the project as zip file", "", ICON_SAVE}, {});
        Add(CMD_FILE_CLOSE, {"Close", "Close the project", "", ICON_CLOSE}, {});
        Add(CMD_EXIT, {"Exit", "Exit the application", "", ICON_CLOSE}, {});

        Add(CMD_GUI_GLOBAL_ZOOM_DECREASE, {"Zoom Out", "Decrease the global zoom level", "", ICON_MINUS}, KC_ZOOM_OUT);
        Add(CMD_GUI_GLOBAL_ZOOM_INCREASE, {"Zoom In", "Increase the global zoom level", "", ICON_PLUS}, KC_ZOOM_IN);

        Add(CMD_SET_STARTUP_PROJECT, {"Set Startup Project", "Set the current project as the startup project", ""}, {});
        Add(CMD_PREFERENCES, {"Preferences", "Open the preferences window", ""}, {});
        Add(CMD_ABOUT, {"About", "Open the about window", ""}, {});
        Add(CMD_SHOW_DEBUG_WINDOW, {"Debug", "Open the debug window", ""}, {});
        Add(CMD_OPEN_SECOND_WINDOW, {"Open Second Window", "Open a second window", ""}, {});
        Add(CMD_CREATE_VIEW, {"Open View", "Open a new view", ""}, {});
        Add(CMD_MOVE_CURSOR, {"Move cursor", "Move the cursor position", ""}, {});
        Add(CMD_RESET_UI_DEFAULT_LAYOUT, {"Reset UI Layout", "Reset the UI layout to the default layout", ""}, {});
        Add(CMD_APPLY_PYTHON_SCRIPT, {"Apply Python Script", "Apply a python script", ""}, {});
        Add(CMD_APPLY_GROOVE, {"Apply groove", "Apply groove to clip", ""}, {});
        Add(CMD_APPLY_ARP, {"Apply arp", "Apply arp to clip", ""}, {});
        Add(CMD_NOTE_ARP_RESET, {"Reset Arp", "Note resets arp (Toggle)", ""}, {});

        auto cmdOpenView = Command{CMD_SWITCH_LAYOUT, {"Switch Layout", "Switch to Layout %d. Hold Shift Key to store", ""}, {}, 0, {}, {}, 0, 0, ""};
        for (int32_t i = 0; i < 10; i++) {
            auto cmdOpenView1 = cmdOpenView;
            cmdOpenView1.desc.name = "Switch to Layout " + std::to_string(i + 1);
            cmdOpenView1.desc.description = StringFormat(cmdOpenView1.desc.description.c_str(), i + 1);
            cmdOpenView1.keybindContextDataArg0 = i;
            if (i < 4) {
                // bind F1-F4 to open the first 4 views
                auto key = GetKeyboardKeyOffset(KeyboardKey::DAW_KB_F1, i);
                cmdOpenView1.keyCombos = {{ KB_MODS_NONE, key, "" }};
            }
            commands.push_back(cmdOpenView1);
        }
        /* for (int32_t i = 0; i < (int)KeyboardKey::DAW_NUM_KNOWN_KEYBOARD_KEYS; ++i) {
            auto ptr = GlfwKeycodeToString((KeyboardKey)i, 0);
            log_printf("%04d\t%s\n", i, ptr);

        } */
        auto ctxtMatcherFocusedOnly = Command::CmdCtxtMatcher{CommandContextType::CMD_CTXT_FOCUSED, gui_type::GUI_TYPE_UNKNOWN};
        int32_t idx = 0;
        for (auto& cmd : commands) {
            cmd.initOrder = idx++;
            switch (cmd.type) {
                case CMD_SET_COLOR:
                case CMD_SET_NAME:
                case CMD_MOVE_CURSOR:
                case CMD_APPLY_PYTHON_SCRIPT:
                    cmd.contextMatcher.ctxtType = CommandContextType::CMD_CTXT_INTERNAL;
                    cmd.keyCombos.clear();
                    break;
                case CMD_COPY:
                case CMD_CUT:
                // case CMD_PASTE:
                case CMD_PASTE_NO_AUTOMATION:
                case CMD_DUPLICATE:
                case CMD_CONSOLIDATE:
                case CMD_QUANTIZE:
                case CMD_MUTE:
                case CMD_BEGIN_RENAME:
                    cmd.contextMatcher = ctxtMatcherFocusedOnly;
                    break;
                default:
                    break;
            }

            switch (cmd.type) {
                case CMD_SOLO:
                case CMD_SWITCH_LAYOUT:
                    cmd.contextMatcher.optionalShiftKey = true;
                    break;
                default:
                    break;
            }
            for (auto& kc : cmd.keyCombos) {
                auto ptr = GlfwKeycodeToString(kc.keyCode, 0);
                if (ptr) {
                    kc.keyChar = ptr;
                }
            }
            auto ptr = GlfwKeycodeToString(cmd.defaultKeyCombo.keyCode, 0);
            if (ptr) {
                cmd.defaultKeyCombo.keyChar = ptr;
            }
        }
    }

    bool IsKeyEventKeyComboMatch(const KeyCombo& kc, const KeyEvent& kevt, bool optionalShiftKey) {
        int modsA = kevt.mods;
        if (optionalShiftKey) modsA &= ~KB_MOD_SHIFT;
        if (modsA != kc.keyMod) {
            return false;
        }
        if (!kc.keyChar.empty()) {
            return kevt.keyname && kc.keyChar == kevt.keyname;
        } else {
            return kevt.keyCode == kc.keyCode;
        }
    }
    Command* CommandManager::matchKeyCombo(const KeyEvent& evt) {
        for (auto& cmd : commands) {
            for (auto& kc : cmd.keyCombos) {
                if (IsKeyEventKeyComboMatch(kc, evt, cmd.contextMatcher.optionalShiftKey)) {
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
    bool Command::CmdCtxtMatcher::matchesFocusedGui(guibase* optionalGui) const {
        using Type = CommandContextType;
        bool bMatches = false;
        switch (ctxtType) {
            case Type::CMD_CTXT_CTR_TYPE:
                bMatches = optionalGui && optionalGui->getGuiType() == ctxtGuiType;
                break;
            case Type::CMD_CTXT_GLOBAL:
            case Type::CMD_CTXT_FOCUSED:
            case Type::CMD_CTXT_INTERNAL:
                bMatches = true;
                break;
        }
        return bMatches;
    }
}// namespace DAW::UI
