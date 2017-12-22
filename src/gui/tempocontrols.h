#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "str_util.h"
#include "color_util.h"

#include "gui.h"
#include "guicontainer.h"
#include "button.h"
#include "knob.h"
#include "../host/vst_host.h"
#include "settings.h"

void testTask();
class gui_tempocontrol : public guibuttonbase {
public:
	gui_tempocontrol()
		:  guibuttonbase() {
		setColor(nvgToRGB(g_guiColors[COL_BG_DRK]));
	}
	bool enabled() {
		return true;
	}
	void render(NVGcontext* vg) {
		NVGcolor c;
		if (!enabled()) {
			c = G_BUTTON_DISABLED;
		}
		else if (pressed()|| focused()) {
			c = colorPressed;
		} else if (hovered()) {
			c = colorHover;
		}
		else {
			c = g_guiColors[COL_BG_DRK];
		}
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgStrokeColor(vg, g_guiColors[COL_GUI_STROKE]);
		nvgStrokeWidth(vg, 5);
		nvgStroke(vg);
		nvgFillColor(vg, c);
		nvgFill(vg);
		String tempo = FormatTempo(MainCtrl::get()->getCurrentTempoBPM());
		setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(tempo), NULL);
	}
	void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			MainCtrl::get()->captureMouse(this);
		}
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
			int disty = (int)evt.dragDistance->y / 10;
			if (abs(disty) < 1)
				return;
			evt.dragDistance->y = 0;
			int tempo = MainCtrl::get()->getCurrentTempo();
			MainCtrl::get()->setTempo(tempo - disty*100);
		}
	}
	void handleDraggedRelease(MouseEvent& evt) {
	}
};
class gui_signaturecontrol_input : public guibuttonbase {
	const int idx;
public:
	gui_signaturecontrol_input(int _idx)
		: guibuttonbase(),
		idx(_idx)
	{
		setColor(nvgToRGB(g_guiColors[COL_BG_DRK]));
	}

	void render(NVGcontext* vg) {
		NVGcolor c;
		/*if (!enabled()) {
			c = G_BUTTON_DISABLED;
		}
		else*/ if (pressed() || focused()) {
			c = colorPressed;
		}
		else if (hovered()) {
			c = colorHover;
		}
		else {
			c = color;
		}
//		NVGcolor c2 = colorStroke;
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgFillColor(vg, c);
		nvgFill(vg);
		setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		int n;
		if (idx == 0) {
			n = MainCtrl::get()->sigNum();
		} else {
			n = MainCtrl::get()->sigDen();
		}
		String str = StringFormat("%d", n);
		nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
	}
	void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			MainCtrl::get()->captureMouse(this);
		}
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
			int disty = (int)evt.dragDistance->y / 20;
			if (abs(disty) < 1)
				return;
			evt.dragDistance->y = 0;
			if (idx == 0) {
				int n = MainCtrl::get()->sigNum();
				n = CLAMP_I(n - disty, 0, 32);
				MainCtrl::get()->setNum(n);
				MainCtrl::get()->updateGrid();
			} else {
				int prev = MainCtrl::get()->sigDen();
				int now = 1<<CLAMP_I((int)log2(prev) - disty, 0, 4);
				printf("old %d new %d\n", prev, now);
				MainCtrl::get()->setDen(now);
				MainCtrl::get()->updateGrid();
			}
		}
	}
	void handleDraggedRelease(MouseEvent& evt) {
	}

};
class gui_signaturecontrol : public guictr_base {
	gui_signaturecontrol_input inputNum;
	gui_signaturecontrol_input inputDen;
public:
	gui_signaturecontrol()
		: guictr_base(),
		inputNum(0),
		inputDen(1)
	{
		padding = 0;
		add(&inputNum);
		add(&inputDen);
	}
	~gui_signaturecontrol() {
		remove(&inputDen);
		remove(&inputNum);
	}

	bool enabled() {
		return true;
	}
	void layout() {
		inputNum.size = ivec2(30, size.y);
		inputDen.size = ivec2(30, size.y);
		inputNum.pos.x = (size.x / 4) - inputNum.size.x / 2;
		inputDen.pos.x = (size.x / 4)*3 - inputNum.size.x / 2;
	}
	void render(NVGcontext* vg) {
		renderWidgetBorder(vg);
		if (!setScissorTransform(vg)) {
			return;
		}
		String sigSep = "/";
		this->inputNum.render(vg);
		this->inputDen.render(vg);
		setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, size.x / 2.0f, G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(sigSep), NULL);
	}
};
class gui_timeinput_field : public guibuttonbase {
	const int idx;
	int32_t& time;
	bool drawBackground = true;
public:
	gui_timeinput_field(int _idx, int32_t& _time)
		: guibuttonbase(),
		idx(_idx),
		time(_time)
	{
		setColor(nvgToRGB(g_guiColors[COL_BG_DRK]));
	}
	void setDrawBackground(bool state) {
		drawBackground = state;
	}

