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
                log_printf("failed changing default for constant %d\n", p->idx);
            }
        }
    }

    int32_t getNextId() {
        static int32_t constantsNextId = 1;
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
    constant_t CONST_FONT_SIZE_TABLE("CONST_FONT_SIZE_TABLE", 200, 1, 2000);
    constant_t CONST_FONT_SIZE_CONTEXT_MENU("CONST_FONT_SIZE_CONTEXT_MENU", 200, 1, 2000);
    constant_t CONST_FONT_SIZE_CTR_LABEL("CONST_FONT_SIZE_CTR_LABEL", 14);

    constant_t CONST_LAYOUT_MARGIN("CONST_LAYOUT_MARGIN", 1, 0, 32);
    constant_t CONST_ROUND("CONST_ROUND", 0, 0, 0);
    constant_t CONST_GUI_INSET_WIDGET_BG("CONST_GUI_INSET_WIDGET_BG", 1, 0, 5);
    constant_t CONST_GUI_FRAME_STROKE_WIDTH("CONST_GUI_FRAME_STROKE_WIDTH", 10, 1, 50);

    constant_t CONST_ROW_HEIGHT("CONST_ROW_HEIGHT", 24, 8, 64);
    constant_t CONST_SMALL_LABEL_HEIGHT("CONST_SMALL_LABEL_HEIGHT", 20, 4, 256);
    constant_t CONST_FIXED_TITLE_HEIGHT("CONST_FIXED_TITLE_HEIGHT", 36, 4, 256);
    constant_t CONST_PLUGIN_TITLE_HEIGHT("CONST_PLUGIN_TITLE_HEIGHT", 34, 4, 256);

    constant_t CONST_TRACK_HEIGHT_STEP("CONST_TRACK_HEIGHT_STEP", 20 + INSET_TRACK_CONTENT * 2, 4, 256);


    constant_t CONST_TRACK_IO_WIDTH("CONST_TRACK_IO_WIDTH", 150, 20, 1000);
    constant_t CONST_TRACK_CONTROLS_WIDTH("CONST_TRACK_CONTROLS_WIDTH", 550, 20, 1000);
    constant_t CONST_METER_WIDTH("CONST_METER_WIDTH", 100, 20, 1000);
    constant_t CONST_MIXER_WIDTH("CONST_MIXER_WIDTH", 250, 40, 1000);

    constant_t CONST_NODES_SCALE("CONST_NODES_SCALE", 40, 1, 1000);


    constant_t CONST_PIANOROLL_STROKE_WIDTH("CONST_PIANOROLL_STROKE_WIDTH", 10);
    constant_t CONST_CLIPEDITOR_HANDLES_STROKE_WIDTH("CONST_CLIPEDITOR_HANDLES_STROKE_WIDTH", 10);


    constant_t CONST_NOTE_RENDER_MODE("CONST_NOTE_RENDER_MODE", 1, 0, 1);
}// namespace GuiConstant
