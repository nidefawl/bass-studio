#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>

#include "str_util.h"
#include "color_util.h"
#include "gui/gui.h"
#include "gui/guicontainer.h"
#include "gui/pluginviewcontainers.h"
#include "gui/knob.h"

#include "leak_detect.h"

class guiknob_labeled_base : public guiknob {
public:
	const int button_inset = 10;
    std::function<String(float)> fnGetDisplayValue;
protected:
	int labelHeight = 0;
	int valueHeight = 0;
	String valueDisplay = "  ";
public:
	guiknob_labeled_base(const bool _renderBackground = true, const bool _isSlider = false) : guiknob(_renderBackground, _isSlider) {

	}
	virtual ~guiknob_labeled_base() {
	}
	virtual void setDisplayValue(float f) override {
		if (fnGetDisplayValue)
		{
			valueDisplay = fnGetDisplayValue(f);
		}
	}
	void layout() override;
	virtual void render(NVGcontext* vg) override;
};

