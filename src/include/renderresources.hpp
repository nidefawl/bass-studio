#pragma once
#include <unordered_map>
#include "fileio.hpp"

enum ICON : int32_t {
    ICON_SYNTH = 0,
    ICON_EFFECT,
    ICON_FOLDER,
    ICON_FOLDER_OPEN,
    ICON_FILE,
    ICON_COPY,
    ICON_PASTE,
    ICON_CUT,
    ICON_DELETE,
    ICON_ADJUST,
    ICON_CLOSE,
    ICON_BYPASS,
    ICON_LOOP,
    IMG_LED,
    IMG_LED_OFF,
    IMG_LED_GLOW,
    ICON_ARR_DOWN,
    ICON_ARR_LEFT,
    ICON_ARR_RIGHT,
    ICON_PLUS,
    ICON_MINUS,
    ICON_AUTOMATION,
    ICON_SAVE,
    ICON_ARR_UP,
    ICON_SPEAKER,
    ICON_X,
    ICON_DAW_EXE,
    ICON_OPT_LOCKED,
    ICON_OPT_UNLOCKED,
    ICON_MIDIPLUG,
    ICON_DUPLICATE,
    ICON_SYNTH_SMALL,
    ICON_WARNING,
    ICON_MODULATION,
    ICON_LOADING,
    ICON_MODULATION_INPUT,
    ICON_FILE_AUDIO,
    ICON_FILE_MIDI,
    ICON_CHECKBOX_UNCHECKED,
    ICON_CHECKBOX_CHECKED,
    ICON_SEARCH,
    ICON_DEBUG,
    ICON_HISTORY,
    ICON_KEYBOARD,
    ICON_EXPORT,
    ICON_SETTINGS,
    ICON_PERFORMANCE,
    ICON_THEME,
    NUM_IMGS,
};

struct NVGcontext;
namespace RenderResources {
    struct FontDesc {
        String name;
        String path;
    };
    struct LoadedFont {
        FontDesc font;
        String name;
        int nvgId   = -1;
        bool loaded = false;
    };
    struct NvgImageTexture {
        std::unordered_map<NVGcontext*, int32_t> perContextId;
        int width  = 0;
        int height = 0;
        int flags  = 0;
    };
    struct NvgFonts {
        std::vector<FontDesc> fontsInstalled;
        std::vector<LoadedFont> fontsLoaded;
    };
    extern LoadedFont emojiFont;
    extern NvgImageTexture imgDashedLine;
    extern NvgImageTexture imgIcons[NUM_IMGS];
    extern std::unordered_map<NVGcontext*, NvgFonts> perContextFonts;
    extern std::vector<FontDesc> fontsInstalled;
    const NvgImageTexture* loadTexture(NVGcontext* vg, const String& path, const int flags);
}// namespace RenderResources

extern const SupportedFileTypes FILE_TYPES_IMAGES;
