#pragma once
#include <vector>
#include "gui/gui.h"
#include "str_util.h"
#include "keyboard.h"
#include "mouse.h"
#include "event.h"
#include <vector>
#include <unordered_map>

namespace DAW::UI {

struct KeybindsSnapshot {
    int32_t version = 0;
    std::unordered_map<String, std::vector<KeyCombo>> keyCombos;
};

KeybindsSnapshot loadKeybindsFile();
void saveKeybindsFile(KeybindsSnapshot& _settings);

struct CommandDesc {
    String name;
    String description;
    String shortcut;
    int32_t iconId = -1;
};
enum class CommandContextType {
    CMD_CTXT_GLOBAL,
    CMD_CTXT_FOCUSED,
    CMD_CTXT_CTR_TYPE,
    CMD_CTXT_INTERNAL,
};

struct CommandContext {
    GlobalCommandType type = GlobalCommandType::CMD_NONE;
    KeyEvent kevt{};
    int32_t argInt = 0;
    String argStr = "";
};
struct Command {
    struct CmdCtxtMatcher {
        CommandContextType ctxtType = CommandContextType::CMD_CTXT_GLOBAL;
        gui_type ctxtGuiType = gui_type::GUI_TYPE_UNKNOWN;
        bool matchesFocusedGui(guibase* optionalGui) const;
    };
    GlobalCommandType type = GlobalCommandType::CMD_NONE;
    CommandDesc desc;
    std::vector<KeyCombo> keyCombos; // TODO: make keyCombos struct of KeyCombo and CommandContext
    int initOrder = 0;
    KeyCombo defaultKeyCombo{};
    CmdCtxtMatcher contextMatcher{};
    int32_t keybindContextDataArg0 = 0;
    String keybindContextDataArg1 = "";
    String toString() const {
        return desc.name;
    }
    const KeyCombo* getFirstKeyCombo() const {
        if (keyCombos.empty()) {
            return nullptr;
        }
        return &keyCombos[0];
    }
    const CmdCtxtMatcher& getContextMatcher() const {
        return contextMatcher;
    }
    CommandContext getKeybindContextData(const KeyEvent& kevt) const {
        return {type, kevt, keybindContextDataArg0, keybindContextDataArg1};
    }
};
class CommandManager {
    std::vector<Command> commands;
public:
    void updateKeybinds();
    void init();
    Command* matchKeyCombo(const KeyEvent& evt);
    Command* getCommand(GlobalCommandType type);
    template<typename T>
    void visitCommandBindings(T&& visitor) {
        for (auto& cmd : commands) {
            if (cmd.contextMatcher.ctxtType != CommandContextType::CMD_CTXT_INTERNAL) {
                visitor(cmd);
            }
        }
    }
    void resetKeybinds();
    void saveKeybinds();
    void loadKeybinds();
    void storeCommandKeybinds(KeybindsSnapshot& out);
    void loadCommandKeybinds(const KeybindsSnapshot& data);
};
} // namespace DAW::UI