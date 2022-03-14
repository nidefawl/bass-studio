#pragma once

#include <cstdint>
#include <vector>

#include "math/seq_math.h"
#include "str_util.h"
#include "color_util.h"

#include "gui.h"
#include "guicontainer.h"
#include "button.h"
#include "renderresources.h"
#include "knob.h"
#include "host/vst_host.h"
#include "host/mainctrl.h"
#include "platform.h"

class gui_tempocontrol : public guibutton {
public:
    gui_tempocontrol()
        : guibutton() {
    }
    void render(NVGcontext* vg) override {
        renderWidgetBorder(vg, getStateFlags());
        String tempo = FormatTempo(dawCtrl->getDaw()->getCurrentTempoBPM());
        setFont(vg, G_FONT_SCALE(size.y), THEMECOL_TEXT, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(tempo), NULL);
    }
    void handleDraggedBegin(MouseEvent& evt) override {
        if (evt.guiDragged == this) {
            parentCtrl->captureMouse(this);
        }
    }
    void handleDraggedMove(MouseEvent& evt) override {
        if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
            int disty = (int) evt.dragDistance->y / 10;
            if (math::abs(disty) < 1)
                return;
            evt.dragDistance->y = 0;
            auto const daw = dawCtrl->getDaw();
            int tempo = daw->getCurrentTempo();
            daw->setTempo(tempo - disty * 100);
            daw->updateVisibleTrackContents();
        }
    }
    void handleDraggedRelease(MouseEvent& evt) override {
    }
};
class gui_signaturecontrol_input : public guibutton {
    const int idx;

public:
    gui_signaturecontrol_input(int _idx)
        : guibutton(),
          idx(_idx) {
    }

    void render(NVGcontext* vg) override {
        int32_t fl = getStateFlags();
        renderWidgetBorder(vg, fl);
        setFont(vg, G_FONT_SCALE(size.y), THEMECOL_TEXT, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        int n;
        if (idx == 0) {
            n = dawCtrl->getDaw()->sigNum();
        } else {
            n = dawCtrl->getDaw()->sigDen();
        }
        String str = StringFormat("%d", n);
        nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
    }
    void handleDraggedBegin(MouseEvent& evt) override {
        if (evt.guiDragged == this) {
            parentCtrl->captureMouse(this);
        }
    }
    void handleDraggedMove(MouseEvent& evt) override {
        if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
            auto daw = dawCtrl->getDaw();
            int disty = (int) evt.dragDistance->y / 20;
            if (math::abs(disty) < 1)
                return;
            evt.dragDistance->y = 0;
            if (idx == 0) {
                int n = daw->sigNum();
                n     = CLAMP_I(n - disty, 0, 32);
                daw->setNum(n);
            } else {
                int prev = daw->sigDen();
                int now  = 1 << CLAMP_I((int) log2(prev) - disty, 0, 4);
                printf("old %d new %d\n", prev, now);
                daw->setDen(now);
            }
            daw->updateVisibleTrackContents();
        }
    }
    void handleDraggedRelease(MouseEvent& evt) override {
    }
};
class gui_signaturecontrol : public guictr_base {
    gui_signaturecontrol_input inputNum;
    gui_signaturecontrol_input inputDen;

public:
    gui_signaturecontrol()
        : guictr_base(),
          inputNum(0),
          inputDen(1) {
        padding = 0;
        add(&inputNum);
        add(&inputDen);
    }
    ~gui_signaturecontrol() override {
        removeGuis();
    }

    bool enabled() {
        return true;
    }
    void layout() override {
        inputNum.size  = ivec2(30, size.y);
        inputDen.size  = ivec2(30, size.y);
        inputNum.pos.x = (size.x / 4) - inputNum.size.x / 2;
        inputDen.pos.x = (size.x / 4) * 3 - inputNum.size.x / 2;
    }
    void render(NVGcontext* vg) override {
        renderWidgetBorder(vg, getStateFlags());
        if (!setScissorTransform(vg)) {
            return;
        }
        String sigSep = "/";
        this->inputNum.render(vg);
        this->inputDen.render(vg);
        setFont(vg, G_FONT_SCALE(size.y), THEMECOL_TEXT, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, size.x / 2.0f, G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(sigSep), NULL);
    }
};
class gui_timeinput_field : public guibutton {
    const int idx;
    int32_t* time;
    const bool isRelative;

public:
    gui_timeinput_field(int _idx, int32_t* _time, const bool _isRelative);
    void setRef(int32_t* time) {
        this->time = time;
    }
    void render(NVGcontext* vg) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override {
        return parent ? parent->getTooltip(appctrl) : nullptr;
    }
};
class gui_timeinput : public guictr_base {
    int32_t* time = nullptr;
    gui_timeinput_field bar;
    gui_timeinput_field beat;
    gui_timeinput_field sixteenths;

public:
    gui_timeinput(int32_t* _time, const bool isRelative = false);
    ~gui_timeinput() override {
        removeGuis();
    }
    void setRef(int32_t* time);
    void setConnectedBG();
    void layout() override;
    void buttonClicked(guibase* button) override {
        if (parent)
            parent->buttonClicked(this);
    }
    void render(NVGcontext* vg) override;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
    int32_t getTime();
};

