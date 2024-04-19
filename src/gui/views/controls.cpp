#include "controls.h"

#include "gui/dialog/dialog_io.h"
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
#include "host/audiohost/audio_host.h"
#include "basectrl.h"
#include "host/daw/mainctrl.h"
#include "appsettings.h"
#include <nanovg.h>


using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
using Table::tblString;

template<>
void guitooltip<gui_timeinput>::setContent() {
    auto ptr = getInstanceOrNull();
    if (!ptr) {
        return;
    }
    auto tmRef = ptr->getSafeIntRef();
    if (!tmRef) {
        return;
    }
    table.tableWidth = 60;
    auto cell = tblString{ptr->getTooltipText()};
    auto cell2 = tblString{StringFormat("Tick %d", *tmRef)};
    if (table.strW) {
        table.tableWidth = math::max(table.tableWidth, table.strW->getStringWidth(cell.str));
        table.tableWidth = math::max(table.tableWidth, table.strW->getStringWidth(cell2.str));
    }
    table.rows.push_back({{std::move(cell)}});
    table.rows.push_back({{std::move(cell2)}});
}
guictxtmenu_base* gui_timeinput::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<gui_timeinput>(this);
    return tooltip;
}
gui_timeinput_field::gui_timeinput_field(gui_timeinput* parentInput, int _idx, const bool _isRelative)
    : guibutton(), idx(_idx), isRelative(_isRelative), parentInput(parentInput) {
    setFlag(FLG_RENDER_BACKGROUND_INSET, true);
    setFlag(FLG_BG_SHADING, true);
}

void gui_timeinput_field::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    int32_t flags = getStateFlags();
    renderWidgetBorder(vg, flags);
    setFont(vg, G_FONT_SCALE(size.y), THEMECOL_TEXT, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    auto time = parentInput->getSafeIntRef();
    int32_t _time      = time ? *time : 0;
    beatbar16th_t step =  dawCtrl->getDaw()->toBeatBar16th(_time, isRelative);
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
    auto time = parentInput->getSafeIntRef();
    if (time && (isCtrl(evt.kbmods) || (evt.type == MouseEventType::M_EVT_DOUBLECLICK))) { 
        if (parent) parent->buttonClicked(this);
        return;
    }
    if (evt.guiDragged == this) {
        parentCtrl->captureMouse(this);
    }
}

