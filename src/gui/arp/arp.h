#pragma once
#include "guicolors.h"
#include "guiconstant.h"
#include "event.h"
#include "math/seq_math.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/knob.h"
#include "gui/controls/textfield.h"
#include "host/midiarp.h"

class guiknob_arp;
class gui_arp : public guictr_base {
    gui_textfield editfield;
    guibuttontoggle buttonBypass;
    clip_view& clipview;
    std::array<guiknob_arp*, 6> knobs{};

public:
    DAW::midiarp* getArp();
    explicit gui_arp(clip_view& _clipview);
    void handleDraggedBegin(MouseEvent& evt) override;
    void buttonClicked(guibase* _button) override;
    void rightClicked(MouseEvent& evt, guibase* button) override;
    ~gui_arp() override;
    bool setScissorTransformContainer(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void showEditClip();
};
