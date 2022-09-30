#include "guicolors.h"
#include <vector>
#include <algorithm>
#include "math/seq_math.h"
#include "logging.h"
#include "str_util.h"
#include "renderresources.h"
#include "guifonts.h"
#include "guiglobals.h"
#include <nanovg.h>


uint32_t colorPalette[COLOR_PALETTE_LEN] = {
        0xe6b0aa, 0xcd6155, 0xa93226, 0x7b241c, 0xf5b7b1, 0xec7063, 0xcb4335, 0x943126, 0xd7bde2, 0xaf7ac5, 0x884ea0, 0x633974,
        0xd2b4de, 0xa569bd, 0x7d3c98, 0x5b2c6f, 0xa9cce3, 0x5499c7, 0x2471a3, 0x1a5276, 0xaed6f1, 0x5dade2, 0x2e86c1, 0x21618c,
        0xa3e4d7, 0x48c9b0, 0x17a589, 0x117864, 0xa2d9ce, 0x45b39d, 0x138d75, 0x0e6655, 0xa9dfbf, 0x52be80, 0x229954, 0x196f3d,
        0xabebc6, 0x58d68d, 0x28b463, 0x1d8348, 0xf9e79f, 0xf4d03f, 0xd4ac0d, 0x9a7d0a, 0xfad7a0, 0xf5b041, 0xd68910, 0x9c640c,
        0xf5cba7, 0xeb984e, 0xca6f1e, 0x935116, 0xedbb99, 0xdc7633, 0xba4a00, 0x873600, 0xF0F0F0, 0xA0A0A0, 0x606060, 0x050505,
};
uint32_t* colorOnlyPalette   = &colorPalette[8];
uint32_t colorOnlyPaletteLen = COLOR_PALETTE_LEN - 8;
namespace GuiColor {
    static std::vector<constant_t*>& _getConstants() {
        static std::vector<constant_t*> allconstants;
        return allconstants;
    }
    constant_t getConstantById(uint32_t id) {
        auto& v = _getConstants();
        for (auto* c : v) {
            if (c->idx == id) {
                return *c;
            }
        }
        return {};
    }
    constant_t getConstantByName(const String& name) {
        auto& v = _getConstants();
        for (auto* c : v) {
            if (c->name == name) {
                return *c;
            }
        }
        return {};
    }
    std::vector<constant_t> getAllConstants() {
        std::vector<constant_t> v;
        auto constants = _getConstants();
        v.reserve(constants.size());
        for (auto it = constants.begin(); it != constants.end();) {
            v.push_back(*(*it++));
        }
        return v;
    }

    uint32_t getNextId() {
        static uint32_t constantsNextId = 1;
        return constantsNextId++;
    }

    constant_t::constant_t() noexcept
        : idx(0), name(nullptr), defValue(0) {
    }
    constant_t::constant_t(const char* _name, uint32_t _defValue) noexcept
        : idx(getNextId()), name(_name), defValue(_defValue) {
        auto& allconstants = _getConstants();
        allconstants.push_back(this);
    }

