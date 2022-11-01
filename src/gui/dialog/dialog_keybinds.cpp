#include "assert_dbg.h"
#include "dialog_io.h"
#include "appsettings.h"
#include "event.h"
#include "gui/controls/button.h"
#include "dialog.h"
#include "gui/controls/scrollbar.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/controls/textfield.h"
#include "gui/container/scrollcontainer.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/dropdown/dropdown.h"
#include "gui/views/controls.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "guiglobals.h"
#include "host/audiohost/audio_host.h"
#include "host/daw/mainctrl.h"
#include "host/midihost/midi_host.h"
#include "host/host_pluginmanager.h"
#include "gui/controls/list.h"
#include "keyboard.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "host/meter/meter.h"
#include "platform.h"
#include "renderresources.h"
#include "seq_util.h"
#include "str_util.h"
#include "tls.h"
#include "types.h"
#include <cstdint>

namespace DAW::DialogSettings {

class guibutton_keybind_update : public guibutton {
    KeyEvent lastEvent;
    bool bIsRecording = false;
    public:
    guibutton_keybind_update() : guibutton() {
        setText("Set Keybind");
        clearFlagInternal(FLG_HAS_COLOR_BG);
        buttonColor = GuiColor::COL_BTN_RECORD_ARM_BG;
    }
    void beginRecording() {
        setText("Press Key");
        drawFn   = drawRecordSymbol;
        setFlagInternal(FLG_HAS_COLOR_BG);
        lastEvent = KeyEvent();
        bIsRecording = true;
    }
    void endRecording() {
        setText("Set Keybind");
        clearFlagInternal(FLG_HAS_COLOR_BG);
        drawFn   = nullptr;
        if (!bIsRecording)
            return;
        bIsRecording = false;
        parentCtrl->focusGui(parent);
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        // if (parent)
        //     parent->buttonClicked(this);
    }
    void handleRightClick(MouseEvent& evt) override {
        // if (parent)
        //     parent->rightClicked(evt, this);
    }
    bool focusEvent(MouseHitEvt& evt, bool focused) override {
        if (focused) {
            beginRecording();
        } else if (bIsRecording) {
            endRecording();
        }
        return guibutton::focusEvent(evt, focused);
    }
    bool handleCharInput(uint32_t codepoint) override {
        return guibutton::handleCharInput(codepoint);
    }
    bool handleKeyInput(KeyEvent& kevt) override {
        if (kevt.type == K_PRESS) {
            if (isAltKey(kevt.keyCode)) {
                return true;
            }
            if (isCtrlKey(kevt.keyCode)) {
                return true;
            }
            if (isShiftKey(kevt.keyCode)) {
                return true;
            }
            if (kevt.keyCode == KeyboardKey::DAW_KB_ENTER) {
                endRecording();
                return true;
            }
            if (kevt.keyCode == KeyboardKey::DAW_KB_ESCAPE) {
                endRecording();
                return true;
            }
            lastEvent = kevt;
            KeyCombo kc = KeyCombo::FromKeyEvent(kevt);
            setText(kc.toString());
            parent->buttonClicked(this);
        }
        return true;
    }
    const KeyEvent& getLastKeyEvent() {
        return lastEvent;
    }
};
class guidialog_keybinds : public setting_dialog {
public:
    class gui_keybind_entry : public gui_list_entry {
        guidialog_keybinds* const parentDialog;
        UI::Command* const command;
        int32_t keyComboIdx;
        KeyCombo* keyBinding = nullptr;
        String title;
        public:
        gui_keybind_entry(guidialog_keybinds* _parentDialog, UI::Command* _command, int32_t buttonId, int32_t _keyComboIdx)
            : gui_list_entry(),
            parentDialog(_parentDialog), command(_command), keyComboIdx(_keyComboIdx)
        {
            if (assert_expr(command)) {
                if (assert_expr(keyComboIdx >= 0 && keyComboIdx < CtrSize(command->keyCombos))) {
                    keyBinding = &command->keyCombos[keyComboIdx];
                }
                title = command->desc.name;
                icon = command->desc.iconId;
                setTooltipText(command->desc.description);
            }
            id = buttonId;
        }
        UI::Command* getCommand() {
            return command;
        }
        const KeyCombo& getKeyCombo() const {
            return *keyBinding;
        }
        int32_t getKeyComboIdx() const {
            return keyComboIdx;
        }
        String getText() override { return title; }
        void dragMoveOn(guibase* target, ivec2 mousepos) override {}
        void dragReleaseOn(guibase* target, ivec2 mousepos) override {}
        void handleDraggedBegin(MouseEvent& evt) override { toggle(); parent->buttonClicked(this); }
        void toggle() {
        }
        bool focused() const override;
        void render(NVGcontext* vg) override {
            auto spacing = INSET_TITLE;
            if (focused()) {
                nvgBeginPath(vg);
                nvgRect(vg, pos.x+spacing/2, pos.y, size.x-spacing, size.y);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER));
                nvgFill(vg);
            }
            nvgTranslate(vg, pos.x + spacing, pos.y);
            float rowHeight = size.y;
            if (icon > -1) {
                RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
                drawIcon(vg, ivec2(size.y), &image);
            }

