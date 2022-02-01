#include "tempocontrols.h"

#include <cstdint>

#include "math/seq_math.h"
#include "str_util.h"
#include "color_util.h"

#include "gui.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "guitooltip.h"
#include "theme.h"
#include "button.h"
#include "renderresources.h"
#include "knob.h"
#include "host/vst_host.h"
#include "host/audio_host.h"
#include "basectrl.h"
#include "host/mainctrl.h"
#include "appsettings.h"


namespace GuiColor {
    constant_t COL_BTN_RECORD_ARM_BG("COL_BTN_RECORD_ARM_BG", 0xFF442222);
}

using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
using Table::tblString;

template<>
void guitooltip<gui_timeinput>::layout() {
    size.x          = 140;
    table.rowHeight = FONT_SIZE_TOOLTIP + INSET_TABLE_CELL_PADDING * 2;
    table.rows.clear();
    table.titleCols.clear();
    table.colSizes.clear();

    {
        tbl_row_t row{};
        row.cols.push_back(tblString{ "Tick" });
        row.cols.push_back(tblint{ ptr->getTime() });
        table.rows.push_back(row);
    }

    Table::AdjustColSizes(table, getSizeContent() - ivec2(INSET_TABLE << 1));
    size.y = table.rows.size() * table.rowHeight;
}
guictxtmenu_base* gui_timeinput::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<gui_timeinput>(this);
    return tooltip;
}
gui_timeinput_field::gui_timeinput_field(int _idx, int32_t* _time, const bool _isRelative)
    : guibutton(), idx(_idx), time(_time), isRelative(_isRelative) {
}