    constant_t COL_AUTOMATED("COL_AUTOMATED", 0xffcc0056);
    constant_t COL_KNOB_HIGHLIGHT("COL_KNOB_HIGHLIGHT", 0xFF6200D0);
    constant_t COL_KNOB_HIGHLIGHT_BACKGROUND("COL_KNOB_HIGHLIGHT_BACKGROUND", 0xFF110024);
    constant_t COL_KNOB_MODULATED("COL_KNOB_MODULATED", 0xff00cc56);
    constant_t COL_MODULATION_SATURATED("COL_MODULATION_SATURATED", 0xffffaa07);
    constant_t COL_BASE_BG("COL_BASE_BG", 0xe0101010);
    constant_t COL_BASE_BG_DISABLED("COL_BASE_BG_DISABLED", 0xff030303);
    constant_t COL_BASE_BG_FOCUSED("COL_BASE_BG_FOCUSED", 0xff343e52);
    constant_t COL_BASE_BG_FRAME_BASE("COL_BASE_BG_FRAME_BASE", 0xff151515);
    constant_t COL_BASE_BG_FRAME_BRIGHT("COL_BASE_BG_FRAME_BRIGHT", 0xff343434);
    constant_t COL_BASE_BG_FRAME_HIGHLIGHT("COL_BASE_BG_FRAME_HIGHLIGHT", 0xff1d1d1d);
    constant_t COL_BASE_BG_FRAME_OUTLINE("COL_BASE_BG_FRAME_OUTLINE", 0xff0c0c0c);
    constant_t COL_BASE_BG_HOVER("COL_BASE_BG_HOVER", 0xff505050);
    constant_t COL_BASE_BG_PRESSED("COL_BASE_BG_PRESSED", 0xff0c0c0c);
    constant_t COL_BASE_BG_STROKE("COL_BASE_BG_STROKE", 0x2e030303);
    constant_t COL_BG_BRT("COL_BG_BRT", 0xeb141414);
    constant_t COL_BG_DRK("COL_BG_DRK", 0x9e040505);
    constant_t COL_BG_DRKER("COL_BG_DRKER", 0xff2b2b77);
    constant_t COL_BG_DRKER2("COL_BG_DRKER2", 0xff0f0f0f);
    constant_t COL_BG_DRK_FOCUSED("COL_BG_DRK_FOCUSED", 0xff8d8d8d);
    constant_t COL_BG_SELECTEDTRACK("COL_BG_SELECTEDTRACK", 0x272f7cf7);
    constant_t COL_BG_SELECTEDTRACK_TITLE("COL_BG_SELECTEDTRACK_TITLE", 0xff1ebafd);
    constant_t COL_BG_WIDGET("COL_BG_WIDGET", 0xff0a0d10);
    constant_t COL_BLACK("COL_BLACK", 0x00000000);
    constant_t COL_BTN_BG_BYPASS_ACTIVE("COL_BTN_BG_BYPASS_ACTIVE", 0xff80abc0);
    constant_t COL_BTN_BG_DEFAULT_ACTIVE("COL_BTN_BG_DEFAULT_ACTIVE", 0xff404040);
    constant_t COL_BTN_BG_DEFAULT_INACTIVE("COL_BTN_BG_DEFAULT_INACTIVE", 0xff202020);
    constant_t COL_BTN_BG_SHOW_ACTIVE("COL_BTN_BG_SHOW_ACTIVE", 0xff40abc0);
    constant_t COL_BTN_LOAD_DEF_PLUGINS("COL_BTN_LOAD_DEF_PLUGINS", 0xffffffff);
    constant_t COL_BTN_RECORD_ARM_BG("COL_BTN_RECORD_ARM_BG", 0xff442222);
    constant_t COL_BTN_SOLO_BG_DISABLED("COL_BTN_SOLO_BG_DISABLED", 0xff696a74);
    constant_t COL_BTN_SOLO_BG_ENABLED("COL_BTN_SOLO_BG_ENABLED", 0xffd63131);
    constant_t COL_BTN_SOLO_BG_PARENT("COL_BTN_SOLO_BG_PARENT", 0x6ce37777);
    constant_t COL_CLEAR_COLOR("COL_CLEAR_COLOR", 0xff0f0f0f);
    constant_t COL_CLIPEDITOR_SHARP("COL_CLIPEDITOR_SHARP", 0x39000000);
    constant_t COL_CLIP_NOTE("COL_CLIP_NOTE", 0xffc9f2ff);
    constant_t COL_CLIP_NOTE_MUTED("COL_CLIP_NOTE_MUTED", 0xff121212);
    constant_t COL_CLIP_NOTE_OVERLAP("COL_CLIP_NOTE_OVERLAP", 0xff0000ff);
    constant_t COL_CLIP_OUTLINE("COL_CLIP_OUTLINE", 0x7f000000);
    constant_t COL_CTXTMNU_HILIGHT("COL_CTXTMNU_HILIGHT", 0xff363636);
    constant_t COL_CTXTMNU_OUTLINE("COL_CTXTMNU_OUTLINE", 0xffffffff);
    constant_t COL_DRAGDROPMOVE_HIGHLIGHT("COL_DRAGDROPMOVE_HIGHLIGHT", 0xff00c000);
    constant_t COL_FOLD_BUTTON("COL_FOLD_BUTTON", 0xffff9933);
    constant_t COL_GRID_BRT("COL_GRID_BRT", 0x802e363c);
    constant_t COL_GRID_DRK("COL_GRID_DRK", 0x7b1c1d20);
    constant_t COL_GUI_HANDLE("COL_GUI_HANDLE", 0xffd7d7d7);
    constant_t COL_GUI_HANDLE_FOCUSED("COL_GUI_HANDLE_FOCUSED", 0xff4d60d7);
    constant_t COL_GUI_STROKE("COL_GUI_STROKE", 0xff8c8c8c);
    constant_t COL_INVALID_INPUT("COL_INVALID_INPUT", 0xffc85a5a);
    constant_t COL_KNOB("COL_KNOB", 0xff456cb8);
    constant_t COL_KNOB_BG("COL_KNOB_BG", 0xff0f1114);
    constant_t COL_KNOB_IND("COL_KNOB_IND", 0xffffffff);
    constant_t COL_LABEL_ACTIVE("COL_LABEL_ACTIVE", 0xffffffff);
    constant_t COL_LABEL_AUTOMATION_TRACK("COL_LABEL_AUTOMATION_TRACK", 0xff7f7f7f);
    constant_t COL_LABEL_CONTAINER("COL_LABEL_CONTAINER", 0xffd0d0d0);
    constant_t COL_LABEL_INACTIVE("COL_LABEL_INACTIVE", 0xff808080);
    constant_t COL_LEVEL_IND_GREEN("COL_LEVEL_IND_GREEN", 0xff97ff1d);
    constant_t COL_LEVEL_IND_GREEN_DRK("COL_LEVEL_IND_GREEN_DRK", 0xff4d7012);
    constant_t COL_LEVEL_IND_GREEN_DRKER("COL_LEVEL_IND_GREEN_DRKER", 0xffb3f29b);
    constant_t COL_LEVEL_IND_YELLOW("COL_LEVEL_IND_YELLOW", 0xffff0000);
    constant_t COL_LEVEL_IND_YELLOW_DRK("COL_LEVEL_IND_YELLOW_DRK", 0xffff6363);
    constant_t COL_LEVEL_IND_YELLOW_DRKER("COL_LEVEL_IND_YELLOW_DRKER", 0xffffaa07);
    constant_t COL_LINE_BAR("COL_LINE_BAR", 0x6e000000);
    constant_t COL_LINE_QRT("COL_LINE_QRT", 0x2a000000);
    constant_t COL_LINE_SEPERATOR("COL_LINE_SEPERATOR", 0xcc000000);
    constant_t COL_LINE_XTH("COL_LINE_XTH", 0x20000000);
    constant_t COL_LOOPHANDLES("COL_LOOPHANDLES", 0xff787878);
    constant_t COL_NODES_EDGE("COL_NODES_EDGE", 0xff4c4f6e);
    constant_t COL_NOTE("COL_NOTE", 0xff5fdd77);
    constant_t COL_NOTE_ARP("COL_NOTE_ARP", 0xff744aff);
    constant_t COL_NOTE_MOUSE("COL_NOTE_MOUSE", 0xff00ffff);
    constant_t COL_NOTE_MUTE("COL_NOTE_MUTE", 0xff666666);
    constant_t COL_NOTE_OUTLINE("COL_NOTE_OUTLINE", 0xff000000);
    constant_t COL_NOTE_PLAYING("COL_NOTE_PLAYING", 0xffec6900);
    constant_t COL_NOTE_REALTIME("COL_NOTE_REALTIME", 0xffff00ff);
    constant_t COL_NOTE_SELECTED("COL_NOTE_SELECTED", 0xe8ea2c2c);
    constant_t COL_NOTE_TEXT("COL_NOTE_TEXT", 0xff333333);
    constant_t COL_OFF("COL_OFF", 0xffd2d2f0);
    constant_t COL_ON("COL_ON", 0xffd2f0d2);
    constant_t COL_PIANOROLL_BLACK("COL_PIANOROLL_BLACK", 0xff111111);
    constant_t COL_PIANOROLL_STROKE("COL_PIANOROLL_STROKE", 0x46000000);
    constant_t COL_PIANOROLL_WHITE("COL_PIANOROLL_WHITE", 0xffe4eefd);
    constant_t COL_PLAYHEAD("COL_PLAYHEAD", 0xfffafafa);
    constant_t COL_PLAYHEAD_OUTLINE("COL_PLAYHEAD_OUTLINE", 0xff787878);
    constant_t COL_PLUGIN_VIEW_FRAME("COL_PLUGIN_VIEW_FRAME", 0x7fffffff);
    constant_t COL_PLUG_TITLE("COL_PLUG_TITLE", 0xff4d60d7);
    constant_t COL_PLUG_TITLE_FOCUSED("COL_PLUG_TITLE_FOCUSED", 0xff8f67ff);
    constant_t COL_PLUG_TITLE_SELECTED("COL_PLUG_TITLE_SELECTED", 0xff8f67ff);
    constant_t COL_SELECTION_BACKGROUND("COL_SELECTION_BACKGROUND", 0x60020202);
    constant_t COL_TEXT("COL_TEXT", 0xffe9eafd);
    constant_t COL_TEXTBOX_TEXT("COL_TEXTBOX_TEXT", 0xffe9eafd);
    constant_t COL_TEXTBOX_TEXT_DISABLED("COL_TEXTBOX_TEXT_DISABLED", 0xff666666);
    constant_t COL_TEXTBOX_TEXT_MARKED("COL_TEXTBOX_TEXT_MARKED", 0xff4d60d7);
    constant_t COL_WHITE("COL_WHITE", 0xffe9eafd);

} // namespace GuiColor

