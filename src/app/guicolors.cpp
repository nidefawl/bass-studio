#include "guicolors.h"
#include <vector>
#include <algorithm>
#include "math/seq_math.h"
#include "logging.h"
#include "str_util.h"
#include "renderresources.h"
#include "guifonts.h"
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
    constant_t getConstantById(int32_t id) {
        auto& v = _getConstants();
        for (auto* c : v) {
            if (c->idx == id) {
                return *c;
            }
        }
        return constant_t();
    }
    constant_t getConstantByName(String name) {
        auto& v = _getConstants();
        for (auto* c : v) {
            if (c->name == name) {
                return *c;
            }
        }
        return constant_t();
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
    void changeConstantDefault(const constant_t& c, int32_t v) {
        for (auto p : _getConstants()) {
            if (p == &c) {
                p->defValue = v;
            } else if (p->idx == c.idx) {
                my_printf("failed changing default for constant %d\n", p->idx);
            }
        }
    }
    int32_t getNextId() {
        static int32_t constantsNextId = 1;
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
    constant_t COL_BASE_BG                 = constant_t("COL_BASE_BG", 0xFF1A1A1A);
    constant_t COL_BASE_BG_HOVER           = constant_t("COL_BASE_BG_HOVER", 0xFF000000);
    constant_t COL_BASE_BG_PRESSED         = constant_t("COL_BASE_BG_PRESSED", 0xFF0C0C0C);
    constant_t COL_BASE_BG_FOCUSED         = constant_t("COL_BASE_BG_FOCUSED", 0xFF4C4C4C);
    constant_t COL_BASE_BG_DISABLED        = constant_t("COL_BASE_BG_DISABLED", 0xFF000000);
    constant_t COL_BASE_BG_FRAME_BASE      = constant_t("COL_BASE_BG_FRAMEBASE", 0xFF171717);
    constant_t COL_BASE_BG_FRAME_BRIGHT    = constant_t("COL_BASE_BG_FRAME_BRIGHT", 0xFF272727);
    constant_t COL_BASE_BG_FRAME_OUTLINE   = constant_t("COL_BASE_BG_FRAME_OUTLINE", 0xFF060606);
    constant_t COL_BASE_BG_FRAME_HIGHLIGHT = constant_t("COL_BASE_BG_FRAME_HIGHLIGHT", 0xFF1D1D1D);
    constant_t COL_BASE_BG_STROKE          = constant_t("COL_BASE_BG_STROKE", 0x339B9B9B);

    constant_t COL_GRID_DRK               = constant_t("COL_GRID_DRK", 0xFF000000);
    constant_t COL_GRID_BRT               = constant_t("COL_GRID_BRT", 0xFF000000);
    constant_t COL_LINE_BAR               = constant_t("COL_LINE_BAR", 0xFF000000);
    constant_t COL_LINE_QRT               = constant_t("COL_LINE_QRT", 0xFF000000);
    constant_t COL_LINE_XTH               = constant_t("COL_LINE_XTH", 0xFF000000);
    constant_t COL_BG_DRK                 = constant_t("COL_BG_DRK", 0xFF000000);
    constant_t COL_BG_BRT                 = constant_t("COL_BG_BRT", 0xFF000000);
    constant_t COL_LINE_SEPERATOR         = constant_t("COL_LINE_SEPERATOR", 0xFF000000);
    constant_t COL_CTXTMNU_OUTLINE        = constant_t("COL_CTXTMNU_OUTLINE", 0xFF000000);
    constant_t COL_CTXTMNU_BG             = constant_t("COL_CTXTMNU_BG", 0xFF000000);
    constant_t COL_CTXTMNU_HILIGHT        = constant_t("COL_CTXTMNU_HILIGHT", 0xFF000000);
    constant_t COL_GUI_STROKE             = constant_t("COL_GUI_STROKE", 0xFF000000);
    constant_t COL_BG_DRK_FOCUSED         = constant_t("COL_BG_DRK_FOCUSED", 0xFF000000);
    constant_t COL_NOTE                   = constant_t("COL_NOTE", 0xFF000000);
    constant_t COL_NOTE_PLAYING           = constant_t("COL_NOTE_PLAYING", 0xFF000000);
    constant_t COL_NOTE_ARP               = constant_t("COL_NOTE_ARP", 0xFF000000);
    constant_t COL_NOTE_MUTE              = constant_t("COL_NOTE_MUTE", 0xFF000000);
    constant_t COL_NOTE_OUTLINE           = constant_t("COL_NOTE_OUTLINE", 0xFF000000);
    constant_t COL_NOTE_SELECTED          = constant_t("COL_NOTE_SELECTED", 0xFFAA88AA);
    constant_t COL_NOTE_TEXT              = constant_t("COL_NOTE_TEXT", 0xFF000000);
    constant_t COL_BG_SELECTEDTRACK       = constant_t("COL_BG_SELECTEDTRACK", 0xFF000000);
    constant_t COL_BG_SELECTEDTRACK_TITLE = constant_t("COL_BG_SELECTEDTRACK_TITLE", 0xFF000000);
    constant_t COL_BG_DRKER               = constant_t("COL_BG_DRKER", 0xFF000000);
    constant_t COL_GUI_HANDLE             = constant_t("COL_GUI_HANDLE", 0xFF000000);
    constant_t COL_GUI_HANDLE_FOCUSED     = constant_t("COL_GUI_HANDLE_FOCUSED", 0xFF323232);
    constant_t COL_BG_DRKER2              = constant_t("COL_BG_DRKER2", 0xFF000000);
    constant_t COL_NODES_EDGE             = constant_t("COL_NODES_EDGE", 0xFF323232);
    constant_t COL_CLEAR_COLOR            = constant_t("COL_CLEAR_COLOR", 0xFF000000);
    constant_t COL_LABEL_ACTIVE           = constant_t("COL_LABEL_ACTIVE", 0xFF000000);
    constant_t COL_LABEL_INACTIVE         = constant_t("COL_LABEL_INACTIVE", 0xFF000000);
    constant_t COL_WHITE("COL_WHITE", 0xFFFFFFFF);
    constant_t COL_BLACK("COL_BLACK", 0);
    constant_t COL_BTN_SOLO_BG_ENABLED("COL_BTN_SOLO_BG_ENABLED", 0xFF333ab6);
    constant_t COL_BTN_SOLO_BG_PARENT("COL_BTN_SOLO_BG_PARENT", 0xFF555ab6);
    constant_t COL_BTN_SOLO_BG_DISABLED("COL_BTN_SOLO_BG_DISABLED", 0xFF696a74);
#define TO_U32(r, g, b, a) ((uint32_t)((r & 0xFF) | ((g & 0xFF) << 8) | ((b & 0xFF) << 16) | ((a & 0xFF) << 24)))
    constant_t COL_LEVEL_IND_GREEN("COL_LEVEL_IND_GREEN", TO_U32(30, 255, 30, 255));
    constant_t COL_LEVEL_IND_GREEN_DRK("COL_LEVEL_IND_GREEN_DRK", TO_U32(10, 160, 10, 255));
    constant_t COL_LEVEL_IND_GREEN_DRKER("COL_LEVEL_IND_GREEN_DRKER", TO_U32(5, 120, 5, 255));
    constant_t COL_LEVEL_IND_YELLOW("COL_LEVEL_IND_YELLOW", TO_U32(255, 255, 30, 255));
    constant_t COL_LEVEL_IND_YELLOW_DRK("COL_LEVEL_IND_YELLOW_DRK", TO_U32(160, 160, 10, 255));
    constant_t COL_LEVEL_IND_YELLOW_DRKER("COL_LEVEL_IND_YELLOW_DRKER", TO_U32(120, 120, 5, 255));
    constant_t COL_ON("COL_ON", TO_U32(210, 240, 210, 255));
    constant_t COL_OFF("COL_OFF", TO_U32(240, 210, 210, 255));
    constant_t COL_TEXTBOX_TEXT("COL_TEXTBOX_TEXT", 0xFFE9EAFD);
    constant_t COL_TEXTBOX_TEXT_DISABLED("COL_TEXTBOX_TEXT_DISABLED", 0xFF666666);
    constant_t COL_TEXTBOX_TEXT_MARKED("COL_TEXTBOX_TEXT_MARKED", 0xFFFFFFFF);

    constant_t COL_PIANOROLL_WHITE("COL_PIANOROLL_WHITE", 0xFFFFFFFF);
    constant_t COL_PIANOROLL_BLACK("COL_PIANOROLL_BLACK", 0xFF111111);
    constant_t COL_PIANOROLL_STROKE("COL_PIANOROLL_STROKE", 0xFF444444);
    constant_t COL_CLIPEDITOR_SHARP("COL_CLIPEDITOR_SHARP", 0x33111111);
    constant_t COL_NOTE_REALTIME("COL_NOTE_REALTIME", 0xFFFF00FF);
    constant_t COL_NOTE_MOUSE("COL_NOTE_MOUSE", 0xFF00FFFF);

    constant_t COL_FOLD_BUTTON("COL_FOLD_BUTTON", 0xFFFF9933);


    constant_t COL_CLIP_OUTLINE("COL_CLIP_OUTLINE", 0x0);
    constant_t COL_CLIP_NOTE("COL_CLIP_NOTE", 0xFFFFFFFF);
    constant_t COL_CLIP_NOTE_OVERLAP("COL_CLIP_NOTE_OVERLAP", 0xFF0000FF);
    constant_t COL_CLIP_NOTE_MUTED("COL_CLIP_NOTE_MUTED", 0xFF121212);

    constant_t COL_BTN_BG_DEFAULT_INACTIVE("COL_BTN_BG_DEFAULT_INACTIVE", 0xff202020);
    constant_t COL_BTN_BG_DEFAULT_ACTIVE("COL_BTN_BG_DEFAULT_ACTIVE", 0xff404040);
    constant_t COL_BTN_BG_BYPASS_ACTIVE("COL_BTN_BG_BYPASS_ACTIVE", 0xff80ABC0);
    constant_t COL_BTN_BG_SHOW_ACTIVE("COL_BTN_BG_SHOW_ACTIVE", 0xff40ABC0);

    constant_t COL_BTN_LOAD_DEF_PLUGINS("COL_BTN_LOAD_DEF_PLUGINS", 0xFFFFFFFF);

    constant_t COL_BTN_RECORD_ARM_BG("COL_BTN_RECORD_ARM_BG", 0xFF442222);

    constant_t COL_PLUGIN_VIEW_FRAME("COL_PLUGIN_VIEW_FRAME", 0x7fffffff);

    constant_t COL_KNOB("COL_KNOB", 0xff00ddff);
    constant_t COL_KNOB_IND("COL_KNOB_IND", 0xffffffff);
    constant_t COL_AUTOMATED("COL_AUTOMATED", 0xFFEF62DF);

    constant_t COL_PLUG_TITLE("COL_PLUG_TITLE", 0xff151515);
    constant_t COL_PLUG_TITLE_SELECTED("COL_PLUG_TITLE_SELECTED", 0xff353535);
    constant_t COL_PLUG_TITLE_FOCUSED("COL_PLUG_TITLE_FOCUSED", 0xffff0000);
    constant_t COL_LABEL_CONTAINER("COL_LABEL_CONTAINER", 0xffd0d0d0);
} // namespace GuiColor

NVGcolor rgbaToNvg(uint32_t i);
uint32_t nvgToRGBA(NVGcolor c);
NVGcolor mulSatBright(NVGcolor rgb, float sat, float brt);
namespace GuiColor {

    void initConstants(int colorVal) {
        int c            = colorVal;
        int c2           = math::max(5, c - 16);
        int c3           = math::min(255, c + 16);
        auto setConstant = [](const GuiColor::constant_t& constantRef, int32_t rgba) { changeConstantDefault(constantRef, rgba); };
        setConstant(GuiColor::COL_GRID_DRK, GUI_COLOR_HEXA(c, 255));
        setConstant(GuiColor::COL_GRID_BRT, GUI_COLOR_HEXA(c + 3, 255));
        setConstant(GuiColor::COL_LINE_BAR, GUI_COLOR_HEXA(c2, 255));
        setConstant(GuiColor::COL_LINE_QRT, GUI_COLOR_HEXA(c2 + 3, 255));
        setConstant(GuiColor::COL_LINE_XTH, GUI_COLOR_HEXA(c2 + 6, 255));
        setConstant(GuiColor::COL_LINE_SEPERATOR, GUI_COLOR_HEXA(c2 - 3, 255));
        setConstant(GuiColor::COL_BG_DRKER, GUI_COLOR_HEXA(math::max(0, c3 - 20), 255));
        setConstant(GuiColor::COL_BG_DRKER2, GUI_COLOR_HEXA(math::max(0, c3 - 40), 255));
        setConstant(GuiColor::COL_BG_DRK, GUI_COLOR_HEXA(c3, 255));
        setConstant(GuiColor::COL_BG_BRT, GUI_COLOR_HEXA(c3 + 20, 255));
        int c4 = math::max(5, c - 32);
        int c5 = math::max(5, c + 32);
        setConstant(GuiColor::COL_CTXTMNU_OUTLINE, GUI_COLOR_HEXA(255, 255));
        setConstant(GuiColor::COL_CTXTMNU_BG, GUI_COLOR_HEXA(c4, 255));
        setConstant(GuiColor::COL_CTXTMNU_HILIGHT, GUI_COLOR_HEXA(c5, 255));
        auto gridDark = rgbaToNvg(GuiColor::COL_GRID_DRK.defValue);
        setConstant(GuiColor::COL_GUI_STROKE, nvgToRGBA(mulSatBright(gridDark, 1.3f, 1.4f)));
        setConstant(GuiColor::COL_BG_DRK_FOCUSED, GUI_COLOR_HEXA(c3 + 48, 255));
        setConstant(GuiColor::COL_CLEAR_COLOR, (0xff000000));

        setConstant(GuiColor::COL_NOTE, (0xffff9933));
        setConstant(GuiColor::COL_NOTE_PLAYING, (0xff33ff33));
        setConstant(GuiColor::COL_NOTE_ARP, (0xff22bb22));
        setConstant(GuiColor::COL_NOTE_MUTE, (0xff666666));
        setConstant(GuiColor::COL_NOTE_OUTLINE, (0xff000000));
        setConstant(GuiColor::COL_NOTE_TEXT, (0xff333333));
        setConstant(GuiColor::COL_BG_SELECTEDTRACK, GUI_COLOR_HEXA(c3 + 20, 80));
        setConstant(GuiColor::COL_LABEL_ACTIVE, GUI_COLOR_HEXA(255, 255));
        setConstant(GuiColor::COL_LABEL_INACTIVE, GUI_COLOR_HEXA(128, 255));
    }
} // namespace GuiColor

namespace UIFont {
    static std::vector<font_type_t*>& _getConstants() noexcept {
        static std::vector<font_type_t*> allconstants;
        return allconstants;
    }
    font_type_t getConstantById(int32_t id) {
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
    int32_t getNextId() {
        static int32_t constantsNextId = 1;
        return constantsNextId++;
    }

    font_type_t::font_type_t() noexcept : idx(0), name(nullptr), defValue("") {
    }

    font_type_t::font_type_t(const char* _name, const char* _defValue) noexcept : idx(getNextId()), name(_name), defValue(_defValue) {
        auto& allconstants = _getConstants();
        allconstants.push_back(this);
    }

    const font_type_t FONT_DEFAULT      = font_type_t("FONT_DEFAULT", "Roboto-Medium.ttf");
    const font_type_t FONT_LABEL        = font_type_t("FONT_LABEL", "Roboto-Medium.ttf");
    const font_type_t FONT_TEXFIELD     = font_type_t("FONT_TEXFIELD", "Roboto-Medium.ttf");
    const font_type_t FONT_CONTEXT_MENU = font_type_t("FONT_CONTEXT_MENU", "Roboto-Medium.ttf");
    const font_type_t FONT_DECIMAL      = font_type_t("FONT_DECIMAL", "Roboto-Medium.ttf");
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
        const int fontIdx = math::clamp<int32_t>(font.fontInstanceIdx, 0, fonts.fontsLoaded.size());
        auto& fontloaded  = fonts.fontsLoaded[fontIdx];
        if (fontloaded.nvgId == -999) {
            log_printf("loading font %s %s\n", StringAsCStr(fontloaded.font.name), StringAsCStr(fontloaded.font.path));
            fontloaded.nvgId = nvgCreateFont(ctx, StringAsCStr(fontloaded.font.name), StringAsCStr(fontloaded.font.path));
        }
        nvgFontFaceId(ctx, fontloaded.nvgId);
    }

} // namespace UIFont
