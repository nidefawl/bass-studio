#pragma once
#include <array>
#include "assert_dbg.h"
#include "math/vec.h"
#include "mouse.h"
#include "str_util.h"

class guibase;
namespace DAW::UI {
    struct Command;
    extern std::array<const char*, 512> GlobalKeyNames;
    void InitKeynames();
}

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

enum KeyboardState {
    K_RELEASE = 0,
    K_PRESS,
    K_REPEAT,
};

enum class KeyboardKey : int32_t {
    DAW_KB_INVALID       = -2,
    DAW_KB_UNKNOWN       = -1,
    DAW_KB_SPACE         = 32,
    DAW_KB_APOSTROPHE    = 39, /* ' */
    DAW_KB_COMMA         = 44, /* , */
    DAW_KB_MINUS         = 45, /* - */
    DAW_KB_PERIOD        = 46, /* . */
    DAW_KB_SLASH         = 47, /* / */
    DAW_KB_0             = 48,
    DAW_KB_1             = 49,
    DAW_KB_2             = 50,
    DAW_KB_3             = 51,
    DAW_KB_4             = 52,
    DAW_KB_5             = 53,
    DAW_KB_6             = 54,
    DAW_KB_7             = 55,
    DAW_KB_8             = 56,
    DAW_KB_9             = 57,
    DAW_KB_SEMICOLON     = 59, /* ; */
    DAW_KB_EQUAL         = 61, /* = */
    DAW_KB_A             = 65,
    DAW_KB_B             = 66,
    DAW_KB_C             = 67,
    DAW_KB_D             = 68,
    DAW_KB_E             = 69,
    DAW_KB_F             = 70,
    DAW_KB_G             = 71,
    DAW_KB_H             = 72,
    DAW_KB_I             = 73,
    DAW_KB_J             = 74,
    DAW_KB_K             = 75,
    DAW_KB_L             = 76,
    DAW_KB_M             = 77,
    DAW_KB_N             = 78,
    DAW_KB_O             = 79,
    DAW_KB_P             = 80,
    DAW_KB_Q             = 81,
    DAW_KB_R             = 82,
    DAW_KB_S             = 83,
    DAW_KB_T             = 84,
    DAW_KB_U             = 85,
    DAW_KB_V             = 86,
    DAW_KB_W             = 87,
    DAW_KB_X             = 88,
    DAW_KB_Y             = 89,
    DAW_KB_Z             = 90,
    DAW_KB_LEFT_BRACKET  = 91,  /* [ */
    DAW_KB_BACKSLASH     = 92,  /* \ */
    DAW_KB_RIGHT_BRACKET = 93,  /* ] */
    DAW_KB_GRAVE_ACCENT  = 96,  /* ` */
    DAW_KB_WORLD_1       = 161, /* non-US #1 */
    DAW_KB_WORLD_2       = 162, /* non-US #2 */
    DAW_KB_ESCAPE        = 256,
    DAW_KB_ENTER         = 257,
    DAW_KB_TAB           = 258,
    DAW_KB_BACKSPACE     = 259,
    DAW_KB_INSERT        = 260,
    DAW_KB_DELETE        = 261,
    DAW_KB_RIGHT         = 262,
    DAW_KB_LEFT          = 263,
    DAW_KB_DOWN          = 264,
    DAW_KB_UP            = 265,
    DAW_KB_PAGE_UP       = 266,
    DAW_KB_PAGE_DOWN     = 267,
    DAW_KB_HOME          = 268,
    DAW_KB_END           = 269,
    DAW_KB_CAPS_LOCK     = 280,
    DAW_KB_SCROLL_LOCK   = 281,
    DAW_KB_NUM_LOCK      = 282,
    DAW_KB_PRINT_SCREEN  = 283,
    DAW_KB_PAUSE         = 284,
    DAW_KB_F1            = 290,
    DAW_KB_F2            = 291,
    DAW_KB_F3            = 292,
    DAW_KB_F4            = 293,
    DAW_KB_F5            = 294,
    DAW_KB_F6            = 295,
    DAW_KB_F7            = 296,
    DAW_KB_F8            = 297,
    DAW_KB_F9            = 298,
    DAW_KB_F10           = 299,
    DAW_KB_F11           = 300,
    DAW_KB_F12           = 301,
    DAW_KB_F13           = 302,
    DAW_KB_F14           = 303,
    DAW_KB_F15           = 304,
    DAW_KB_F16           = 305,
    DAW_KB_F17           = 306,
    DAW_KB_F18           = 307,
    DAW_KB_F19           = 308,
    DAW_KB_F20           = 309,
    DAW_KB_F21           = 310,
    DAW_KB_F22           = 311,
    DAW_KB_F23           = 312,
    DAW_KB_F24           = 313,
    DAW_KB_F25           = 314,
    DAW_KB_KP_0          = 320,
    DAW_KB_KP_1          = 321,
    DAW_KB_KP_2          = 322,
    DAW_KB_KP_3          = 323,
    DAW_KB_KP_4          = 324,
    DAW_KB_KP_5          = 325,
    DAW_KB_KP_6          = 326,
    DAW_KB_KP_7          = 327,
    DAW_KB_KP_8          = 328,
    DAW_KB_KP_9          = 329,
    DAW_KB_KP_DECIMAL    = 330,
    DAW_KB_KP_DIVIDE     = 331,
    DAW_KB_KP_MULTIPLY   = 332,
    DAW_KB_KP_SUBTRACT   = 333,
    DAW_KB_KP_ADD        = 334,
    DAW_KB_KP_ENTER      = 335,
    DAW_KB_KP_EQUAL      = 336,
    DAW_KB_LEFT_SHIFT    = 340,
    DAW_KB_LEFT_CONTROL  = 341,
    DAW_KB_LEFT_ALT      = 342,
    DAW_KB_LEFT_SUPER    = 343,
    DAW_KB_RIGHT_SHIFT   = 344,
    DAW_KB_RIGHT_CONTROL = 345,
    DAW_KB_RIGHT_ALT     = 346,
    DAW_KB_RIGHT_SUPER   = 347,
    DAW_KB_MENU          = 348,
    DAW_NUM_KNOWN_KEYBOARD_KEYS
};
inline KeyboardKey GetKeyboardKeyOffset(KeyboardKey key, int32_t offset) {
    auto offsetKey = static_cast<int32_t>(key) + offset;
    dbgassert(offsetKey >= 0 && offsetKey < static_cast<int32_t>(KeyboardKey::DAW_NUM_KNOWN_KEYBOARD_KEYS) && "Invalid keyboard key offset");
    return static_cast<KeyboardKey>(offsetKey);
}

