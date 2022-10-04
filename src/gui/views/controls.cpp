#include "controls.h"

#include "keyboard.h"
#include "seq_time.h"
#include "tls.h"
#include "types.h"

#include "math/seq_math.h"
#include "str_util.h"
#include "color_util.h"

#include "gui/gui.h"
#include "gui/container/container.h"
#include "guicolors.h"
#include "gui/tooltip/tooltip.h"
#include "theme.h"
#include "gui/controls/button.h"
#include "renderresources.h"
#include "gui/controls/knob.h"
#include "host/host.h"
#include "host/audio_host.h"
#include "basectrl.h"
#include "host/mainctrl.h"
#include "appsettings.h"


using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
using Table::tblString;

template<>
void guitooltip<gui_timeinput>::setContent() {
    table.tableWidth = 140;
    tbl_row_t row{};
    row.cols.push_back(tblString{ "Tick" });
    row.cols.push_back(tblint{ ptr->getTime() });
    table.rows.push_back(row);
}
guictxtmenu_base* gui_timeinput::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<gui_timeinput>(this);
    return tooltip;
}
gui_timeinput_field::gui_timeinput_field(gui_timeinput* parentInput, int _idx, int32_t* _time, const bool _isRelative)
    : guibutton(), idx(_idx), isRelative(_isRelative), parentInput(parentInput), time(_time) {
    setFlag(FLG_RENDER_BACKGROUND_INSET, true);
    setFlag(FLG_BG_SHADING, true);
}

void gui_timeinput_field::render(NVGcontext* vg) {
    int32_t flags = getStateFlags();
    renderWidgetBorder(vg, flags);
    setFont(vg, G_FONT_SCALE(size.y), THEMECOL_TEXT, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    int32_t _time      = time ? *time : 0;
    beatbar16th_t step =  project_controller_t::get()->toBeatBar16th(_time, isRelative);
    int32_t val        = step[idx];
    if ((val < 0) == isRelative) {
        val++;
    }
    String str = StringFormat("%d", val);
    if (_time < 0 && isRelative && idx == 0 && val == 0) {
        str = "-" + str;
    }
    nvgText(vg, pos.x + size.x - 3, pos.y + G_FONT_MIDDLE_OFFSET(size.y), str.c_str(), &str.back() + 1);
}

void gui_timeinput_field::handleDraggedBegin(MouseEvent& evt) {
    if (time && (isCtrl(evt.kbmods) || (evt.type == MouseEventType::M_EVT_DOUBLECLICK))) { 
        if (parent) parent->buttonClicked(this);
        return;
    }
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
        onKeyInputChangeValue(ivec2(0, -disty));
    }
}

void gui_timeinput_field::handleDraggedRelease(MouseEvent& evt) {
}

bool gui_timeinput_field::handleKeyInput(KeyEvent& kevt) {
    bool handled = false;
    if (kevt.type != K_RELEASE) {
        if (isArrowKey(kevt.keyCode)) {
            ivec2 dir;
            arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
            if (dir.y) {
                if ((kevt.mods & KB_MOD_SHIFT)) {
                    dir *= 12;
                }
                onKeyInputChangeValue(dir);
                handled = true;
            }
        }
    }
    if (!handled) {
        if (kevt.type != K_RELEASE && isNumericInput(kevt.keyCode)) {
            parentInput->buttonClicked(this);
        }
    }
    return handled;
}

void gui_timeinput_field::onKeyInputChangeValue(ivec2 direction) {
    auto disty = direction.y;
    int32_t curVal      = *time;
    switch (idx) {
        case 0:
            curVal += disty * TICKS_BAR;
            break;
        case 1:
            curVal += disty * TICKS_QUARTER;
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
            curVal += disty * TICKS_16TH;
            break;
    }
    setNewValue(curVal);
    if (parentInput)
        parentInput->onInputChanged(this);
}

