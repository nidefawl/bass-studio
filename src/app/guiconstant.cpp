#include "guiconstant.h"
#include <nanovg_min.h>
#include <vector>
#include "guiglobals.h"
#include "logging.h"

namespace GuiConstant {

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

    constant_t::constant_t() noexcept : idx(0), name(nullptr), defValue(0) {
    }

    constant_t::constant_t(const char* _name, int32_t _defValue) noexcept
        : idx(getNextId()), name(_name), defValue(_defValue) {
        auto& allconstants = _getConstants();
        allconstants.push_back(this);
    }

    constant_t::constant_t(const char* _name, int32_t _defValue, int _rangeMin, int _rangeMax) noexcept
        : idx(getNextId()), name(_name), defValue(_defValue), rangeMin(_rangeMin), rangeMax(_rangeMax) {
        auto& allconstants = _getConstants();
        allconstants.push_back(this);
    }

    constant_t& constant_t::setMinMax(int iMin, int iMax) noexcept {
        rangeMin = iMin;
        rangeMax = iMax;
        return *this;
    }

}// namespace GuiConstant

namespace GuiConstant {
    constant_t CONST_FONT_SCALE("CONST_FONT_SCALE", 7, 1, 64);
    constant_t CONST_FONT_SIZE_TABLE("CONST_FONT_SIZE_TABLE", 260, 1, 2000);
    constant_t CONST_FONT_SIZE_CONTEXT_MENU("CONST_FONT_SIZE_CONTEXT_MENU", 200, 1, 2000);
    constant_t CONST_FONT_SIZE_CTR_LABEL("CONST_FONT_SIZE_CTR_LABEL", 14);

    constant_t CONST_PADDING_TRACK_CONTROLS("CONST_PADDING_TRACK_CONTROLS", 1, 0, 32);
    constant_t CONST_PADDING_EDITOR_CONTROLS("CONST_PADDING_EDITOR_CONTROLS", 4, 0, 32);
    constant_t CONST_GUI_INSET_WIDGET_BG("CONST_GUI_INSET_WIDGET_BG", 1, 0, 5);
    constant_t CONST_GUI_FRAME_STROKE_WIDTH("CONST_GUI_FRAME_STROKE_WIDTH", 22, 1, 50);
    constant_t CONST_BORDER_WIDTH("CONST_BORDER_WIDTH", 2, 0, 24);

    constant_t CONST_ROW_HEIGHT("CONST_ROW_HEIGHT", 24, 8, 64);
    constant_t CONST_SMALL_LABEL_HEIGHT("CONST_SMALL_LABEL_HEIGHT", 20, 4, 256);
    constant_t CONST_FIXED_TITLE_HEIGHT("CONST_FIXED_TITLE_HEIGHT", 30, 4, 256);
    constant_t CONST_PLUGIN_TITLE_HEIGHT("CONST_PLUGIN_TITLE_HEIGHT", 40, 4, 256);

    constant_t CONST_TRACK_HEIGHT_STEP("CONST_TRACK_HEIGHT_STEP", 23, 4, 256);


    constant_t CONST_TRACK_IO_WIDTH("CONST_TRACK_IO_WIDTH", 150, 20, 1000);
    constant_t CONST_TRACK_CONTROLS_WIDTH("CONST_TRACK_CONTROLS_WIDTH", 550, 20, 1000);
    constant_t CONST_METER_WIDTH("CONST_METER_WIDTH", 40, 20, 1000);
    constant_t CONST_MIXER_WIDTH("CONST_MIXER_WIDTH", 250, 40, 1000);

    constant_t CONST_NODES_SCALE("CONST_NODES_SCALE", 40, 1, 1000);


    constant_t CONST_PIANOROLL_STROKE_WIDTH("CONST_PIANOROLL_STROKE_WIDTH", 10);
    constant_t CONST_CLIPEDITOR_HANDLES_STROKE_WIDTH("CONST_CLIPEDITOR_HANDLES_STROKE_WIDTH", 10);


    constant_t CONST_NOTE_RENDER_MODE("CONST_NOTE_RENDER_MODE", 1, 0, 1);
}// namespace GuiConstant
