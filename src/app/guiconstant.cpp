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
	constant_t::constant_t(const char* _name, int32_t _defValue, int _rangeMin, int _rangeMax)
	: idx(getNextId()),
	  name(_name),
	  defValue(_defValue),
	  rangeMin(_rangeMin),
	  rangeMax(_rangeMax) {
		auto& allconstants = _getConstants();
		my_printf("push %16s to %12X -> size %d\n", _name, (int64_t)&allconstants, allconstants.size());
	  allconstants.push_back(this);
	}
	constant_t& constant_t::setMinMax(int iMin, int iMax) {
		rangeMin = iMin;
		rangeMax = iMax;
		return *this;
	}
}

namespace GuiConstant {
constant_t CONST_FIXED_TITLE_HEIGHT("CONST_FIXED_TITLE_HEIGHT", 24);
constant_t CONST_PLUGIN_TITLE_HEIGHT("CONST_PLUGIN_TITLE_HEIGHT", 24);
constant_t CONST_TRACK_HEIGHT_STEP("CONST_TRACK_HEIGHT_STEP", 24+INSET_TRACK_CONTENT*2);
constant_t CONST_TRACK_HEIGHT_TITLE("CONST_TRACK_HEIGHT_TITLE", 24+INSET_TRACK_CONTENT*2);
constant_t CONST_METER_WIDTH("CONST_METER_WIDTH", 32);
constant_t CONST_FONT_SCALE("CONST_FONT_SCALE", 10);
}