	void render(NVGcontext* vg) {
		NVGcolor c;
		bool hl = false;
		/*if (!enabled()) {
			c = G_BUTTON_DISABLED;
		}
		else*/ if (pressed() || focused()) {
			c = colorPressed;
			hl = true;
		}
		else if (hovered()) {
			c = colorHover;
			hl = true;
		}
		else {
			c = color;
		}
		if (drawBackground) {
			renderWidgetBorder(vg);
		}
		if (drawBackground || hl) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, c);
			nvgFill(vg);
		}
		setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		beatbar16th_t step = MainCtrl::get()->toBeatBar16th(time);
		int32_t val = step[idx];
		String str = StringFormat("%d", val < 0 ? val : (val+1));
		nvgText(vg, pos.x + size.x-3, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
	}
	void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			MainCtrl::get()->captureMouse(this);
		}
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
			int disty = (int)evt.dragDistance->y / 20;
			if (abs(disty) < 1)
				return;
			evt.dragDistance->y = 0;
			switch (idx) {
			case 0:
				time -= disty*TICKS_BAR;
				break;
			case 1:
				time -= disty*TICKS_QUARTER;
				break;
			case 2:
				if (disty > 0) {
					if (time & TICK_MASK_16TH) {
						time &=~TICK_MASK_16TH;
						break;
					}
				}
				if (disty < 0) {
					if (time & TICK_MASK_16TH) {
						time &=~TICK_MASK_16TH;
					}
				}
				time -= disty*TICKS_16TH;
				break;
			}
		}
	}
	void handleDraggedRelease(MouseEvent& evt) {
	}

};
class gui_timeinput : public guictr_base {
	int32_t& time;
	gui_timeinput_field bar;
	gui_timeinput_field beat;
	gui_timeinput_field sixteenths;
	bool drawModeFullBG = false;
public:
	gui_timeinput(int32_t& _time)
		: guictr_base(),
		  time(_time),
		  bar(0, _time),
		  beat(1, _time),
		  sixteenths(2, _time)
	{
		padding = 0;
		time = 0;
		add(&bar);
		add(&beat);
		add(&sixteenths);
	}
	~gui_timeinput() {
		remove(&sixteenths);
		remove(&beat);
		remove(&bar);
	}
	bool enabled() {
		return true;
	}
	void setConnectedBG() {
		drawModeFullBG = true;
		bar.setDrawBackground(!drawModeFullBG);
		beat.setDrawBackground(!drawModeFullBG);
		sixteenths.setDrawBackground(!drawModeFullBG);
	}
	void layout() {
		int fieldH = size.y-2;
		int smallStepW = fieldH-5;
		int dist = 8;
		int barW = size.x-5-5-((smallStepW+8)*2);
		bar.size = ivec2(barW, fieldH);
		beat.size = ivec2(smallStepW, fieldH);
		sixteenths.size = ivec2(smallStepW, fieldH);
		bar.pos = ivec2(5, size.y/2-bar.size.y/2);
		beat.pos = ivec2(bar.right()+dist, bar.top());
		sixteenths.pos = ivec2(beat.right()+dist, beat.top());
	}
	void render(NVGcontext* vg) {
		if (drawModeFullBG) {
			renderWidgetBorder(vg);
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, bar.color);
			nvgFill(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
		this->bar.render(vg);
		this->beat.render(vg);
		this->sixteenths.render(vg);
		if (drawModeFullBG) {
			String str = ".";
			setFont(vg, G_FONT_SCALE(this->sixteenths.size.y), G_WHITE, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
			nvgText(vg, this->beat.pos.x, this->beat.pos.y + G_FONT_MIDDLE_OFFSET(this->beat.size.y), StringAsCStr(str), NULL);
			nvgText(vg, this->sixteenths.pos.x, this->sixteenths.pos.y + G_FONT_MIDDLE_OFFSET(this->sixteenths.size.y), StringAsCStr(str), NULL);
		}
	}
};

class guibutton_audioengine : public guibutton {

public:
	guibutton_audioengine() : guibutton() {
	}
	virtual bool enabled() {
		return vsthost::getInstance()->isStreaming();
	}
};
class guictr_tempocontrols : public guictr_base {
	project_t& project;
	gui_tempocontrol tempo;
	gui_signaturecontrol signature;
	gui_timeinput cursorPos;
	gui_timeinput songPos;
	guibutton_audioengine btnAudioOnOff;
	guibutton btnPlay;
	guibutton btnStop;
	guibutton btnLoop;
public:
	guictr_tempocontrols(project_t& _project)
		: guictr_base(),
		  project(_project),
		  cursorPos(MainCtrl::get()->cursor.cursorPos),
		  songPos(MainCtrl::get()->playbackPos)
	{
		btnAudioOnOff.setColor(0x00ddff);
		songPos.setConnectedBG();
		btnPlay.drawFn = drawPlaySymbol;
		btnStop.drawFn = drawStopSymbol;
		btnLoop.drawFn = drawTextureSymbol;
		btnLoop.drawParm = ICON_LOOP;
		btnLoop.setActiveRef(&project.loopEnabled);
		add(&tempo);
		add(&signature);
		add(&cursorPos);
		add(&btnLoop);
		add(&btnStop);
		add(&btnPlay);
		add(&songPos);
		add(&btnAudioOnOff);
		padding = 8;
	}
	~guictr_tempocontrols() {
		remove(&btnAudioOnOff);
		remove(&songPos);
		remove(&btnPlay);
		remove(&btnStop);
		remove(&btnLoop);
		remove(&cursorPos);
		remove(&signature);
		remove(&tempo);
	}
	void render(NVGcontext* vg) {
//		guictr_base::setScissorTransform(vg);
		ivec2 posInset = getPosContent();
		nvgTranslate(vg, posInset.x, posInset.y);
		for (guibase* gui : guis) {
			nvgSave(vg);
			gui->render(vg);
			nvgRestore(vg);
		}
	}
	void layout() {
		ivec2 cs = getSizeContent();
		int32_t spacing = 10;
		tempo.pos = ivec2(5, 5);
		tempo.size = ivec2(80, 28);
		signature.pos = ivec2(tempo.right() + spacing, 5);
		signature.size = ivec2(80, 28);
		cursorPos.pos = ivec2(signature.right() + spacing, 5);
		cursorPos.size = ivec2(120, 28);

		int32_t spacingCtrls = 5;
		btnLoop.size = btnStop.size = btnPlay.size = ivec2(32, 32);
		btnLoop.size.x = 48;
		songPos.size = ivec2(140, 32);
		int32_t transportWidth = btnPlay.size.x + spacingCtrls + btnStop.size.x+ spacingCtrls + songPos.size.x;
		int32_t transportCtrls = max(cs.x / 2 - transportWidth / 2, cursorPos.right() + spacing);
//		btnPlay.pos = ivec2(transportCtrls, 5);
//		btnStop.pos = ivec2(btnPlay.right() + spacingCtrls, 5);
//		songPos.pos = ivec2(btnStop.right() + spacingCtrls, 5);
//		btnLoop.pos = ivec2(songPos.right() + spacingCtrls, 5);
		int posX = transportCtrls;
		for (auto el :  (std::vector<guibase*>{&btnPlay, &btnStop, &songPos, &btnLoop})) {
			el->pos = ivec2(posX, 5);
			posX = el->right() + spacingCtrls;
		}

		btnAudioOnOff.size = ivec2(100, 28);
		btnAudioOnOff.pos = ivec2(max(songPos.right()+spacing, cs.x-5-btnAudioOnOff.size.x), 5);
		tempo.layout();
		signature.layout();
		cursorPos.layout();
		songPos.layout();
		btnPlay.layout();
		btnAudioOnOff.layout();
	}
	void buttonClicked(guibase* button) {
		if (button == &this->btnPlay) {
			MainCtrl::get()->startPlaying();
		}
		if (button == &this->btnStop) {
			MainCtrl::get()->stopPlaying();
		}
		if (button == &this->btnLoop) {
			project.loopEnabled = !project.loopEnabled;
		}
		if (button == &this->btnAudioOnOff) {
			if (vsthost::getInstance()->isStreaming()) {
				vsthost::getInstance()->stopAudio();
				settings.startEngine = false;
			} else {
				if (vsthost::getInstance()->startAudio()) {
					settings.startEngine = true;
				}
			}
		}
	}
};
