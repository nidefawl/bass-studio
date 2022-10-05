#pragma once

#include "math/vec.h"
#include "mouse.h"
#include "str_util.h"

class guibase;
namespace DAW::UI {
    struct Command;
}

enum GlobalCommandType {
    CMD_NONE = 0,
    CMD_EXIT = 1,
    CMD_FILE_NEW,
    CMD_FILE_OPEN,
    CMD_FILE_SAVE,
    CMD_FILE_SAVEAS,
    CMD_FILE_CLOSE,
    CMD_GUI_GLOBAL_ZOOM_DECREASE,
    CMD_GUI_GLOBAL_ZOOM_INCREASE,
    CMD_UNDO,
    CMD_REDO,
    CMD_CUT,
    CMD_COPY,
    CMD_PASTE,
    CMD_PASTE_NO_AUTOMATION,
    CMD_DELETE,
    CMD_DUPLICATE,
    CMD_CONSOLIDATE,
    CMD_MUTE,
    CMD_QUANTIZE,
    CMD_SOLO,
    CMD_SELECT_ALL,
    CMD_PREFERENCES,
    CMD_ABOUT,
    CMD_SHOW_DEBUG_WINDOW,
    CMD_INSERT_AUDIO_TRACK,
    CMD_INSERT_MIDI_TRACK,
    CMD_INSERT_RETURN_TRACK,
    CMD_INSERT_MASTER_TRACK,
    CMD_OPEN_SECOND_WINDOW,
    CMD_OPEN_VIEW,
    CMD_REACTIVATE_AUTOMATION,
    CMD_SET_STARTUP_PROJECT,
    CMD_CREATE_EMPTY_CLIP,
    NUM_COMMANDS
};

enum MouseEventType {
    M_EVT_BTN_DOWN,
    M_EVT_BTN_UP,
    M_EVT_DOUBLECLICK,
    M_EVT_MOVE,
    M_EVT_CAPTURED_MOVE,
    M_EVT_SCROLL,
};
enum MouseHitType {
    MOUSE_OVER,
    MOUSE_LEFT,
    MOUSE_RIGHT,
    MOUSE_DRAGDROP_CLIP,
    MOUSE_DRAGDROP_OBJECT,
    MOUSE_SCROLL,
    MOUSE_BTN_3,
    MOUSE_BTN_4,
    MOUSE_BTN_5
};
inline MouseHitType fromButton(const int button) {
    switch (button) {
        case 0:
            return MOUSE_LEFT;
        case 1:
            return MOUSE_RIGHT;
        case 2:
            return MOUSE_BTN_3;
        case 3:
            return MOUSE_BTN_4;
        case 4:
            return MOUSE_BTN_5;
    }
    return MOUSE_BTN_3;
}
struct MouseEvent {
    MouseEventType type;
    int button;
    guibase* guiDragged;
    ivec2 mousepos;
    ivec2 relMousepos;
    ivec2 dragStart;
    ivec2 dragOffset;
    ivec2* dragDistance;
    int kbmods;
};
class MouseHitEvt {
    guibase* guiHit    = nullptr;
    guibase* draggedThing = nullptr;
    int cursorIcon     = CURSOR_DEFAULT;
    bool cursorChanged = false;

public:
    MouseHitType type;
    int kbmods;
    MouseHitEvt(MouseHitType _type, int _kbmods) : type(_type), kbmods(_kbmods) {
    }
    void requestFocus(guibase* gui) {
        guiHit = gui;
    }
    guibase* getGuiHit() {
        return guiHit;
    }
    int getCursor() {
        return cursorIcon;
    }
    bool hasCursorChanged() {
        return cursorChanged;
    }
    void requestCursor(int _cursorIcon) {
        cursorChanged = true;
        cursorIcon    = _cursorIcon;
    }
    guibase* getDraggedThing() {
        return draggedThing;
    }
    void setDraggedThing(guibase* _draggedThing) {
        draggedThing = _draggedThing;
    }
};
enum KeyEventType {
    K_RELEASE,
    K_PRESS,
    K_REPEAT,
};
struct KeyEvent {
    KeyEventType type = K_PRESS;
    int keyCode{};
    int scancode{};
    int mods{};
    const char* keyname = nullptr;
    DAW::UI::Command* cmd = nullptr;
    String toString() const;
    bool isCommand(GlobalCommandType cmd) const;
};
