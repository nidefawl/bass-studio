#pragma once
#include "math/seq_math.h"
#include "types.h"
#include "gui/container/container.h"
#include "host/track/track.h"
#include "host/track/trackctr_types.h"
#include "trackctr.h"
#include "host/daw/mainctrl.h"
#include "host/track/track_impl.h"
#include "gui/contextmenu/contextmenu_daw.h"

enum class DragModeTrack : uint8_t {
    DRAG_TRACK_NONE,
    DRAG_TRACK_RESIZE,
};

class gui_track_content_base : public guictr_base {
    scaled_grid& m_grid;
public:
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;
    explicit gui_track_content_base(track_gui_entry_t* _entry, scaled_grid& _grid);
    scaled_grid& getGrid() {
        return m_grid;
    }
};

class gui_track_subtrack_controls;
class gui_trackcontrols_title;

class gui_track_control final : public gui_track_content_base {
    gui_trackcontrols_title* title;
    guictr_base* mixer;
    guictr_base* io;
    std::vector<gui_track_subtrack_controls*> automationLaneControls;
    DragModeTrack dragMode = DragModeTrack::DRAG_TRACK_NONE;

public:
    explicit gui_track_control(track_gui_entry_t* _entry, scaled_grid& _grid);
    ~gui_track_control() override;
    void addSubtrackMixer(track_gui_entry_t* entry, gui_track_subtrack* al);
    void removeSubtrackMixer(gui_track_subtrack* al);
    void removeAllAutomationLanes(automatable_t* at, int32_t paramIdx);
    void removeAllAutomationLanes(automatable_t* at);
    void removeAllSubtracks();
    void render(NVGcontext* vg) override;
    void renderGroupHandle(NVGcontext* vg);
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    void handleRightClick(MouseEvent& evt) override;
    bool isResize(ivec2 mpos);
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void layout() override;
    guibase* getTitle();
    String getLabel() const override;
    void handleDragDropHover(MouseHitEvt& mouseHit) override;
};

class gui_slider_gain : public gui_slider_textfield {
public:
    gui_slider_gain() : gui_slider_textfield() {
    }
    float getRenderScaledValue(float param) override {
        float gainDb = dsp_util::linScaleToGain(param);
        float fParamScaled = (gainDb - dsp_util::GAIN_DBFLOOR) / (dsp_util::GAIN_DB6 - dsp_util::GAIN_DBFLOOR);
        if (fParamScaled <= 0) {
            fParamScaled = 0;
        } else {
            fParamScaled = pow(fParamScaled, 1 / 3.0f);
        }
        return fParamScaled;
    }
    bool renderAsBipolar() override {
        return false;
    }
    float modifyParam(float param, float amt, bool applyUserInputScaling) override {
        float fGain = dsp_util::linScaleToGain(param);
        if (fGain < dsp_util::GAIN_DBFLOOR) {
            fGain = dsp_util::GAIN_DBFLOOR;
        }
        float dbfs  = dsp_util::dBFS(fGain);
        float delta = 1.0f;
        if (applyUserInputScaling) {
            for (int i = 1; i < 4; i++) {
                if (dbfs < -12 * i) {
                    delta *= 2;
                }
            }
        }
        dbfs -= delta * amt;
        float f    = dsp_util::fromdBFS(dbfs);
        float fNew = dsp_util::clampGain(f);
        return dsp_util::gainToLinScale(fNew);
    }
    float parseTextValue(const String& str) override {
        float fTextFieldVal = atof(StringAsCStr(str));
        float fGain         = dsp_util::fromdBFSClampInf6(fTextFieldVal);
        if (fGain < dsp_util::GAIN_DBFLOOR) {
            fGain = dsp_util::GAIN_DBFLOOR;
        }
        float fNew = dsp_util::clampGain(fGain);
        return dsp_util::gainToLinScale(fNew);
    }

    String getValueAsString(float param) override {
        float gain = dsp_util::linScaleToGain(param);
        if (gain < dsp_util::GAIN_DBFLOOR) return "-inf";
        float f = math::min(6.0f, 20.0f * std::log10(gain));
        return StringFormat("%.2f", f);
    }
};
class gui_slider_gain_vertical : public gui_slider_gain {
    track_t* const m_track;
public:
    explicit gui_slider_gain_vertical(track_gui_entry_t* _entry)
        : gui_slider_gain(),
        m_track(_entry->track)
    {
        setRenderVerticalSlider(true);
    }