void gui_timeinput_field::render(NVGcontext* vg) {
    int32_t flags = getStateFlags();
    renderWidgetBorder(vg, flags);
    setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    int32_t _time      = time ? *time : 0;
    beatbar16th_t step = project_controller_t::get()->toBeatBar16th(_time);
    int32_t val        = step[idx];
    if (val >= 0 && !isRelative) {
        val++;
    }
    String str = StringFormat("%d", val);
    nvgText(vg, pos.x + size.x - 3, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
}

void gui_timeinput_field::handleDraggedBegin(MouseEvent& evt) {
    if (evt.guiDragged == this) {
        parentCtrl->captureMouse(this);
    }
}

void gui_timeinput_field::handleDraggedMove(MouseEvent& evt) {
    if (time && evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
        int disty = (int) evt.dragDistance->y / 20;
        if (math::abs(disty) < 1)
            return;
        evt.dragDistance->y = 0;
        int32_t curVal      = *time;
        switch (idx) {
            case 0:
                curVal -= disty * TICKS_BAR;
                break;
            case 1:
                curVal -= disty * TICKS_QUARTER;
                break;
            case 2:
                if (disty > 0) {
                    if (curVal & TICK_MASK_16TH) {
                        curVal &= ~TICK_MASK_16TH;
                        break;
                    }
                }
                if (disty < 0) {
                    if (curVal & TICK_MASK_16TH) {
                        curVal &= ~TICK_MASK_16TH;
                    }
                }
                curVal -= disty * TICKS_16TH;
                break;
        }
        if (!isRelative || curVal > 0) {
            *time = curVal;
        }
        if (parent)
            parent->buttonClicked(this);
    }
}

void gui_timeinput_field::handleDraggedRelease(MouseEvent& evt) {
}

gui_timeinput::gui_timeinput(int32_t* _time, const bool isRelative) : guictr_base(), time(_time), bar(0, _time, isRelative), beat(1, _time, isRelative), sixteenths(2, _time, isRelative) {
    padding = 0;
    add(&bar);
    add(&beat);
    add(&sixteenths);
    setCanMouseHit(true);
}

int32_t gui_timeinput::getTime() {
    return *time;
}
void gui_timeinput::setRef(int32_t* time) {
    this->time = time;
    bar.setRef(time);
    beat.setRef(time);
    sixteenths.setRef(time);
}

void gui_timeinput::setConnectedBG() {
    setBackgroundRendered(true);
    bar.setBackgroundRendered(false);
    beat.setBackgroundRendered(false);
    sixteenths.setBackgroundRendered(false);
}

void gui_timeinput::layout() {
    int inset       = 4;
    int fieldH      = size.y;
    int barW        = (size.x) / 2;
    int smallStepW  = (size.x - barW - inset * 2) / 2;
    bar.size        = ivec2(barW, fieldH);
    beat.size       = ivec2(smallStepW, fieldH);
    sixteenths.size = ivec2(smallStepW, fieldH);
    bar.pos         = ivec2(0, size.y / 2 - bar.size.y / 2);
    beat.pos        = ivec2(bar.right() + inset, bar.top());
    sixteenths.pos  = ivec2(beat.right() + inset, beat.top());
}
namespace {

    void drawBg(NVGcontext* vg, guibase* b) {
        ivec2 pos  = b->pos;
        ivec2 size = b->size;
        auto col   = b->theme->getBgColor(b->getStateFlags());
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, col);
        nvgFill(vg);
    }
}// namespace
void gui_timeinput::render(NVGcontext* vg) {
    int flags = getStateFlags();
    for (auto g : guis) {
        if (g->hovered() || g->pressed())
            flags |= FLG_HVRD;
    }
    renderWidgetBorder(vg, flags);
    if (!setScissorTransform(vg)) {
        return;
    }
    if (isBackgroundRendered()) {
        if (this->bar.getStateFlags() & (FLG_HVRD | FLG_DRG))
            drawBg(vg, &this->bar);
        if (this->beat.getStateFlags() & (FLG_HVRD | FLG_DRG))
            drawBg(vg, &this->beat);
        if (this->sixteenths.getStateFlags() & (FLG_HVRD | FLG_DRG))
            drawBg(vg, &this->sixteenths);
    }

    this->bar.render(vg);
    this->beat.render(vg);
    this->sixteenths.render(vg);
    if (isBackgroundRendered()) {
        String str = ".";
        setFont(vg, G_FONT_SCALE(this->sixteenths.size.y), G_WHITE, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(vg, this->beat.pos.x, this->beat.pos.y + G_FONT_MIDDLE_OFFSET(this->beat.size.y), StringAsCStr(str), NULL);
        nvgText(vg, this->sixteenths.pos.x, this->sixteenths.pos.y + G_FONT_MIDDLE_OFFSET(this->sixteenths.size.y), StringAsCStr(str),
                NULL);
    }
}
void updateSrBs();
void guictr_tempocontrols::buttonClicked(guibase* button) {
    if (button == &this->btnPlay) {
        DawInstance::get()->startPlaying();
    }
    if (button == &this->btnStop) {
        DawInstance::get()->stopPlaying();
    }
    if (button == &this->btnRecord) {
        projectGlobals.recordArmed = !projectGlobals.recordArmed;
    }
    if (button == &this->btnLoop) {
        projectGlobals.loopEnabled = !projectGlobals.loopEnabled;
    }
    if (button == &this->btnAudioOnOff) {
        using DAW::settings;
        settings.startEngine = !settings.startEngine;
        updateSrBs();
    }
}

void guibutton_audioengine::prerender(NVGcontext* vg) {
    if (dawCtrl) {
        ThreadLock lock = dawCtrl->getDaw()->getPlayThread()->tryLockThread();
        if (lock.isLocked()) {
            vsthost::getInstance()->getStats(stats);
        } else {
            this->cpuUsage = vsthost::getInstance()->getCpuUsage();
        }
    }
}

void guibutton_audioengine::render(NVGcontext* vg) {
    audiohost* ahost = audiohost::getInstance();
    if (!ahost || !ahost->isStreaming()) {
        setText("Off");
    } else {
        setText(StringFormat("%.0f%%", this->cpuUsage * 100.0));
    }
    int32_t fl = getStateFlags();
    renderWidgetBorder(vg, fl);
    renderButtonLabel(vg, fl);
}

NVGcolor guibutton_audioengine::getBackgroundColor(int stateflags) const {
    NVGcolor c = theme->getBgColor(stateflags);
    //lastTickBar = stats.tickBar;
    auto projCtrl = project_controller_t::get();
    if (projCtrl) {
        const auto& globals        = projCtrl->getGlobals();
        const float fBarNumFloor   = (int32_t) stats.tickBar / (int32_t) TICKS_BAR;
        const auto tmConstantBar   = 60000.0 * 100.0 / globals.tempo100;
        const float barProgress    = getTimeMillisd() / tmConstantBar;
        float fMo                  = fmodf(barProgress, 1.0f);
        const float fBarTmAbsolute = fBarNumFloor + fMo;
        float t                    = std::sin(fBarTmAbsolute * M_PI * 2.0f) * 0.5f + 0.5f;

        vec4 v{ c.r, c.g, c.b, c.a };
        vec4 v2 = v;
        v2.r    = math::min(1.0f, v.r * this->cpuUsage);
        v2.b    = math::min(1.0f, v.b * t);
        float d = (float) (math::clamp(t, 0.0f, 1.0f));
        v       = v + d * (v2 - v);
        return NVGcolor{ v.x, v.y, v.z, v.w };
    }


    return c;
}
