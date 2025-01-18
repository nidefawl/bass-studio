#pragma once
#include <memory>
#include <vector>
#include "gui/controls/inputfield.h"
#include "guicolors.h"
#include "host/project/project.h"
#include "host/track/track.h"
#include "saferef.h"
#include "seq_util.h"
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
class gui_tempocontrol_input final : public guibutton {
    gui_tempocontrol* const parentInput;
    int32_t dragBeginTempo = 0;
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
    void handleDraggedRelease(MouseEvent& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    void onKeyInputChangeValue(ivec2 direction);
};
class gui_tempocontrol final : public guictr_base {
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
    void showEditField();
};

class gui_signaturecontrol_input final : public guibutton {
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
class gui_signaturecontrol final : public guictr_base {
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
class gui_timeinput_field final : public guibutton {
public:
    const int idx;
    const bool isRelative;
private:
    gui_timeinput* const parentInput;
public:
    gui_timeinput_field(gui_timeinput* parentInput, int _idx, const bool _isRelative);
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
class gui_timeinput final : public guictr_base {
    SafeRef<guibase> ref;
    int32_t* refPtr = nullptr; //TODO: make this a safe reference
    gui_timeinput_field bar;
    gui_timeinput_field beat;
    gui_timeinput_field sixteenths;
    gui_textfield editfield;
    bool bCanGoNegative = false;
public:
    explicit gui_timeinput(const bool isRelative = false);
    ~gui_timeinput() override {
        removeGuis();
    }
    int32_t* getSafeIntRef() {
        auto ptr = safeRefGet(ref);
        if (ptr && refPtr) {
            return refPtr;
        }
        return nullptr;
    }
    void setCanGoNegative(const bool b) {
        bCanGoNegative = b;
    }
    int32_t clampValue(int32_t val);
    void setRef(SafeRef<guibase> ref, int32_t* time);
    void clearRef();
    void setConnectedBG();
    void layout() override;
    void buttonClicked(guibase* button) override;
    void render(NVGcontext* vg) override;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
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
    void onTick(AppCtrl* ctrl) override;
};

class guibutton_audioengine_lowlatency final : public guibutton_audioengine {
public:
    guibutton_audioengine_lowlatency() = default;
    bool getState() const override;
    void render(NVGcontext* vg) override;
    void onTick(AppCtrl* ctrl) override;
};

struct GlobalZoom {
    float zoom = 1.0f;
};
class guibutton_select : public guibutton {
public:
    int32_t btnIndex = 0;
public:
    guibutton_select() = default;
    int32_t getIndex() const {
        return btnIndex;
    }
    void renderButtonLabel(NVGcontext* vg, int32_t stateFlags) override;
};
class guictr_daw_layout_select final : public guictr_base {
    class guibutton_layout_select : public guibutton_select {
    public:
        guibutton_layout_select() = default;
        bool getState() const override;
    };
    std::array<guibutton_layout_select, 4> btnViews;
public:
    guictr_daw_layout_select() {
        padding = 0;
        setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        for (auto& btn : btnViews) {
            btn.btnIndex = static_cast<int32_t>(&btn - btnViews.data());
            add(&btn);
            btn.setTooltipText("Load UI Layout " + std::to_string(btn.btnIndex + 1));
            btn.setText(std::to_string(btn.btnIndex + 1));
            btn.setButtonColor(GuiColor::COL_BTN_BG_SHOW_ACTIVE);
        }
    }
    ~guictr_daw_layout_select() override {
        removeGuis();
    }
    void layout() override {
        auto pos = ivec2(0, 0);
        for (auto& gui : btnViews) {
            gui.pos = pos;
            gui.size = ivec2(size.x / btnViews.size(), size.y);
        }
        guictr_base::layout();
    }
    int32_t getNumButtons() const {
        return CtrSize(btnViews);
    }
    void buttonClicked(guibase* button) override;
};
class guictr_daw_viewmode_select final : public guictr_base {
    class guibutton_viewmode_select : public guibutton_select {
    public:
        guibutton_viewmode_select() = default;
        bool getState() const override;
    };
    std::array<guibutton_viewmode_select, 3> btnViews;
public:
    guictr_daw_viewmode_select() {
        padding = 0;
        setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        btnViews[0].setTooltipText("Show Tracks");
        btnViews[1].setTooltipText("Show Mixers");
        btnViews[2].setTooltipText("Show Nodes");
        btnViews[0].setText("T");
        btnViews[1].setText("M");
        btnViews[2].setText("N");
        for (auto& btn : btnViews) {
            btn.btnIndex = static_cast<int32_t>(&btn - btnViews.data());
            add(&btn);
            btn.setButtonColor(GuiColor::COL_BTN_BG_SHOW_ACTIVE);
        }
    }
    ~guictr_daw_viewmode_select() override {
        removeGuis();
    }
    void layout() override {
        auto pos = ivec2(0, 0);
        for (auto& gui : btnViews) {
            gui.pos = pos;
            gui.size = ivec2(size.x / btnViews.size(), size.y);
        }
        guictr_base::layout();
    }
    int32_t getNumButtons() const {
        return CtrSize(btnViews);
    }
    void buttonClicked(guibase* button) override;
};
class guictr_daw_controls final : public guictr_base {
    project_globals_t& projectGlobals;
    gui_tempocontrol tempo;
    gui_signaturecontrol signature;
    gui_timeinput cursorPos;
    gui_timeinput songPos;
    guibutton_audioengine btnAudioOnOff;
    guibutton_audioengine_lowlatency btnLowLatency;
    guibuttonstate btnRecord;
    guibuttonstate btnPlay;
    guibuttonstate btnStop;
    guibuttonstate btnLoop;
    gui_timeinput loopPos;
    gui_timeinput loopLen;
    gui_numberinput_field_generic<GlobalZoom> zoom;
    GlobalZoom globalZoom;
    guibuttonstate btnUiLayoutLock;
    guictr_daw_layout_select layoutSelect;
    guictr_daw_viewmode_select viewSelect;
    class guictr_controls_group : public guictr_base {
    public:
        guictr_controls_group();
        ~guictr_controls_group() override {
            removeGuis();
        }
    };
    guictr_controls_group ctrLeft;
    guictr_controls_group ctrCenter;
    guictr_controls_group ctrRight;
public:
    guictr_daw_controls(project_t& _project, project_globals_t& _projectGlobals);
    ~guictr_daw_controls() override;
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