    void layout() override;

    String getLabel() const override {
        return m_track->name;
    }
};
class gui_slider_pan final : public gui_slider_textfield {
public:
    gui_slider_pan() : gui_slider_textfield() {
    }
    bool renderAsBipolar() override {
        return true;
    }
};

class guibutton_trackbypass final : public guibuttonstate {
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;

public:
    explicit guibutton_trackbypass(track_gui_entry_t* _entry)
        : guibuttonstate(),
            m_track(_entry->track),
            m_trackentry(_entry) {
        (void) m_trackentry;
    }
    bool trackenabled() const {
        return m_track->audio && m_track->audio->mixer.isEnabled();
    }
    bool getState() const override {
        return trackenabled();
    }
    void handleRightClick(MouseEvent& evt) override {
        parentCtrl->openContextMenu(new guictxtmenu_at_param(dawCtrl, &m_track->audio->mixer, PARAM_ENABLE), evt.mousepos);
    }
};

class guibutton_track_solo final : public guibuttonstate {
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;

public:
    explicit guibutton_track_solo(track_gui_entry_t* _entry)
        : guibuttonstate(),
            m_track(_entry->track),
            m_trackentry(_entry) {
        (void) m_trackentry;
        setText("S");
    }
    

    GuiColor::constant_t getBackgroundColorFromState(int32_t stateflags) const override {
        if ((m_track->audio->flags & audiostageflags_t::SOLO) != audiostageflags_t::NONE) {
            return GuiColor::COL_BTN_SOLO_BG_ENABLED;
        }
        if ((m_track->audio->flags & audiostageflags_t::SOLO_PARENT) != audiostageflags_t::NONE) {
            return GuiColor::COL_BTN_SOLO_BG_PARENT;
        }
        return guibuttonstate::getBackgroundColorFromState(stateflags);
    }
    bool getState() const override {
        if (m_track->audio) {
            return (m_track->audio->flags & (audiostageflags_t::SOLO | audiostageflags_t::SOLO_PARENT)) != audiostageflags_t::NONE;
        }
        return false;
    }
    void handleRightClick(MouseEvent& evt) override {
    }
};

class guibutton_track_record_arm final : public guibuttonstate {
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;

public:
    explicit guibutton_track_record_arm(track_gui_entry_t* _entry)
        : guibuttonstate(),
            m_track(_entry->track),
            m_trackentry(_entry) {
        (void) m_trackentry;
        drawFn = drawRecordSymbol;
        setLabel("Record");
    }
    

    GuiColor::constant_t getBackgroundColorFromState(int32_t stateflags) const override {
        if ((m_track->audio->flags & audiostageflags_t::RECORD_ARMED) != audiostageflags_t::NONE) {
            return GuiColor::COL_BTN_RECORD_ARM_BG;
        }
        return guibuttonstate::getBackgroundColorFromState(stateflags);
    }
    bool getState() const override {
        if (m_track->audio) {
            return (m_track->audio->flags & (audiostageflags_t::RECORD_ARMED)) != audiostageflags_t::NONE;
        }
        return false;
    }
    void handleRightClick(MouseEvent& evt) override {
    }
};

class guictxtmenu_track final : public guictxtmenu {
    track_gui_entry_t* const m_trackentry;
    ctxtmenu_entry* cmdPickColor;
    ctxtmenu_entry* cmdDuplicateTrack;
    ctxtmenu_entry* cmdRenameTrack;
    ctxtmenu_entry* cmdShowAllAutomation;
    ctxtmenu_entry* cmdReactivateAutomation;
    ctxtmenu_entry* cmdShowWaveform;
    ctxtmenu_entry* cmdAddChildMidiTrack;
    ctxtmenu_entry* cmdDeleteTrack;
public:
    guictxtmenu_track(DawCtrl* _dawCtrl, track_gui_entry_t* const trackentry);
    ~guictxtmenu_track() override = default;
    bool clickedElement(ctxtmenu_entry* e, int _id) override;
};