gui_timeinput::gui_timeinput(int32_t* _time, const bool isRelative)
    : guictr_base(),
    time(_time),
    bar(this, 0, _time, isRelative),
    beat(this, 1, _time, isRelative),
    sixteenths(this, 2, _time, isRelative)
{
    setCanGoNegative(!isRelative);
    padding = 0;
    add(&bar);
    add(&beat);
    add(&sixteenths);
    setCanMouseHit(true);
    setBackgroundRendered(false);
    setFlag(FLG_RENDER_BACKGROUND_INSET, true);
    setFlag(FLG_BG_SHADING, true);
    editfield.setFlag(FLG_NO_LAYOUT, true);
    editfield.setVisible(false);
    editfield.setAlignment(gui_textfield::Alignment::Center);
    editfield.setReturnCommits(true);
    add(&editfield);
    bar.setFlag(FLG_RENDER_BACKGROUND_INSET, true);
    beat.setFlag(FLG_RENDER_BACKGROUND_INSET, true);
    sixteenths.setFlag(FLG_RENDER_BACKGROUND_INSET, true);
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
        setFont(vg, G_FONT_SCALE(this->sixteenths.size.y), THEMECOL_TEXT, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(vg, this->beat.pos.x, this->beat.pos.y + G_FONT_MIDDLE_OFFSET(this->beat.size.y), StringAsCStr(str), NULL);
        nvgText(vg, this->sixteenths.pos.x, this->sixteenths.pos.y + G_FONT_MIDDLE_OFFSET(this->sixteenths.size.y), StringAsCStr(str),
                NULL);
    }
    if (editfield.isVisible()) {
        editfield.render(vg);
    }
}

void gui_timeinput::onInputChanged(const gui_timeinput_field* input) {
    if (parent)
        parent->buttonClicked(this);
}
void gui_timeinput::showEditField() {
    const bool isRelative = bar.isRelative;
    auto daw = dawCtrl->getDaw();
    editfield.mCallbackEnd = [this, isRelative](const std::string& str) {
        auto daw = dawCtrl->getDaw();
        auto beatBarNth = stringToBeatBarNth(str, isRelative, daw->getGlobals().signatureNum, daw->getGlobals().signatureDenom);
        auto tick = daw->beatBarNthToTick(beatBarNth, isRelative);
        editfield.setVisible(false);
        if (time) {
            *time = tick;
            onInputChanged(&bar);
        }
        return true;
    };
    editfield.pos  = {};
    editfield.size = size;
    editfield.setVisible(true);
    editfield.layout();
    auto beatBarNth = daw->toBeatBar16th(*time, isRelative);
    log_lf(Log::L_DEBUG, "beatBarNthToString beg: %s\n", StringAsCStr(beatBarNthToString(beatBarNth, isRelative)));
    editfield.setValue(beatBarNthToString(beatBarNth, bar.isRelative));
    editfield.setSelectionRange(-1, -1);
    editfield.setFontSize(bar.size.y);
    parentCtrl->focusGui(&editfield);
}

void gui_timeinput::buttonClicked(guibase* button) {
    if (time) {
        auto field = dynamic_cast<gui_timeinput_field*>(button);
        if (field) {
            showEditField();
            return;
        }
    }
    guictr_base::buttonClicked(button);
}


void guictr_tempocontrols::buttonClicked(guibase* button) {
    if (button == &this->btnPlay) {
        dawCtrl->getDaw()->startPlaying();
    }
    if (button == &this->btnStop) {
        dawCtrl->getDaw()->stopPlaying();
    }
    if (button == &this->btnRecord) {
        projectGlobals.recordArmed = !projectGlobals.recordArmed;
    }
    if (button == &this->btnLoop) {
        projectGlobals.loopEnabled = !projectGlobals.loopEnabled;
    }
    if (button == &this->btnAudioOnOff) {
        auto& settings = daw_tls::getSettings();
        settings.dawsettings.audioEnabled = !settings.dawsettings.audioEnabled;
        dawCtrl->getDaw()->configureSampleRate();
    }
}

