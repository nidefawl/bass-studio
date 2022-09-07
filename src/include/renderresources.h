#pragma once
#include <unordered_map>
#include "fileio.h"

#define ICON_SYNTH 0
#define ICON_EFFECT 1
#define ICON_FOLDER 2
#define ICON_FOLDER_OPEN 3
#define ICON_FILE 4
#define ICON_COPY 5
#define ICON_PASTE 6
#define ICON_CUT 7
#define ICON_ADJUST 8
#define ICON_CLOSE 9
#define ICON_BYPASS 10
#define ICON_LOOP 11
#define IMG_LED 12
#define IMG_LED_OFF 13
#define IMG_LED_GLOW 14
#define ICON_ARR_DOWN 15
#define ICON_ARR_LEFT 16
#define ICON_ARR_RIGHT 17
#define ICON_PLUS 18
#define ICON_MINUS 19
#define ICON_AUTOMATION 20
#define ICON_SAVE 21
#define ICON_ARR_UP 22
#define ICON_SPEAKER 23
#define ICON_X 24
#define ICON_DAW_EXE 25
#define ICON_OPT_LOCKED 26
#define ICON_OPT_UNLOCKED 27
#define ICON_MIDIPLUG 28
#define ICON_DUPLICATE 29
#define ICON_SYNTH_SMALL 30
#define ICON_WARNING 31
#define NUM_IMGS 32
#define MAX_FONTS 256

struct NVGcontext;
namespace RenderResources {
    struct FontDesc {
        String name;
        String path;
    };
    struct LoadedFont {
        bool loaded;
        int nvgId;
        String name;
        FontDesc font;
    };
    struct NvgImageTexture {
        std::unordered_map<NVGcontext*, int32_t> perContextId;
        int width;
        int height;
    };
    struct NvgFonts {
        std::vector<FontDesc> fontsInstalled;
        std::vector<LoadedFont> fontsLoaded;
    };
    extern NvgImageTexture imgDashedLine;
    extern NvgImageTexture imgIcons[NUM_IMGS];
    extern std::unordered_map<NVGcontext*, NvgFonts> perContextFonts;
    extern std::vector<FontDesc> fontsInstalled;
}// namespace RenderResources
