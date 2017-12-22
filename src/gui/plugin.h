#pragma once
#include <glm/vec2.hpp>
#include "gui.h"
#include "guicolors.h"
#include "event.h"
#include "str_util.h"
#include "color_util.h"
#include "button.h"
#include "renderresources.h"
#include "list.h"
#include "knob.h"

using glm::vec2;
using glm::ivec2;

class vstplugin;
class guibuttontoggle : public guibuttonbase {
public:
	float radius = 0;
	bool* state = NULL;
	int icon = -1;
	guibuttontoggle() : guibuttonbase() {
	}
	guibuttontoggle(ivec2 _pos, float _radius) : guibuttonbase(_pos, ivec2((int)(_radius * 2))) {
		this->radius = _radius;
	}
	bool enabled() {
		return *state;
	}
	void render(NVGcontext* vg) {
		NVGcolor c;
		if (!enabled()) {
			c = G_BUTTON_DISABLED;
		}
		else if (pressed()) {
			c = colorPressed;
		}
		else if (hovered()) {
			c = colorHover;
		}
		else {
			c = color;
		}
		vec2 cen = vec2(radius);
		cen.x += pos.x;
		cen.y += pos.y;
		NVGcolor c2 = colorStroke;
		if (hovered()) {
			c2 = G_WHITE;
		}
		nvgBeginPath(vg);
		nvgCircle(vg, cen.x, cen.y, radius);
		nvgFillColor(vg, c);
		nvgFill(vg);
		nvgStrokeColor(vg, c2);
		nvgStrokeWidth(vg, G_STROKE);
		nvgStroke(vg);
		if (icon >= 0) {


			int32_t extImg = 2;
			int32_t iconW = (int32_t)ceil(radius*2)+extImg*2;
			RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
			NVGpaint paintIcon = nvgImagePattern(vg, -extImg, -extImg, iconW, iconW, 0, image.id, 1.0f);
			nvgTranslate(vg, pos.x, pos.y);
			nvgBeginPath(vg);
			nvgRect(vg, -extImg, -extImg, iconW, iconW);
			nvgFillPaint(vg, paintIcon);
			nvgFill(vg);
			nvgTranslate(vg, -pos.x, -pos.y);
		}

		/*nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgFillColor(vg, c);
		nvgFill(vg);*/
	}
};

class vstplugin;

class guiplugin : public guibase {
public:
	vstplugin* const vst;
	gui_list params;
	char text[MAX_STR_TITLE];
	guibuttontoggle buttonBypass;
	guibuttontoggle buttonOpenEditor;
	guibuttontoggle buttonDelete;
	guiplugin(vstplugin* _vst);
	~guiplugin() {
		my_printf("DSTR!\n",0);
	}
	virtual void renderDragged(NVGcontext* vg, ivec2 mousepos) {
		mousepos -= pos;
		nvgTranslate(vg, mousepos.x, mousepos.y);
		render(vg);
	}
	void render(NVGcontext* vg);
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	void setTitle(String wxtext) {
		const char* wxmb = StringAsCStr(wxtext);
		strncpy_s(this->text, MAX_STR_TITLE, wxmb, strlen(wxmb));
	}
	void layout() {
		int32_t inset1 = (HEIGHT_PLUGIN_TITLE - buttonBypass.size.y) / 2;
		buttonBypass.pos.y = inset1;
		buttonBypass.pos.x = inset1;
		buttonOpenEditor.pos.y = inset1;
		buttonOpenEditor.pos.x = buttonBypass.right();
		buttonDelete.pos.y = inset1;
		buttonDelete.pos.x = size.x - buttonDelete.size.x - inset1;
		int32_t insetCtrls = INSET_TITLE;
		params.pos = ivec2(insetCtrls, insetCtrls + HEIGHT_PLUGIN_TITLE);
		params.size = size - params.pos - ivec2(insetCtrls);
		params.layout();
	}
	void handleDraggedMove(MouseEvent& evt) override;
	void handleDraggedRelease(MouseEvent& evt) override;
	void dragMoveOn(guibase* target, ivec2 mousepos) override;
	void dragReleaseOn(guibase* target, ivec2 mousepos) override;
	void setState(bool state) {
	}
	void buttonClicked(guibase* _button);
	bool isDragMoveable() {
		return true;
	}
};
