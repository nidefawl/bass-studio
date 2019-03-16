#pragma once
#include <vector>
#include "event.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "basectrl.h"

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;

class ctxtmenu_entry {
public:
	String title;
	int width = -1;
	int height = 0;
	int id = 0;
	int y = 0;
	int fontSize = 0;
	guitheme_t* theme = nullptr;
	ctxtmenu_entry(String _title, int _id) {
		this->id = _id;
		this->title = _title;
	}
	virtual ~ctxtmenu_entry() {

	}
	virtual void layout(ivec2 size, int32_t _fontSize) {
		this->fontSize = _fontSize;
		this->height = (int32_t) round(_fontSize*1.1f);
		this->width = std::max(size.x, this->width);
	}
	int leftOffset() {
		return (int32_t) round(this->fontSize/2.4f);
	}
	virtual void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
		if (contains(ctxtSize, mouse)) {
			nvgBeginPath(vg);
			nvgRect(vg, 0, y, ctxtSize.x, height);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
			nvgFill(vg);
		}
		setFont(vg, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(title), NULL);
	}
	bool contains(ivec2& ctxtSize, ivec2& mouse) {
		return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
	}
	virtual int getClicked(ivec2& ctxtSize, ivec2& mouse) {
		if (contains(ctxtSize, mouse)) {
			return id;
		}
		return -1;
	}
};
class ctxtmenu_splitter : public ctxtmenu_entry {
public:
	ctxtmenu_splitter()
		: ctxtmenu_entry("-", -1)
	{
	}
	void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, y+height/2);
		nvgLineTo(vg, ctxtSize.x, y+height/2);
		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_OUTLINE));
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);
	}
	void layout(ivec2 size, int32_t _fontSize) override {
		this->fontSize = _fontSize;
		this->height = ((int32_t) round(_fontSize*1.1f)) / 2;
	}
	bool contains(ivec2& ctxtSize, ivec2& mouse) {
		return false;
	}
};
class guictxtmenu_base : public guictr_base {
protected:
	int paddingV = 2;
	int fontSize = FONT_SIZE_CTXT;
public:
	bool scrollbarOutside = false;
	bool canTakeInputFocus = false;
	int maxHeight = 360;
//	int curTooltip = 0;
	guictxtmenu_base() : guictr_base() {
		margin = 0;
		padding = 0;
	}
	virtual ~guictxtmenu_base() {
	}
	virtual bool isTransient() {
		return false;
	}
	virtual bool canClose() {
		return false;
	}

	void render(NVGcontext* vg) {
		guictr_base::render(vg);
	}
	virtual void onChildLayoutChanged(guibase* g) {
		ivec2 maxSize = ivec2(0);
		for (guibase* gui : guis) {
			maxSize.x = max(maxSize.x, gui->right());
			maxSize.y = max(maxSize.y, gui->bottom());
		}
		size = maxSize;
		if (this->parent != NULL) {
			this->parent->onChildLayoutChanged(this);
		}
	}
};