class guibutton_audioengine : public guibuttonstate {
    host_stats_t stats;
    float cpuUsage = 0.0f;

public:
    guibutton_audioengine() = default;
    bool getState() const override {
        return vsthost::getInstance()->isStreaming();
    }
    void render(NVGcontext* vg) override;
    void prerender(NVGcontext* vg) override;
    void renderWidgetBorderPosSize(NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size) const override;

};
class guictr_tempocontrols : public guictr_base {
    project_globals_t& projectGlobals;
    gui_tempocontrol tempo;
    gui_signaturecontrol signature;
    gui_timeinput cursorPos;
    gui_timeinput songPos;
    guibutton_audioengine btnAudioOnOff;
    guibuttonstate btnRecord;
    guibuttonstate btnPlay;
    guibuttonstate btnStop;
    guibuttonstate btnLoop;
    gui_timeinput loopPos;
    gui_timeinput loopLen;

public:
    guictr_tempocontrols(project_t& _project, project_globals_t& _projectGlobals)
        : guictr_base(),
          projectGlobals(_projectGlobals),
          cursorPos(&projectGlobals.cursor.cursorPos),
          songPos(&projectGlobals.playbackPos),
          loopPos(&projectGlobals.loopStart),
          loopLen(&projectGlobals.loopLen, true) {
        //btnAudioOnOff.setTint(0x00ddff);
        songPos.setConnectedBG();
        loopPos.setConnectedBG();
        loopLen.setConnectedBG();
        btnRecord.drawFn = drawRecordSymbol;
        btnPlay.drawFn   = drawPlaySymbol;
        btnStop.drawFn   = drawStopSymbol;
        btnLoop.drawFn   = drawTextureSymbol;
        btnLoop.drawParm = ICON_LOOP;
        btnLoop.setFlag(FLG_RENDER_BUTTON_WITH_LED, true);
        btnLoop.setStateRef(&projectGlobals.loopEnabled);
        btnRecord.setStateRef(&projectGlobals.recordArmed);
        btnRecord.setButtonColor(GuiColor::COL_BTN_RECORD_ARM_BG);
        add(&loopLen);
        add(&loopPos);
        add(&tempo);
        add(&signature);
        add(&cursorPos);
        add(&btnLoop);
        add(&btnStop);
        add(&btnPlay);
        add(&btnRecord);
        add(&songPos);
        add(&btnAudioOnOff);
        padding = 8;
    }
    ~guictr_tempocontrols() override {
        remove(&btnAudioOnOff);
        remove(&songPos);
        remove(&btnRecord);
        remove(&btnPlay);
        remove(&btnStop);
        remove(&btnLoop);
        remove(&cursorPos);
        remove(&signature);
        remove(&tempo);
        remove(&loopPos);
        remove(&loopLen);
    }
    void render(NVGcontext* vg) override {
        //guictr_base::setScissorTransform(vg);
        ivec2 posInset = getPosContent();
        nvgTranslate(vg, posInset.x, posInset.y);
        for (guibase* gui : guis) {
            nvgSave(vg);
            gui->render(vg);
            nvgRestore(vg);
        }
    }
    void layout() override {
        ivec2 cs        = getSizeContent();
        int32_t spacing = 10;
        tempo.pos       = ivec2(5, 5);
        tempo.size      = ivec2(80, 28);
        signature.pos   = ivec2(tempo.right() + spacing, 5);
        signature.size  = ivec2(80, 28);
        cursorPos.pos   = ivec2(signature.right() + spacing, 5);
        cursorPos.size  = ivec2(120, 28);

        int32_t spacingCtrls = 5;
        btnRecord.size = btnLoop.size = btnStop.size = btnPlay.size = ivec2(32, 32);

        btnLoop.size.x = 48;
        loopPos.size   = ivec2(100, 32);
        loopLen.size   = ivec2(100, 32);
        songPos.size   = ivec2(140, 32);

        int32_t transportWidth = btnPlay.size.x + spacingCtrls + btnStop.size.x + spacingCtrls + songPos.size.x;
        int32_t transportCtrls = math::max(cs.x / 2 - transportWidth / 2, cursorPos.right() + spacing);

        std::vector<guibase*> v{ &btnRecord, &btnPlay, &btnStop, &songPos };
        std::vector<guibase*> v2{ &btnLoop, &loopPos, &loopLen };
        int posX = transportCtrls;
        for (auto el : v) {
            el->pos = ivec2(posX, 5);
            posX    = el->right() + spacingCtrls;
        }
        posX += spacingCtrls * 3;
        for (auto el : v2) {
            el->pos = ivec2(posX, 5);
            posX    = el->right() + spacingCtrls;
        }

        btnAudioOnOff.size = ivec2(100, 28);
        btnAudioOnOff.pos  = ivec2(math::max(songPos.right() + spacing, cs.x - 5 - btnAudioOnOff.size.x), 5);
//        tempo.layout();
//        signature.layout();
//        cursorPos.layout();
//        songPos.layout();
//        btnPlay.layout();
//        btnAudioOnOff.layout();
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    void buttonClicked(guibase* button) override;
};
