#pragma once
#include <vector>
#include "math/vec.h"
#include "event.h"
#include "gui/gui.h"
#include "contextmenu_base.h"
#include "contextmenu.h"
#include "contextmenu_color.h"
#include "gui/track/trackctr.h"
#include "track.h"
#include "track_impl.h"
#include "guicolors.h"

class ctxtmenu_time_select;
class guictxtmenu_track_editor : public guictxtmenu {
protected:
    track_gui_entry_t* const m_trackentry;
    gui_clip* const m_gclip;
    ctxtmenu_color_select* sel = nullptr;
    ctxtmenu_time_select* timeSel1 = nullptr;
    ctxtmenu_time_select* timeSel2 = nullptr;
public:
    guictxtmenu_track_editor(DawCtrl* const _dawCtrl, track_gui_entry_t* const _trackentry, gui_clip* optionalContextClip);

    bool clickedElement(ctxtmenu_entry* e, int _id) override;
};
class guictxtmenu_clip : public guictxtmenu_track_editor {
public:
    explicit guictxtmenu_clip(DawCtrl* const _dawCtrl, gui_clip* const _gclip);
    bool clickedElement(ctxtmenu_entry* e, int _id) override;
};
class guictxtmenu_notrack : public guictxtmenu {
private:
    int idxImport;

public:
    guictxtmenu_notrack() {
        this->size.x = 190;

        int i = 0;
        for (; i < NUM_TRACK_TYPES; i++) {
            addEntry(new ctxtmenu_entry(StringFormat("Insert %s Track", TrackTypeToName(i)), i));
        }
        idxImport = i;
        addEntry(new ctxtmenu_entry("Import Track", i++));
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override;
};


class guictxtmenu_at_param : public guictxtmenu {
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