#include "guicolors.h"
#include <nanovg_min.h>
#include <vector>
#include <algorithm>
#include "math/seq_math.h"
#include "logging.h"
#include "str_util.h"


uint32_t colorPalette[COLOR_PALETTE_LEN] = {

		 0xe6b0aa, 0xcd6155, 0xa93226, 0x7b241c,
		 0xf5b7b1, 0xec7063, 0xcb4335, 0x943126,
		 0xd7bde2, 0xaf7ac5, 0x884ea0, 0x633974,
		 0xd2b4de, 0xa569bd, 0x7d3c98, 0x5b2c6f,
		 0xa9cce3, 0x5499c7, 0x2471a3, 0x1a5276,

		 0xaed6f1, 0x5dade2, 0x2e86c1, 0x21618c,
		 0xa3e4d7, 0x48c9b0, 0x17a589, 0x117864,
		 0xa2d9ce, 0x45b39d, 0x138d75, 0x0e6655,
		 0xa9dfbf, 0x52be80, 0x229954, 0x196f3d,
		 0xabebc6, 0x58d68d, 0x28b463, 0x1d8348,

		 0xf9e79f, 0xf4d03f, 0xd4ac0d, 0x9a7d0a,
		 0xfad7a0, 0xf5b041, 0xd68910, 0x9c640c,
		 0xf5cba7, 0xeb984e, 0xca6f1e, 0x935116,
		 0xedbb99, 0xdc7633, 0xba4a00, 0x873600,
		 0xF0F0F0, 0xA0A0A0, 0x606060, 0x050505,
};
namespace GuiColor {
static std::vector<constant_t*>& _getConstants() {
	static std::vector<constant_t*> allconstants;
	return allconstants;
}
constant_t getConstantById(int32_t id) {
	auto& v =_getConstants();
	for (auto* c : v) {
		if (c->idx == id) {
			return *c;
		}
	}
	return constant_t();
}
constant_t getConstantByName(String name) {
	auto& v =_getConstants();
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

constant_t::constant_t()
: idx(0),
  name(nullptr),
  defValue(0) {
//  allconstants.push_back(*this);
}
constant_t::constant_t(const char* _name, int32_t _defValue)
: idx(getNextId()),
  name(_name),
  defValue(_defValue) {
	auto& allconstants = _getConstants();
	my_printf("push %16s to %12X -> size %d\n", _name, (int64_t)&allconstants, allconstants.size());
  allconstants.push_back(this);
}
constant_t COL_GRID_DRK = constant_t("COL_GRID_DRK", 0xFF000000);
constant_t COL_GRID_BRT = constant_t("COL_GRID_BRT", 0xFF000000);
constant_t COL_LINE_BAR = constant_t("COL_LINE_BAR", 0xFF000000);
constant_t COL_LINE_QRT = constant_t("COL_LINE_QRT", 0xFF000000);
constant_t COL_LINE_XTH = constant_t("COL_LINE_XTH", 0xFF000000);
constant_t COL_BG_DRK = constant_t("COL_BG_DRK", 0xFF000000);
constant_t COL_BG_BRT = constant_t("COL_BG_BRT", 0xFF000000);
constant_t COL_LINE_SEPERATOR = constant_t("COL_LINE_SEPERATOR", 0xFF000000);
constant_t COL_CTXTMNU_OUTLINE = constant_t("COL_CTXTMNU_OUTLINE", 0xFF000000);
constant_t COL_CTXTMNU_BG = constant_t("COL_CTXTMNU_BG", 0xFF000000);
constant_t COL_CTXTMNU_HILIGHT = constant_t("COL_CTXTMNU_HILIGHT", 0xFF000000);
constant_t COL_GUI_STROKE = constant_t("COL_GUI_STROKE", 0xFF000000);
constant_t COL_BG_DRK_FOCUSED = constant_t("COL_BG_DRK_FOCUSED", 0xFF000000);
constant_t COL_NOTE = constant_t("COL_NOTE", 0xFF000000);
constant_t COL_NOTE_PLAYING = constant_t("COL_NOTE_PLAYING", 0xFF000000);
constant_t COL_NOTE_ARP = constant_t("COL_NOTE_ARP", 0xFF000000);
constant_t COL_NOTE_MUTE = constant_t("COL_NOTE_MUTE", 0xFF000000);
constant_t COL_NOTE_OUTLINE = constant_t("COL_NOTE_OUTLINE", 0xFF000000);
constant_t COL_NOTE_TEXT = constant_t("COL_NOTE_TEXT", 0xFF000000);
constant_t COL_BG_SELECTEDTRACK = constant_t("COL_BG_SELECTEDTRACK", 0xFF000000);
constant_t COL_BG_SELECTEDTRACK_TITLE = constant_t("COL_BG_SELECTEDTRACK_TITLE", 0xFF000000);
constant_t COL_BG_DRKER = constant_t("COL_BG_DRKER", 0xFF000000);
constant_t COL_BG_DRKER2 = constant_t("COL_BG_DRKER2", 0xFF000000);
constant_t COL_CLEAR_COLOR = constant_t("COL_CLEAR_COLOR", 0xFF000000);
constant_t COL_LABEL_ACTIVE = constant_t("COL_LABEL_ACTIVE", 0xFF000000);
constant_t COL_LABEL_INACTIVE = constant_t("COL_LABEL_INACTIVE", 0xFF000000);
constant_t COL_WHITE("COL_WHITE", -1);
constant_t COL_BLACK("COL_BLACK", 0);
#define TO_INT32(r,g,b,a) ((int32_t)((r&0xFF)|((g&0xFF)<<8)|((b&0xFF)<<16)|((a&0xFF)<<24)))
constant_t COL_LEVEL_IND_GREEN("COL_LEVEL_IND_GREEN", TO_INT32(30, 255, 30, 255));
constant_t COL_LEVEL_IND_GREEN_DRK("COL_LEVEL_IND_GREEN_DRK",TO_INT32(10, 160, 10, 255));
constant_t COL_LEVEL_IND_GREEN_DRKER("COL_LEVEL_IND_GREEN_DRKER", TO_INT32(5, 120, 5, 255));
constant_t COL_LEVEL_IND_YELLOW("COL_LEVEL_IND_YELLOW", TO_INT32(255, 255, 30, 255));
constant_t COL_LEVEL_IND_YELLOW_DRK("COL_LEVEL_IND_YELLOW_DRK", TO_INT32(160, 160, 10, 255));
constant_t COL_LEVEL_IND_YELLOW_DRKER("COL_LEVEL_IND_YELLOW_DRKER", TO_INT32(120, 120, 5, 255));

}

NVGcolor rgbaToNvg(uint32_t i);
uint32_t nvgToRGBA(NVGcolor c);
NVGcolor mulSatBright(NVGcolor rgb, float sat, float brt);
namespace GuiColor {

void initConstants(int colorVal) {
	int c = colorVal;
	int c2 = math::max(5, c - 16);
	int c3 = math::min(255, c + 16);
	auto setConstant = [](const GuiColor::constant_t& constantRef, int32_t rgba) {
		changeConstantDefault(constantRef, rgba);
	};
	setConstant(GuiColor::COL_GRID_DRK, GUI_COLOR_HEXA(c, 255));
	setConstant(GuiColor::COL_GRID_BRT, GUI_COLOR_HEXA(c + 3, 255));
	setConstant(GuiColor::COL_LINE_BAR, GUI_COLOR_HEXA(c2, 255));
	setConstant(GuiColor::COL_LINE_QRT, GUI_COLOR_HEXA(c2 + 3, 255));
	setConstant(GuiColor::COL_LINE_XTH, GUI_COLOR_HEXA(c2 + 6, 255));
	setConstant(GuiColor::COL_LINE_SEPERATOR, GUI_COLOR_HEXA(c2 - 3, 255));
	setConstant(GuiColor::COL_BG_DRKER, GUI_COLOR_HEXA(math::max(0, c3-20), 255));
	setConstant(GuiColor::COL_BG_DRKER2, GUI_COLOR_HEXA(math::max(0, c3-40), 255));
	setConstant(GuiColor::COL_BG_DRK, GUI_COLOR_HEXA(c3, 255));
	setConstant(GuiColor::COL_BG_BRT, GUI_COLOR_HEXA(c3 + 20, 255));
	int c4 = math::max(5, c - 32);
	int c5 = math::max(5, c + 32);
	setConstant(GuiColor::COL_CTXTMNU_OUTLINE, GUI_COLOR_HEXA(255, 255));
	setConstant(GuiColor::COL_CTXTMNU_BG, GUI_COLOR_HEXA(c4, 255));
	setConstant(GuiColor::COL_CTXTMNU_HILIGHT, GUI_COLOR_HEXA(c5, 255));
	auto gridDark = rgbaToNvg(GuiColor::COL_GRID_DRK.defValue);
	setConstant(GuiColor::COL_GUI_STROKE, nvgToRGBA(mulSatBright(gridDark, 1.3f, 1.4f)));
	setConstant(GuiColor::COL_BG_DRK_FOCUSED, GUI_COLOR_HEXA(c3+48, 255));
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
}