            renderText(vg,
                    vec2(size.y + spacing, size.y * 0.5f),
                    vec2(size.x*0.5f, size.y),
                    getText(),
                    rowHeight);

            renderTextLabel(vg,
                            vec2(size.x - spacing*3, size.y * 0.5f),
                            vec2(size.x*0.5f, size.y),
                            keyBinding->toString(),
                            theme,
                            rowHeight,
                            theme->getColor(GuiColor::COL_LABEL_ACTIVE),
                            NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);

            nvgTranslate(vg, -(pos.x + spacing), -pos.y);
        }
        void determineSize(glm::ivec2& prefSize) override {
            const auto height = theme->get(GuiConstant::CONST_ROW_HEIGHT);
            prefSize.y = height;
        }
    };
private:
    DawInstance* const daw;
    guictr_scrollbar listKeybinds;
    guictr_vert_layout ctrEditSelectedKeybind;
    gui_textfield* textFieldDesc;
    gui_textfield* textFieldCurrentBinding;
    guibutton_keybind_update* btnSetKeybind;
    guibutton* btnClearKeybind;
    guibutton* btnResetKeybind;
    UI::Command* currentCommand = nullptr;
    int32_t currentKeybindIdx = -1;
    static const int32_t BTN_ID_SET = 1;
    static const int32_t BTN_ID_CLEAR = 2;
    static const int32_t BTN_ID_RESET = 3;