void gui_timeinput_field::handleDraggedMove(MouseEvent& evt) {
    auto time = parentInput->getSafeIntRef();
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
    auto time = parentInput->getSafeIntRef();
    if (!time)
        return;
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

gui_timeinput::gui_timeinput(const bool isRelative)
    : guictr_base(),
    bar(this, 0, isRelative),
    beat(this, 1, isRelative),
    sixteenths(this, 2, isRelative)
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

void gui_timeinput::setRef(SafeRef<guibase> ref, int32_t* time) {
    this->ref  = ref;
    this->refPtr = time;
}

void gui_timeinput::clearRef() {
    this->ref  = {};
    this->refPtr = nullptr;
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
    if (!isRenderableSizeAndContext(vg))
        return;
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
    auto p = getSafeIntRef();
    if (!p)
        return;
    const bool isRelative = bar.isRelative;
    auto daw = dawCtrl->getDaw();
    editfield.mCallbackEnd = [this, isRelative](const std::string& str) {
        auto p = getSafeIntRef();
        if (p) {
            auto daw = dawCtrl->getDaw();
            auto beatBarNth = stringToBeatBarNth(str, isRelative, daw->getGlobals().signatureNum, daw->getGlobals().signatureDenom);
            auto tick = daw->beatBarNthToTick(beatBarNth, isRelative);
            *p = tick;
            onInputChanged(&bar);
        }
        editfield.setVisible(false);
        return true;
    };
    editfield.pos  = {};
    editfield.size = size;
    editfield.setVisible(true);
    editfield.layout();
    if (p) {
        auto beatBarNth = daw->toBeatBar16th(*p, isRelative);
        log_lf(Log::L_DEBUG, "beatBarNthToString beg: %s\n", StringAsCStr(beatBarNthToString(beatBarNth, isRelative)));
        editfield.setValue(beatBarNthToString(beatBarNth, bar.isRelative));
        editfield.setSelectionRange(-1, -1);
    } else {
        editfield.setValue("");
    }
    editfield.setFontSize(bar.size.y);
    parentCtrl->focusGui(&editfield);
}

void gui_timeinput::buttonClicked(guibase* button) {
    auto field = dynamic_cast<gui_timeinput_field*>(button);
    if (field) {
        showEditField();
        return;
    }
    guictr_base::buttonClicked(button);
}


void guictr_daw_controls::buttonClicked(guibase* button) {
    auto daw = dawCtrl->getDaw();
    if (button == &this->btnPlay) {
        projectGlobals.recordArmed = false;
        daw->startPlaying();
    }
    if (button == &this->btnStop) {
        daw->stopPlaying();
        projectGlobals.recordArmed = false;
    }
    if (button == &this->btnRecord) {
        projectGlobals.recordArmed = true;
        daw->startPlaying();
    }
    if (button == &this->btnLoop) {
        projectGlobals.loopEnabled = !projectGlobals.loopEnabled;
    }
    if (button == &this->btnAudioOnOff) {
        auto& settings = daw_tls::getSettings();
        if (daw->getAudioHost()->isStreaming()) {
            settings.dawsettings.audioEnabled = false;
        } else {
            settings.dawsettings.audioEnabled = true;
        }
        if (!daw->configureSampleRate()) {
            settings.dawsettings.audioEnabled = false;
            daw->getMainControl()->openDialog(new DAW::DialogSettings::guidialog_settings(daw));
        } else {
            settings.dawsettings.audioEnabled = true;
        }
    }
    if (button == &this->btnUiLayoutLock) {
        auto& settings = daw_tls::getSettings();
        settings.dawsettings.uiLayoutLocked = !settings.dawsettings.uiLayoutLocked;
        dawCtrl->relayout();
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
    if (!isRenderableSizeAndContext(vg))
        return;
    if (!dawCtrl)
        return;
    int32_t fl = getStateFlags();
    renderWidgetBorder(vg, fl);
    if (size.y > 10 && size.x > 10) {
        nvgSave(vg);
        setScissorTransform(vg);
        auto fontSizeScaled = math::clamp(size.y, 4, 48) * FONT_AUTOSCALE;
        auto posText = vec2(0) + vec2(size.x - 3, size.y * 0.5f);
        float textWidth = renderTextLabel(vg,
                        posText,
                        vec2(size),
                        str,
                        theme,
                        fontSizeScaled,
                        theme->getColor(getLabelColor()),
                        NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        renderTextLabel(vg,
                        vec2(0) + vec2(3.0f, size.y * 0.5f),
                        vec2(size.x - textWidth - 6.0f, size.y),
                        label,
                        theme,
                        fontSizeScaled,
                        theme->getColor(GuiColor::COL_LABEL_INACTIVE),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgRestore(vg);
    }
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
    daw->updateVisibleTrackContents();
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
    if (!isRenderableSizeAndContext(vg))
        return;
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
    if (!isRenderableSizeAndContext(vg))
        return;
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
    if (!isRenderableSizeAndContext(vg))
        return;
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
guictr_daw_controls::guictr_daw_controls(project_t& _project, project_globals_t& _projectGlobals)
    : guictr_base(),
      projectGlobals(_projectGlobals),
      cursorPos(false),
      songPos(false),
      loopPos(false),
      loopLen(true),
      zoom(&globalZoom) {
    cursorPos.setRef(toRef(), &projectGlobals.cursor.cursorPos);
    songPos.setRef(toRef(), &projectGlobals.playbackPos);
    loopPos.setRef(toRef(), &projectGlobals.loopStart);
    loopLen.setRef(toRef(), &projectGlobals.loopLen);
    zoom.fnValueEditChanged = [this](gui_numberinput_field_base*, GlobalZoom globalZoom) {
        globalZoom.zoom = math::clamp(globalZoom.zoom, 0.5f, 2.0f);
        if (parentCtrl) {
            parentCtrl->updateZoomLevel(globalZoom.zoom);
            parentCtrl->relayout();
        }
    };
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
    btnUiLayoutLock.setStateRef(&daw_tls::getSettings().dawsettings.uiLayoutLocked);
    btnUiLayoutLock.drawFn   = drawTextureSymbol;
    btnUiLayoutLock.drawParm = ICON_OPT_LOCKED;
    btnRecord.setStateRef(&projectGlobals.recordArmed);
    btnRecord.setButtonColor(GuiColor::COL_BTN_RECORD_ARM_BG);
    add(&loopLen);
    add(&loopPos);
    add(&tempo);
    add(&signature);
    add(&cursorPos);
    add(&layoutSelect);
    add(&viewSelect);
    add(&btnLoop);
    add(&btnStop);
    add(&btnPlay);
    add(&btnRecord);
    add(&songPos);
    add(&btnUiLayoutLock);
    add(&zoom);
    add(&btnAudioOnOff);
    padding = 8;
    zoom.setLabel("Zoom");
    tempo.setLabel("Tempo");
    btnRecord.setLabel("Record (Arm)");
    btnPlay.setLabel("Play");
    btnStop.setLabel("Stop");
    btnLoop.setLabel("Loop");
    btnAudioOnOff.setLabel("Audio CPU Usage");
    signature.setLabel("Signature");
    cursorPos.setLabel("Cursor Position");
    songPos.setLabel("Playback Position");
    loopPos.setLabel("Loop Start Position");
    loopLen.setLabel("Loop Length");
    btnUiLayoutLock.setLabel("Lock UI Layout");
    layoutSelect.setLabel("Select View");
    viewSelect.setLabel("Switch between Tracks / Nodes View");
}
guictr_daw_controls::~guictr_daw_controls() {
    remove(&btnAudioOnOff);
    remove(&zoom);
    remove(&btnUiLayoutLock);
    remove(&songPos);
    remove(&btnRecord);
    remove(&btnPlay);
    remove(&btnStop);
    remove(&btnLoop);
    remove(&viewSelect);
    remove(&layoutSelect);
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
bool gui_numberinput_field_generic<GlobalZoom>::onMouseDragValue(int32_t disty, int32_t absy) {
    if (this->number) {
        setValue(GlobalZoom{number->zoom - (disty) * 0.125f});
    }
    return true;
}
template<>
void gui_numberinput_field_generic<GlobalZoom>::onKeyInputChangeValue(ivec2 direction) {
    if (this->number) {
        setValue(GlobalZoom{number->zoom - (-direction.y) * 0.125f});
    }
}
void guictr_daw_controls::layout() {
    ivec2 cs        = getSizeContent();
    int32_t verticalSpacing = 10;
    int32_t smallHeight = 28;
    int32_t bigHeight = 32;
    tempo.size      = ivec2(bigHeight * 3, smallHeight);
    signature.size  = ivec2(bigHeight * 3, smallHeight);
    cursorPos.size  = ivec2(bigHeight * 3, smallHeight);
    tempo.pos       = ivec2(verticalSpacing >> 1, (cs.y - tempo.size.y) / 2);
    signature.pos   = ivec2(tempo.right() + verticalSpacing, (cs.y - signature.size.y) / 2);
    cursorPos.pos   = ivec2(signature.right() + verticalSpacing, (cs.y - cursorPos.size.y) / 2);


    int32_t spacingCtrls = 5;
    btnRecord.size = btnLoop.size = btnStop.size = btnPlay.size = ivec2(bigHeight, bigHeight);

    btnLoop.size.x = bigHeight + (bigHeight >> 1);
    loopPos.size   = ivec2(bigHeight * 3, bigHeight);
    loopLen.size   = ivec2(bigHeight * 3, bigHeight);
    songPos.size   = ivec2(bigHeight * 4, bigHeight);

    int32_t transportWidth = btnPlay.size.x + spacingCtrls + btnStop.size.x + spacingCtrls + songPos.size.x;
    int32_t transportCtrls = math::max(cs.x / 2 - transportWidth / 2, cursorPos.right() + verticalSpacing);

    std::vector<guibase*> playbackControls{ &btnRecord, &btnPlay, &btnStop, &songPos };
    std::vector<guibase*> loopControls{ &btnLoop, &loopPos, &loopLen };
    int posX = transportCtrls;
    for (auto el : playbackControls) {
        el->pos = ivec2(posX, (cs.y - el->size.y) / 2);
        posX    = el->right() + spacingCtrls;
    }
    posX += spacingCtrls * 3;
    for (auto el : loopControls) {
        el->pos = ivec2(posX, (cs.y - el->size.y) / 2);
        posX    = el->right() + spacingCtrls;
    }

    btnAudioOnOff.size = ivec2(smallHeight*3, smallHeight);
    zoom.size          = ivec2(smallHeight*3, smallHeight);
    btnUiLayoutLock.size = ivec2(smallHeight, smallHeight);
    layoutSelect.size    = ivec2((smallHeight+5) * layoutSelect.getNumButtons(), smallHeight);
    viewSelect.size      = ivec2((smallHeight+5) * viewSelect.getNumButtons(), smallHeight);
    btnAudioOnOff.pos  = ivec2(math::max(songPos.right() + verticalSpacing, cs.x - 5 - btnAudioOnOff.size.x), (cs.y - btnAudioOnOff.size.y) / 2);
    zoom.pos = ivec2(btnAudioOnOff.left() - zoom.size.x - spacingCtrls, (cs.y - zoom.size.y) / 2);
    btnUiLayoutLock.pos = ivec2(zoom.left() - btnUiLayoutLock.size.x - spacingCtrls, (cs.y - btnUiLayoutLock.size.y) / 2);
    layoutSelect.pos = ivec2(btnUiLayoutLock.left() - layoutSelect.size.x - spacingCtrls, (cs.y - layoutSelect.size.y) / 2);
    viewSelect.pos = ivec2(layoutSelect.left() - viewSelect.size.x - spacingCtrls, (cs.y - viewSelect.size.y) / 2);

    for (guibase* gui : guis) {
        gui->layout();
    }
}
void guictr_daw_controls::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    //guictr_base::setScissorTransform(vg);

    bool bIsLockedUiLayout = daw_tls::getSettings().dawsettings.uiLayoutLocked;
    btnUiLayoutLock.drawParm = bIsLockedUiLayout ? ICON_OPT_LOCKED : ICON_OPT_UNLOCKED;
    ivec2 posInset = getPosContent();
    nvgTranslate(vg, posInset.x, posInset.y);
    for (guibase* gui : guis) {
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }
}
void gui_timeinput_field::setNewValue(int32_t val) {
    auto ptr = parentInput->getSafeIntRef();
    if (!ptr)
        return;
    *ptr = parentInput->clampValue(val);
}
int32_t gui_timeinput::clampValue(int32_t val) {
    if (!bCanGoNegative) {
        return math::max(0, val);
    }
    return val;
}
void guibutton_audioengine::onTick(AppCtrl* ctrl) {
    auto ahost = dawCtrl->getDaw()->getAudioHost();
    if (!ahost || !ahost->isStreaming()) {
        if (size.x > 100) {
            setText("Off");
            setLabel("Audio Enabled");
        } else {
            setText("Off");
            setLabel("Audio");
        }
        setTooltipText("Audio Engine is not running");
    } else {
        setText(StringFormat("%.0f%%", this->cpuUsage * 100.0));
        if (size.x > 100) {
            setLabel("Audio CPU");
        } else {
            setLabel("Audio");
        }
        setTooltipText("Audio Engine is running");
    }
}
void guictr_daw_viewmode_select::buttonClicked(guibase* button) {
    int32_t idx = 0;
    for (auto & btnView : btnViews) {
        if (button == &btnView) {
            auto temp = DAW::UI::CommandContext{GlobalCommandType::CMD_SWITCH_VIEW, {}, idx};
            temp.kevt.mods = parentCtrl->lastMouseEvent.kbmods;
            dawCtrl->handleGlobalCommand(temp);
            break;
        }
        idx++;
    }
}
void guictr_daw_layout_select::buttonClicked(guibase* button) {
    int32_t idx = 0;
    for (auto & btnView : btnViews) {
        if (button == &btnView) {
            auto temp = DAW::UI::CommandContext{GlobalCommandType::CMD_SWITCH_LAYOUT, {}, idx};
            temp.kevt.mods = parentCtrl->lastMouseEvent.kbmods;
            dawCtrl->handleGlobalCommand(temp);
            break;
        }
        idx++;
    }
}

void guibutton_select::renderButtonLabel(NVGcontext* vg, int32_t stateFlags) {
    if ((drawFn || str.length()) && size.y > 10 && size.x > 10) {
        nvgSave(vg);
        setScissorTransform(vg);
        ivec2 renderFrame = size;
        ivec2 renderPos(0);
        if (str.length() > 0) {
            auto fontScale = math::clamp(math::min(size.y, size.x), 4, 48) * FONT_AUTOSCALE;
            renderCenteredMultilineText(vg, theme, str, fontScale, getLabelColor(), renderPos, renderFrame);
        }
        drawSquareInset(vg, renderPos, renderFrame, theme->getColor(getBackgroundColor()), 0, 0);
        nvgRestore(vg);
    }
}

bool guictr_daw_layout_select::guibutton_layout_select::getState() const {
    return dawCtrl->getLayoutIndex() == this->btnIndex;
}

bool guictr_daw_viewmode_select::guibutton_viewmode_select::getState() const {
    return dawCtrl->getViewMode() == static_cast<view_mode_t>(this->btnIndex);
}
