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
};
struct CommandContext {
    CommandContextType ctxtType = CommandContextType::CMD_CTXT_GLOBAL;
    gui_type ctxtGuiType = gui_type::GUI_TYPE_UNKNOWN;
    int32_t argInt = 0;
    bool matchesFocusedGui(guibase* optionalGui) const;
};
struct Command {
    GlobalCommandType type = GlobalCommandType::CMD_NONE;
    CommandDesc desc;
    std::vector<KeyCombo> keyCombos;
    int initOrder = 0;
    KeyCombo defaultKeyCombo{};
    CommandContext context{};
    String toString() const {
        return desc.name;
    }
    const KeyCombo* getFirstKeyCombo() const {
        if (keyCombos.empty()) {
            return nullptr;
        }
        return &keyCombos[0];
    }
    const CommandContext& getContext() const {
        return context;
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
    void visitCommands(T&& visitor) {
        for (auto& cmd : commands) {
            visitor(cmd);
        }
    }
    template<typename T>
    void visitCommands(T&& visitor) const {
        for (const auto& cmd : commands) {
            visitor(cmd);
        }
    }
    void resetKeybinds();
    void saveKeybinds();
    void loadKeybinds();
    void storeCommandKeybinds(KeybindsSnapshot& out);
    void loadCommandKeybinds(const KeybindsSnapshot& data);
};
} // namespace DAW::UI