void guibutton_audioengine::prerender(NVGcontext* vg) {
    if (dawCtrl) {
        auto daw = dawCtrl->getDaw();
        ThreadLock lock = daw->getPlayThread()->tryLockThread();
        if (lock.isLocked()) {
            daw->getHost()->getStats(stats);
        } else {
            this->cpuUsage = daw->getHost()->getCpuUsage();
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

void guibutton_audioengine::renderWidgetBorderPosSize(NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size) const {
    nvgBeginPath(vg);
    nvgRect(vg, pos.x, pos.y, size.x, size.y);
    nvgFillColor(vg, theme->getBgStrokeColor(flags));
    nvgFill(vg);
    int n       = theme->get(GuiConstant::CONST_GUI_INSET_WIDGET_BG);
    auto bgPos  = pos + ivec2(n);
    auto bgSize = size - ivec2(n * 2);
    if (bgSize.x > 0 && bgSize.y > 0) {

        
        NVGcolor bgColor = theme->getColor(getBackgroundColor());
        auto const daw = dawCtrl ? dawCtrl->getDaw() : nullptr;
        if (daw) {
            const auto& globals      = daw->getGlobals();
            const float fBarNumFloor = (int32_t) stats.tickBar / (int32_t) TICKS_BAR;

            // yikes, this sucks. Taking the tickBar from non-determenistic host stats _and_ using own timer to get per GPU frame progress
            const auto tmConstantBar  = 60000.0 * 100.0 / static_cast<double>(globals.tempo100);
            const auto barProgress    = getTimeMillisD() / tmConstantBar;
            const auto fBarTmAbsolute = fBarNumFloor + fmod(barProgress, 1.0);

            float t = static_cast<float>(std::sin(fBarTmAbsolute * M_PI * 2.0) * 0.5 + 0.5);

            vec4 v{ bgColor.r, bgColor.g, bgColor.b, bgColor.a };
            vec4 v2 = v;
            v2.r    = math::min(1.0f, v.r * this->cpuUsage);
            v2.b    = math::min(1.0f, v.b * t);
            float d = (float) (math::clamp(t, 0.0f, 1.0f));
            v       = v + d * (v2 - v);
            bgColor = NVGcolor{ v.x, v.y, v.z, v.w };
        }
        nvgBeginPath(vg);
        nvgRect(vg, bgPos.x, bgPos.y, bgSize.x, bgSize.y);
        nvgFillColor(vg, bgColor);
        nvgFill(vg);
    }
}

void gui_tempocontrol::buttonClicked(guibase* button) {
    auto field = dynamic_cast<gui_tempocontrol_input*>(button);
    if (field) {
        showEditField();
        return;
    }
    guictr_base::buttonClicked(button);
}
void gui_tempocontrol::onInputChanged(const gui_tempocontrol_input* input) {
    auto const daw = dawCtrl->getDaw();
    daw->updateVisibleTrackContents();
}
void gui_tempocontrol::showEditField() {
    editfield.mCallbackEnd = [this](const String& str) {
        auto daw = dawCtrl->getDaw();
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(str)));
        editfield.setVisible(false);
        daw->setTempo(math::clamp(math::roundfS32(fTextFieldVal*100.0f), 100, 99900));
        onInputChanged(&this->tempoInput);
        return true;
    };
    editfield.pos  = {};
    editfield.size = size;
    editfield.setVisible(true);
    editfield.layout();
    editfield.setValue(FormatTempo(dawCtrl->getDaw()->getCurrentTempoBPM()));
    editfield.setSelectionRange(-1, -1);
    editfield.setFontSize(tempoInput.size.y);
    parentCtrl->focusGui(&editfield);
}
void gui_tempocontrol_input::onKeyInputChangeValue(ivec2 direction) {
    auto const daw = dawCtrl->getDaw();
    int tempo = daw->getCurrentTempo();
    daw->setTempo(tempo + direction.y);
}
bool gui_tempocontrol_input::handleKeyInput(KeyEvent& kevt) {
    bool handled = false;
    if (kevt.type != K_RELEASE) {
        if (isArrowKey(kevt.keyCode)) {
            ivec2 dir;
            arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
            if (dir.y) {
                if ((kevt.mods & KB_MOD_SHIFT)) {
                    dir *= 12;
                }
                onKeyInputChangeValue(dir);
                handled = true;
            }
        }
    }
    if (!handled) {
        if (kevt.type != K_RELEASE && isNumericInput(kevt.keyCode)) {
            parentInput->buttonClicked(this);
        }
    }
    return handled;
}
bool guibutton_audioengine::getState() const {
    // TODO: get rid of getInstance call (required in settings dialog window)
    return DAW::Host::getInstance()->isStreaming();
}
void gui_signaturecontrol_input::render(NVGcontext* vg) {
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
void gui_signaturecontrol_input::handleDraggedMove(MouseEvent& evt) {
    if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
        auto daw  = dawCtrl->getDaw();
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
void gui_signaturecontrol_input::handleDraggedBegin(MouseEvent& evt) {
    if (evt.guiDragged == this) {
        parentCtrl->captureMouse(this);
    }
}
gui_tempocontrol::gui_tempocontrol()
    : tempoInput(this) {
    padding = 0;
    add(&tempoInput);
    setCanMouseHit(true);
    editfield.setFlag(FLG_NO_LAYOUT, true);
    editfield.setVisible(false);
    editfield.setAlignment(gui_textfield::Alignment::Center);
    editfield.setReturnCommits(true);
    add(&editfield);
}
void gui_tempocontrol_input::render(NVGcontext* vg) {
    renderWidgetBorder(vg, getStateFlags());
    String tempo = FormatTempo(dawCtrl->getDaw()->getCurrentTempoBPM());
    setFont(vg, G_FONT_SCALE(size.y), THEMECOL_TEXT, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(tempo), NULL);
}
void gui_tempocontrol_input::handleDraggedBegin(MouseEvent& evt) {
    if (isCtrl(evt.kbmods) || (evt.type == MouseEventType::M_EVT_DOUBLECLICK)) {
        if (parent) parent->buttonClicked(this);
        return;
    }
    if (evt.guiDragged == this) {
        parentCtrl->captureMouse(this);
    }
}
void gui_tempocontrol_input::handleDraggedMove(MouseEvent& evt) {
    if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
        int disty = (int) evt.dragDistance->y / 10;
        if (math::abs(disty) < 1)
            return;
        evt.dragDistance->y = 0;
        onKeyInputChangeValue(ivec2{ 0, -disty * 100 });
    }
}
void gui_signaturecontrol::layout() {
    inputNum.size  = ivec2(30, size.y);
    inputDen.size  = ivec2(30, size.y);
    inputNum.pos.x = (size.x / 4) - inputNum.size.x / 2;
    inputDen.pos.x = (size.x / 4) * 3 - inputNum.size.x / 2;
}
void gui_signaturecontrol::render(NVGcontext* vg) {
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
guictr_tempocontrols::guictr_tempocontrols(project_t& _project, project_globals_t& _projectGlobals)
    : guictr_base(),
      projectGlobals(_projectGlobals),
      cursorPos(&projectGlobals.cursor.cursorPos),
      songPos(&projectGlobals.playbackPos),
      loopPos(&projectGlobals.loopStart),
      loopLen(&projectGlobals.loopLen, true),
      zoom(&globalZoom) {
    zoom.fnValueEditChanged = [this](gui_numberinput_field_base*, GlobalZoom globalZoom) {
        globalZoom.zoom = math::clamp(globalZoom.zoom, 0.25f, 4.0f);
        if (parentCtrl) {
            parentCtrl->m_scale = globalZoom.zoom;
            parentCtrl->relayout();
        }
    };
    zoom.setLabel("Zoom");
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
    add(&zoom);
    add(&btnAudioOnOff);
    padding = 8;
}
guictr_tempocontrols::~guictr_tempocontrols() {
    remove(&btnAudioOnOff);
    remove(&zoom);
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
template<>
String gui_numberinput_field_generic<GlobalZoom>::valueToStringLiteral(GlobalZoom val) {
    return StringFormat(strFormat ? strFormat : "%.3f", val.zoom);
}
template<>
GlobalZoom gui_numberinput_field_generic<GlobalZoom>::parseLiteral(const char* szNumber) {
    return GlobalZoom{float(atof(szNumber))};
}
template<>
void gui_numberinput_field_generic<GlobalZoom>::onMouseDragValue(int32_t disty, int32_t absy) {
    if (this->number) {
        setValue(GlobalZoom{number->zoom - (disty) * 0.125f});
    }
}
template<>
void gui_numberinput_field_generic<GlobalZoom>::onKeyInputChangeValue(ivec2 direction) {
    if (this->number) {
        setValue(GlobalZoom{number->zoom - (-direction.y) * 0.125f});
    }
}
void guictr_tempocontrols::layout() {
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
    zoom.size          = ivec2(90, 28);
    zoom.pos = btnAudioOnOff.pos - ivec2(zoom.size.x+spacingCtrls, 0);
    for (guibase* gui : guis) {
        gui->layout();
    }
}
void guictr_tempocontrols::render(NVGcontext* vg) {
    //guictr_base::setScissorTransform(vg);
    ivec2 posInset = getPosContent();
    nvgTranslate(vg, posInset.x, posInset.y);
    for (guibase* gui : guis) {
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }
}
void gui_timeinput_field::setNewValue(int32_t val) {
    *time = parentInput->clampValue(val);
}
int32_t gui_timeinput::clampValue(int32_t val) {
    if (!bCanGoNegative) {
        return math::max(0, val);
    }
    return val;
}
