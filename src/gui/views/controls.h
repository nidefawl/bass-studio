#pragma once
#include <memory>
#include <vector>
#include "gui/controls/inputfield.h"
#include "host/project/project.h"
#include "host/track/track.h"
#include "types.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "color_util.h"
#include "renderresources.h"
#include "platform.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/knob.h"
#include "gui/controls/textfield.h"
#include "gui/gui.h"
#include "util/profiling.h"

class gui_tempocontrol;
class gui_tempocontrol_input : public guibutton {
    gui_tempocontrol* const parentInput;
public:
    gui_tempocontrol_input(gui_tempocontrol* parent)
        : guibutton(), parentInput(parent) {
        setFlag(FLG_RENDER_BACKGROUND_INSET, true);
        setFlag(FLG_BG_SHADING, true);
        setLabel("Tempo");
    }
    void render(NVGcontext* vg) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    void handleDraggedRelease(MouseEvent& evt) override {
    }
    void onKeyInputChangeValue(ivec2 direction);
};
class gui_tempocontrol : public guictr_base {
    gui_tempocontrol_input tempoInput;
    gui_textfield editfield;
public:
    gui_tempocontrol();
    ~gui_tempocontrol() override {
        removeGuis();
        editfield.setLabel("Tempo");
    }
    void layout() override {
        tempoInput.size = size;
    }
    void buttonClicked(guibase* button) override;
    void onInputChanged(const gui_tempocontrol_input* input);
    void showEditField();
};

class gui_signaturecontrol_input : public guibutton {
    const int idx;

public:
    gui_signaturecontrol_input(int _idx)
        : guibutton(),
          idx(_idx) {
        setFlag(FLG_RENDER_BACKGROUND_INSET, true);
        setFlag(FLG_BG_SHADING, true);
    }

    void render(NVGcontext* vg) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
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
        inputNum.setLabel("Signature Numerator");
        inputDen.setLabel("Signature Denominator");
    }
    ~gui_signaturecontrol() override {
        removeGuis();
    }

    bool enabled() {
        return true;
    }
    void layout() override;
    void render(NVGcontext* vg) override;
};

class gui_timeinput;
class gui_timeinput_field : public guibutton {
public:
    const int idx;
    const bool isRelative;
private:
    gui_timeinput* const parentInput;
    int32_t* time;
public:
    gui_timeinput_field(gui_timeinput* parentInput, int _idx, int32_t* _time, const bool _isRelative);
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
    void onKeyInputChangeValue(ivec2 direction);
    void setNewValue(int32_t val);
    bool handleKeyInput(KeyEvent& kevt) override;
};
class gui_timeinput : public guictr_base {
    int32_t* time = nullptr;
    gui_timeinput_field bar;
    gui_timeinput_field beat;
    gui_timeinput_field sixteenths;
    gui_textfield editfield;
    bool bCanGoNegative = false;
public:
    gui_timeinput(int32_t* _time, const bool isRelative = false);
    ~gui_timeinput() override {
        removeGuis();
    }
    void setCanGoNegative(const bool b) {
        bCanGoNegative = b;
    }
    int32_t clampValue(int32_t val);
    void setRef(int32_t* time);
    void setConnectedBG();
    void layout() override;
    void buttonClicked(guibase* button) override;
    void render(NVGcontext* vg) override;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
    int32_t getTime();
    void onInputChanged(const gui_timeinput_field* input);
    void showEditField();
};

class guibutton_audioengine : public guibuttonstate {
    host_stats_t stats;
    float cpuUsage = 0.0f;

public:
    guibutton_audioengine() = default;
    bool getState() const override;
    void render(NVGcontext* vg) override;
    void prerender(NVGcontext* vg) override;
    void renderWidgetBorderPosSize(NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size) const override;

};
struct GlobalZoom {
    float zoom = 1.0f;
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
    gui_numberinput_field_generic<GlobalZoom> zoom;
    GlobalZoom globalZoom;
    guibuttonstate btnUiLayoutLock;
public:
    guictr_tempocontrols(project_t& _project, project_globals_t& _projectGlobals);
    ~guictr_tempocontrols() override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void buttonClicked(guibase* button) override;
    void onAdded() override {
        guictr_base::onAdded();
        onGlobalZoomChanged();
    }
    void onGlobalZoomChanged() {
        if (parentCtrl)
            globalZoom.zoom = parentCtrl->m_scale;
    }
};
