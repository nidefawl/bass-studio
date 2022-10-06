#pragma once
#include <vector>
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
struct CommandContext {
    int dummy = 0;
};
struct Command {
    GlobalCommandType type = GlobalCommandType::CMD_NONE;
    CommandDesc desc;
    std::vector<KeyCombo> keyCombos;
    int group = 0;
    KeyCombo defaultKeyCombo{};
    CommandContext context{};
    String toString() const {
        return desc.name;
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