NVGcolor rgbaToNvg(uint32_t i);
uint32_t nvgToRGBA(NVGcolor c);
NVGcolor mulSatBright(NVGcolor rgb, float sat, float brt);

namespace UIFont {
    static std::vector<font_type_t*>& _getConstants() noexcept {
        static std::vector<font_type_t*> allconstants;
        return allconstants;
    }
    font_type_t getConstantById(uint32_t id) {
        auto& v = _getConstants();
        for (auto* c : v) {
            if (c->idx == id) {
                return *c;
            }
        }
        return {};
    }
    font_type_t getConstantByName(const String& name) {
        auto& v = _getConstants();
        for (auto* c : v) {
            if (c->name == name) {
                return *c;
            }
        }
        return {};
    }
    std::vector<font_type_t> getAllConstants() {
        std::vector<font_type_t> v;
        auto constants = _getConstants();
        v.reserve(constants.size());
        for (auto it = constants.cbegin(); it != constants.cend();) {
            v.push_back(*(*it++));
        }
        return v;
    }
    uint32_t getNextId() {
        static uint32_t constantsNextId = 1;
        return constantsNextId++;
    }

    font_type_t::font_type_t() noexcept : idx(0), name(nullptr), defValue("") {
    }

    font_type_t::font_type_t(const char* _name, const char* _defValue) noexcept : idx(getNextId()), name(_name), defValue(_defValue) {
        auto& allconstants = _getConstants();
        allconstants.push_back(this);
    }