public:
    explicit guidialog_keybinds(DawInstance* _daw)
        : setting_dialog(),
        daw(_daw),
        ctrEditSelectedKeybind(0)
    {
        setCanMouseHit(false);
        setBackgroundRendered(false);
        padding = 12;
        margin  = 0;
        listKeybinds.padding = 0;
        listKeybinds.margin = 0;
        setGuiType(gui_type::CTR_TYPE_KEYBINDS);
        listKeybinds.maxHeight = -1;
        listKeybinds.setCanMouseHit(true);
        listKeybinds.setBackgroundRendered(true);
        listKeybinds.setLabel("Select Command");
        listKeybinds.setFlag(FLG_RENDER_LABEL, true);
        listKeybinds.setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        // listKeybinds.scrollbarOutside = true;
        ctrEditSelectedKeybind.setCanMouseHit(true);
        ctrEditSelectedKeybind.setBackgroundRendered(true);
        ctrEditSelectedKeybind.setLabel("Edit keybind");
        ctrEditSelectedKeybind.setFlag(FLG_RENDER_LABEL, true);
        btnSetKeybind = new guibutton_keybind_update{};
        btnSetKeybind->id = BTN_ID_SET;
        btnClearKeybind = new guibutton{};
        btnClearKeybind->setText("Clear");
        btnClearKeybind->id = BTN_ID_CLEAR;
        btnResetKeybind = new guibutton{};
        btnResetKeybind->setText("Reset");
        btnResetKeybind->id = BTN_ID_RESET;
        textFieldDesc = new gui_textfield{};
        textFieldDesc->id = 4;
        textFieldCurrentBinding = new gui_textfield{};
        textFieldCurrentBinding->id = 5;
        ctrEditSelectedKeybind.addElement({0.25f, textFieldDesc});
        ctrEditSelectedKeybind.addElement({0.25f, textFieldCurrentBinding});
        ctrEditSelectedKeybind.addElement({0.25f, btnSetKeybind});
        ctrEditSelectedKeybind.addElement({0.125f, btnClearKeybind});
        ctrEditSelectedKeybind.addElement({0.125f, btnResetKeybind});
        add(&listKeybinds);
        add(&ctrEditSelectedKeybind);
        btnSetKeybind->setText("Set Keybind");
        textFieldDesc->setEnabled(false);
        textFieldCurrentBinding->setEnabled(false);
        for (auto gui : ctrEditSelectedKeybind.guis) {
            gui->setEnabled(false);
        }
        initList();
    }

    void initList() {
        listKeybinds.destroyGuis();
        auto commandMgr = daw->getCommandManager();
        int32_t buttonId = 16;
        commandMgr->visitCommandBindings([&](auto& cmd) {
            int32_t len = CtrSize(cmd.keyCombos);
            if (&cmd == currentCommand) {
                if (currentKeybindIdx < 0 || currentKeybindIdx >=len) {
                    currentKeybindIdx = len - 1;
                }
            }
            for (int32_t i = 0; i < len; ++i) {
                listKeybinds.add(new gui_keybind_entry(this, &cmd, buttonId++, i));
            }
        });
        
    }

    void reinitList() {
        initList();
        layout();
    }

    ~guidialog_keybinds() override {
        listKeybinds.destroyGuis();
        removeGuis();
    }

    void onDialogShow() override { }
    
    void layout() override {
        auto innerPad = INSET_TITLE;
        ctrEditSelectedKeybind.setLayoutPadding(vec2(1, 2));
        ctrEditSelectedKeybind.padding = 0;
        auto titleHeight    = theme->get(GuiConstant::CONST_FONT_SIZE_CTR_LABEL);
        const auto heightEditCtr = theme->get(GuiConstant::CONST_ROW_HEIGHT)+innerPad*2 + titleHeight;
        const auto cs = getSizeContent();
        ctrEditSelectedKeybind.pos = {0, titleHeight};
        ctrEditSelectedKeybind.size = ivec2(cs.x, heightEditCtr - titleHeight);
        listKeybinds.pos   = {0, ctrEditSelectedKeybind.bottom() + innerPad + titleHeight};
        listKeybinds.size  = cs - ivec2(0, listKeybinds.pos.y);
        listKeybinds.determineSize(listKeybinds.size);

        guictr_base::layout();
    }
    void buttonClicked(guibase* button) override {
        auto cmdManager = daw->getCommandManager();
        UI::Command* cmd = currentCommand;
        if (cmd && button->id == BTN_ID_SET) {
            while (currentKeybindIdx >= CtrSize(cmd->keyCombos)) {
                cmd->keyCombos.push_back({});
            }
            guibutton_keybind_update* btn = static_cast<guibutton_keybind_update*>(button);
            auto kevt = btn->getLastKeyEvent();
            KeyCombo kc = KeyCombo::FromKeyEvent(kevt);
            if (currentKeybindIdx > -1 && currentKeybindIdx < CtrSize(cmd->keyCombos)) {
                cmd->keyCombos[currentKeybindIdx] = kc;
            }
            textFieldCurrentBinding->setValue(kc.toString());
            cmdManager->updateKeybinds();
            cmdManager->saveKeybinds();
            btn->endRecording();
        } else if (cmd && button->id == BTN_ID_CLEAR) {
            if (currentKeybindIdx > 0 && currentKeybindIdx < CtrSize(cmd->keyCombos)) {
                cmd->keyCombos.erase(cmd->keyCombos.begin() + currentKeybindIdx);
            } else if (!cmd->keyCombos.empty()) {
                cmd->keyCombos[0] = {};
            }
            cmdManager->updateKeybinds();
            cmdManager->saveKeybinds();
            reinitList();
        } else if (cmd && button->id == BTN_ID_RESET) {
            if (currentKeybindIdx == 0 && currentKeybindIdx < CtrSize(cmd->keyCombos)) {
                cmd->keyCombos[currentKeybindIdx] = cmd->defaultKeyCombo;
            } else if (currentKeybindIdx > 0 && currentKeybindIdx < CtrSize(cmd->keyCombos)) {
                cmd->keyCombos.erase(cmd->keyCombos.begin() + currentKeybindIdx);
            }
            cmdManager->updateKeybinds();
            cmdManager->saveKeybinds();
            reinitList();
        } else if (button->id >= 16) {
            auto entry = dynamic_cast<gui_keybind_entry*>(button);
            if (assert_expr(entry)) {
                cmd = entry->getCommand();
                btnClearKeybind->setEnabled(true);
                btnResetKeybind->setEnabled(true);
                btnSetKeybind->setEnabled(true);
                currentCommand = cmd;
                currentKeybindIdx = entry->getKeyComboIdx();
                textFieldCurrentBinding->setValue(entry->getKeyCombo().toString());
            }
        }
        if (cmd) {
            auto desc = cmd->desc.description;
            setTooltipText(desc);
            textFieldDesc->setTooltipText(desc);
            // textFieldCurrentBinding->setTooltipText("Default Keybind: " + cmd->defaultKeyCombo.toString());
            btnResetKeybind->setTooltipText("Default Keybind: " + cmd->defaultKeyCombo.toString());
            textFieldDesc->setValue(desc);
            ctrEditSelectedKeybind.setLabel("Edit keybind: " + cmd->desc.name);
        }
    }
};

bool guidialog_keybinds::gui_keybind_entry::focused() const {
    dbgassert(parentDialog);
    if (parentCtrl->isCtrOrChildFocused(parentDialog)) {
        if (parentDialog->currentCommand == this->command) {
            return true;
        }
    }
    return false;
}

setting_dialog* makeKeybindsDialog(DawInstance* daw) {
    return new guidialog_keybinds(daw);
}

} // namespace DAW::DialogSettings