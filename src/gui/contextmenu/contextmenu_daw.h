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



class guictxtmenu_clip : public guictxtmenu {
    ctxtmenu_color_select* sel;
    clip_t* const m_clip;

public:
    explicit guictxtmenu_clip(clip_t* const _clip) : m_clip(_clip) {
        this->size.x = 120;

        sel  = new ctxtmenu_color_select("Pick Color", 100);
        addEntry(sel);
    }
    void clicked(int _id) override {
        if (_id >= sel->id) {
            _id -= sel->id;
            int32_t col = colorPalette[_id];
            if (m_clip) {
                m_clip->rgb = col;
            }
        }
        closeContextMenu();
    }
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
    void clicked(int _id) override;
};


class guictxtmenu_at_param : public guictxtmenu {
    automatable_t* const atl;
    int32_t const paramIdx;

public:
    guictxtmenu_at_param(DawCtrl* _dawCtrl, automatable_t* _atl, int32_t _paramIdx);
    void clicked(int _id) override;
};
