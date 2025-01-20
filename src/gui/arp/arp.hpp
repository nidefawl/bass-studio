#pragma once
#include "guicolors.hpp"
#include "guiconstant.hpp"
#include "event.hpp"
#include "math/seq_math.hpp"
#include "gui/container/container.hpp"
#include "gui/controls/button.hpp"
#include "gui/controls/knob.hpp"
#include "gui/controls/textfield.hpp"
#include "host/midiarp/midiarp.hpp"

class guiknob_arp;
class gui_arp final : public guictr_base {
    gui_textfield editfield;
    guibuttontoggle buttonBypass;
    clip_view_t& clipview;
    std::array<guiknob_arp*, 6> knobs{};

public:
    DAW::midiarp* getArp();
    const DAW::midiarp* getArp() const;
    explicit gui_arp(clip_view_t& _clipview);
    void handleDraggedBegin(MouseEvent& evt) override;
    void buttonClicked(guibase* _button) override;
    void rightClicked(MouseEvent& evt, guibase* button) override;
    ~gui_arp() override;
    bool setScissorTransformContainer(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void updateClipViewReferences();
};