    const font_type_t FONT_DEFAULT      = font_type_t("FONT_DEFAULT", "Roboto-Regular.ttf");
    const font_type_t FONT_LABEL        = font_type_t("FONT_LABEL", "Roboto-Regular.ttf");
    const font_type_t FONT_TEXTFIELD    = font_type_t("FONT_TEXTFIELD", "Roboto-Regular.ttf");
    const font_type_t FONT_CONTEXT_MENU = font_type_t("FONT_CONTEXT_MENU", "Roboto-Regular.ttf");
    const font_type_t FONT_DECIMAL      = font_type_t("FONT_DECIMAL", "Roboto-Regular.ttf");
    const font_type_t FONT_TEST         = font_type_t("FONT_TEST", "jbmononf.ttf");

    void bindFont(NVGcontext* ctx, UIFont::font_instance font) {
        RenderResources::NvgFonts& fonts = RenderResources::perContextFonts[ctx];
        if (font.fontInstanceIdx == -1) {
            font.fontInstanceIdx = -2;
            int i                = 0;
            for (auto& f : fonts.fontsLoaded) {
                if (f.name == font.name) {
                    font.fontInstanceIdx = i;
                    break;
                }
                i++;
            }
        }
        if (fonts.fontsLoaded.empty()) {
            return;
        }
        const auto fontIdx = math::clamp<uint32_t>(font.fontInstanceIdx, 0, fonts.fontsLoaded.size());
        auto& fontloaded  = fonts.fontsLoaded[fontIdx];
        if (fontloaded.nvgId == -999) {
            log_lf(Log::L_DEBUG, "loading font %s %s\n", StringAsCStr(fontloaded.font.name), StringAsCStr(fontloaded.font.path));
            fontloaded.nvgId = nvgCreateFont(ctx, StringAsCStr(fontloaded.font.name), StringAsCStr(fontloaded.font.path));
        }
        nvgFontFaceId(ctx, fontloaded.nvgId);
    }

} // namespace UIFont
