#pragma once
#include <vector>
#include "math/vec.h"
#include "event.h"
#include "gui/gui.h"
#include "contextmenu_base.h"
#include "contextmenu.h"
#include "contextmenu_color.h"
#include "gui/track/trackctr.h"
#include "host/plugin/modules.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "guicolors.h"

class ctxtmenu_time_select;
class guictxtmenu_track_editor : public guictxtmenu {
protected:
    guitrack_editor* const m_editor;
    track_gui_entry_t* const m_trackentry;
    ctxtmenu_color_select* sel = nullptr;
    ctxtmenu_time_select* timeSel1 = nullptr;
    ctxtmenu_time_select* timeSel2 = nullptr;
public:
    guictxtmenu_track_editor(guitrack_editor* const _editor, track_gui_entry_t* const _trackentry, gui_clip* optionalContextClip);

    bool clickedElement(ctxtmenu_entry* e, int _id) override;
};
class guictxtmenu_clip final : public guictxtmenu_track_editor {
public:
    explicit guictxtmenu_clip(guitrack_editor* const _editor, track_gui_entry_t* const _trackentry, gui_clip* const _gclip);
    bool clickedElement(ctxtmenu_entry* e, int _id) override;
};
class guictxtmenu_notrack final : public guictxtmenu {
public:
    explicit guictxtmenu_notrack(guictr_tracks* const _editor) {
        dawCtrl = _editor->dawCtrl;
        this->size.x = 190;
        addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_INSERT_MIDI_TRACK));
        addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_INSERT_AUDIO_TRACK));
        addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_INSERT_RETURN_TRACK));
        addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_INSERT_MASTER_TRACK));
        addEntry(new ctxtmenu_entry(dawCtrl, GlobalCommandType::CMD_IMPORT_TRACK));
    }
};


class guictxtmenu_at_param final : public guictxtmenu {
    automatable_t* const atl;
    int32_t const paramIdx;

public:
    guictxtmenu_at_param(DawCtrl* _dawCtrl, automatable_t* _atl, int32_t _paramIdx);
    bool clickedElement(ctxtmenu_entry* e, int _id) override;

    guictxtmenu* createPopupForEntry(ctxtmenu_entry* e, int lvl) override;
};


/* track io menus */
class ctxtmenu_entry_track_io : public ctxtmenu_entry {
public:
    ctxtmenu_entry_track_io(int32_t _id, const String& name) : ctxtmenu_entry(name, _id) {
    }
    ~ctxtmenu_entry_track_io() override = default;
    virtual bool isBus()                = 0;
    void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
        if (contains(ctxtSize, mouse)) {
            nvgBeginPath(vg);
            nvgRect(vg, 0, y, ctxtSize.x, height);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
            nvgFill(vg);
        }

        renderTextLabel(vg,
                        vec2(leftOffset(), y + height * 0.5f),
                        vec2(width-leftOffset(), height),
                        title,
                        theme,
                        fontSize,
                        THEMECOL_TEXT,
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }
};

class effectbase;
class guictr_plugins;
class guictxtmenu_plugin final : public guictxtmenu {
    effectbase* const effectOptional;
    guictr_plugins* const pluginCtrOptional;
public:
    const int CMD_SHOW_AUTOMATION = 1;
    const int CMD_SHOW_PARAM_LIST = 2;
    const int CMD_LOAD_PLUGIN = 3;
    guictxtmenu_plugin(DawCtrl* _dawCtrl, guictr_plugins* _ctrOptional, effectbase* _effectOptional);
    bool clickedElement(ctxtmenu_entry* e, int _id) override;
};