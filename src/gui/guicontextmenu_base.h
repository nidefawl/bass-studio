#pragma once
#include <vector>
#include "math/vec.h"
#include "event.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "basectrl.h"

namespace RenderResources {
struct NvgImageTexture;
}
class ctxtmenu_entry {
public:
	int id = 0;
	String title;
	int width = -1;
	int height = 0;
	int y = 0;
	int fontSize = 0;
	guitheme_t* theme = nullptr;
	int fixedLeftOffset = -1;
	RenderResources::NvgImageTexture* icon = nullptr;
	ctxtmenu_entry(String _title, int _id) : id(_id), title(_title) {
	}
	virtual ~ctxtmenu_entry() {

	}
	void setIcon(RenderResources::NvgImageTexture* _icon) {
		this->icon = _icon;
	}
	virtual void layout(ivec2 size, int32_t _fontSize) {
		this->fontSize = _fontSize;
		this->height = (int32_t) round(_fontSize*1.1f);
		this->width = math::max(size.x, this->width);
	}
	virtual int leftOffset() {
		if (fixedLeftOffset >= 0) {
			return fixedLeftOffset;
		}
		int32_t offset = (int32_t) round(this->fontSize/2.4f);
		if (icon != nullptr) {
			offset += height;
		}
		return offset;
	}
	virtual void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
		if (contains(ctxtSize, mouse)) {
			nvgBeginPath(vg);
			nvgRect(vg, 0, y, ctxtSize.x, height);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
			nvgFill(vg);
		}
		UTIL_setFont(vg, theme, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
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
	void setFontSize(int i) {
		this->fontSize = i;
	}
	virtual ~guictxtmenu_base() {
		destroyGuis();
	}
	virtual bool isTransient() {
		return false;
	}
	virtual bool isDialog() {
		return false;
	}
//	virtual bool canClose() {
//		return false;
//	}
	virtual void onParentWindowClose() {

	}

	void render(NVGcontext* vg) override {
		guictr_base::render(vg);
	}
	void determineSize(ivec2& prefSize) override {
		for (guibase* gui : guis) {
			auto prefSizeCpy = prefSize;
			gui->determineSize(prefSizeCpy);
			gui->size = prefSizeCpy;
		}
		ivec2 maxSize = ivec2(0);
		for (guibase* gui : guis) {
			maxSize.x = math::max(maxSize.x, gui->right());
			maxSize.y = math::max(maxSize.y, gui->bottom());
		}
		prefSize = maxSize;
	}
	virtual void onChildLayoutChanged(guibase* g) override {
//		determineSize();
		if (this->parent != NULL) {
			this->parent->onChildLayoutChanged(this);
		}
	}
	void layout() override {
		for (auto* g : guis) {
			g->pos = {0, 0};
			g->size = size;
			g->layout();
		}
	}
	void closeContextMenu() {
		// may be null if we got closed
		if (parentCtrl)
			parentCtrl->closePopup();
	}
};
