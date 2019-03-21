#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "str_util.h"
#include "color_util.h"

#include "gui.h"
#include "guicontainer.h"
#include "button.h"
#include "renderresources.h"
#include "knob.h"
#include "../host/vst_host.h"
#include "settings.h"

void testTask();
class gui_tempocontrol : public guibuttonbase {
public:
	gui_tempocontrol()
		:  guibuttonbase() {
	}
	void render(NVGcontext* vg) {
		renderWidgetBorder(vg, getStateFlags());
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
			MainCtrl::get()->updateVisibleTrackContents();
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
		idx(_idx) {
	}

	void render(NVGcontext* vg) {
		int32_t flags = getStateFlags();
		if (flags & (FLG_FOC|FLG_HVRD|FLG_DRG|FLG_ACT)) {
			renderWidgetBorder(vg, flags);
		}
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
		removeGuis();
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
		renderWidgetBorder(vg, getStateFlags());
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
	int32_t* time;
	const bool isRelative;
	bool drawBackground = true;
public:
	gui_timeinput_field(int _idx, int32_t* _time, const bool _isRelative)
		: guibuttonbase(),
		idx(_idx),
		time(_time),
		isRelative(_isRelative)
	{
	}
	void setDrawBackground(bool state) {
		drawBackground = state;
	}
	void setRef(int32_t* time) {
		this->time = time;
	}

	void render(NVGcontext* vg) {
		int32_t flags = getStateFlags();
		if (drawBackground || flags > FLG_ENBL) {
			renderWidgetBorder(vg, flags);
		}
		setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		int32_t _time = time ? *time : 0;
		beatbar16th_t step = MainCtrl::get()->toBeatBar16th(_time);
		int32_t val = step[idx];
		if (val >= 0 && !isRelative) {
			val++;
		}
		String str = StringFormat("%d", val);
		nvgText(vg, pos.x + size.x-3, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
	}
	void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			parentCtrl->captureMouse(this);
		}
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (time && evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
			int disty = (int)evt.dragDistance->y / 20;
			if (abs(disty) < 1)
				return;
			evt.dragDistance->y = 0;
			switch (idx) {
			case 0:
				*time -= disty*TICKS_BAR;
				break;
			case 1:
				*time -= disty*TICKS_QUARTER;
				break;
			case 2:
				if (disty > 0) {
					if (*time & TICK_MASK_16TH) {
						*time &=~TICK_MASK_16TH;
						break;
					}
				}
				if (disty < 0) {
					if (*time & TICK_MASK_16TH) {
						*time &=~TICK_MASK_16TH;
					}
				}
				*time -= disty*TICKS_16TH;
				break;
			}
			if (parent)
				parent->buttonClicked(this);

		}
	}
	void handleDraggedRelease(MouseEvent& evt) {
	}

};
class gui_timeinput : public guictr_base {
	int32_t* time = nullptr;
	gui_timeinput_field bar;
	gui_timeinput_field beat;
	gui_timeinput_field sixteenths;
	bool drawModeFullBG = false;
public:
	gui_timeinput(int32_t* _time, const bool isRelative = false)
		: guictr_base(),
		  time(_time),
		  bar(0, _time, isRelative),
		  beat(1, _time, isRelative),
		  sixteenths(2, _time, isRelative)
	{
		padding = 0;
		add(&bar);
		add(&beat);
		add(&sixteenths);
	}
	~gui_timeinput() {
		removeGuis();
	}
	void setRef(int32_t* time) {
		this->time = time;
		bar.setRef(time);
		beat.setRef(time);
		sixteenths.setRef(time);
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
		int inset = 4;
		int fieldH = size.y;
		int barW = (size.x)/2;
		int smallStepW = (size.x-barW-inset*2)/2;
		bar.size = ivec2(barW, fieldH);
		beat.size = ivec2(smallStepW, fieldH);
		sixteenths.size = ivec2(smallStepW, fieldH);
		bar.pos = ivec2(0, size.y/2-bar.size.y/2);
		beat.pos = ivec2(bar.right()+inset, bar.top());
		sixteenths.pos = ivec2(beat.right()+inset, beat.top());
	}
	void buttonClicked(guibase* button) override {
		if (parent)
			parent->buttonClicked(this);
	}
	void render(NVGcontext* vg) {
		if (drawModeFullBG) {
			renderWidgetBorder(vg, getStateFlags());
//			nvgBeginPath(vg);
//			nvgRect(vg, pos.x, pos.y, size.x, size.y);
//			nvgFillColor(vg, bar.color);
//			nvgFill(vg);
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
	bool isEnabled() const override {
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
	gui_timeinput loopPos;
	gui_timeinput loopLen;
public:
	guictr_tempocontrols(project_t& _project)
		: guictr_base(),
		  project(_project),
		  cursorPos(&MainCtrl::get()->cursor.cursorPos),
		  songPos(&MainCtrl::get()->playbackPos),
		  loopPos(&MainCtrl::get()->loopStart),
		  loopLen(&MainCtrl::get()->loopLen, true)
	{
//		btnAudioOnOff.setTint(0x00ddff);
		songPos.setConnectedBG();
		loopPos.setConnectedBG();
		loopLen.setConnectedBG();
		btnPlay.drawFn = drawPlaySymbol;
		btnStop.drawFn = drawStopSymbol;
		btnLoop.drawFn = drawTextureSymbol;
		btnLoop.drawParm = ICON_LOOP;
		btnLoop.setEnabledRef(&project.loopEnabled);
		add(&loopLen);
		add(&loopPos);
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
		remove(&loopPos);
		remove(&loopLen);
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
		loopPos.size = ivec2(100, 32);
		loopLen.size = ivec2(100, 32);
		songPos.size = ivec2(140, 32);
		int32_t transportWidth = btnPlay.size.x + spacingCtrls + btnStop.size.x+ spacingCtrls + songPos.size.x;
		int32_t transportCtrls = max(cs.x / 2 - transportWidth / 2, cursorPos.right() + spacing);
//		btnPlay.pos = ivec2(transportCtrls, 5);
//		btnStop.pos = ivec2(btnPlay.right() + spacingCtrls, 5);
//		songPos.pos = ivec2(btnStop.right() + spacingCtrls, 5);
//		btnLoop.pos = ivec2(songPos.right() + spacingCtrls, 5);
		int posX = transportCtrls;
		for (auto el :  (std::vector<guibase*>{&btnPlay, &btnStop, &songPos})) {
			el->pos = ivec2(posX, 5);
			posX = el->right() + spacingCtrls;
		}
		posX += spacingCtrls*3;
		for (auto el :  (std::vector<guibase*>{&btnLoop, &loopPos, &loopLen})) {
			el->pos = ivec2(posX, 5);
			posX = el->right() + spacingCtrls;
		}

		btnAudioOnOff.size = ivec2(100, 28);
		btnAudioOnOff.pos = ivec2(max(songPos.right()+spacing, cs.x-5-btnAudioOnOff.size.x), 5);
//		tempo.layout();
//		signature.layout();
//		cursorPos.layout();
//		songPos.layout();
//		btnPlay.layout();
//		btnAudioOnOff.layout();
		for (guibase* gui : guis) {
			gui->layout();
		}
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