enum KeyboardMods {
    KB_MODS_NONE = 0,
    KB_MOD_SHIFT = 0x0001,
    KB_MOD_CTRL = 0x0002,
    KB_MOD_ALT = 0x0004,
    KB_MOD_SUPER = 0x0008,
};

// Define command key for windows/mac/linux
#if defined(__APPLE__) || defined(DOXYGEN_DOCUMENTATION_BUILD)
/// If on OSX, maps to ``GLFW_MOD_SUPER``.  Otherwise, maps to ``GLFW_MOD_CONTROL``.
#define KB_MOD_SYSTEM KB_MOD_SUPER
#else
#define KB_MOD_SYSTEM KB_MOD_CTRL
#endif


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
    guibase* guiDragged; // TODO: Use a SafeRef<guibase> instead
    ivec2 mousepos;
    ivec2 relMousepos;
    ivec2 dragStart;
    ivec2 dragOffset;
    ivec2* dragDistance;
    KeyboardMods kbmods;
};
class MouseHitEvt {
    guibase* guiHit    = nullptr;
    guibase* draggedThing = nullptr;
    int cursorIcon     = CURSOR_DEFAULT;
    bool cursorChanged = false;

public:
    MouseHitType type;
    KeyboardMods kbmods;
    MouseHitEvt(MouseHitType _type, KeyboardMods _kbmods) : type(_type), kbmods(_kbmods) {
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

enum GlobalCommandType {
    CMD_NONE = 0,
    CMD_EXIT = 1,
    CMD_FILE_NEW,
    CMD_FILE_OPEN,
    CMD_FILE_SAVE,
    CMD_FILE_SAVEAS,
    CMD_BUNDLE_PROJECT_DIRECTORY,
    CMD_BUNDLE_PROJECT_ZIP,
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
    CMD_BEGIN_RENAME,
    CMD_SELECT_ALL,
    CMD_PREFERENCES,
    CMD_ABOUT,
    CMD_SHOW_DEBUG_WINDOW,
    CMD_INSERT_MASTER_TRACK,
    CMD_INSERT_RETURN_TRACK,
    CMD_INSERT_MIDI_TRACK,
    CMD_INSERT_AUDIO_TRACK,
    CMD_IMPORT_TRACK,
    CMD_EXPORT_TRACK,
    CMD_OPEN_SECOND_WINDOW,
    CMD_CREATE_VIEW,
    CMD_SWITCH_LAYOUT,
    CMD_REACTIVATE_AUTOMATION,
    CMD_SET_STARTUP_PROJECT,
    CMD_CREATE_EMPTY_CLIP,
    CMD_SWITCH_VIEW,
    CMD_STARTSTOP_PLAYBOCK,
    CMD_SET_COLOR,
    CMD_SET_NAME,
    CMD_MOVE_CURSOR,
    CMD_RESET_UI_DEFAULT_LAYOUT,
    NUM_COMMANDS
};

struct KeyEvent {
    KeyboardState type = KeyboardState::K_PRESS;
    KeyboardKey keyCode = KeyboardKey::DAW_KB_INVALID;
    int scancode{};
    KeyboardMods mods = KeyboardMods::KB_MODS_NONE;
    const char* keyname = nullptr;
    DAW::UI::Command* cmd = nullptr;
    String toString() const;
};
