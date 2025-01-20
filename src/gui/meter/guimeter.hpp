#pragma once

#include "config.hpp"
#include "math/seq_math.hpp"
#include "gui/gui.hpp"
#include "theme.hpp"
#include "dsp_util.hpp"
#include "host/meter/meter.hpp"

void renderMeterAt(NVGcontext* vg, guitheme_t* theme, const ivec2& pos, const ivec2& size, DAW::rmsmeter* meter, textlabel_dynamic_t* label);
void renderMeterHorizontal(NVGcontext *vg, guitheme_t *theme, const vec2 &pos, const vec2 &size, DAW::rmsmeter *meter, textlabel_dynamic_t* label);

class gui_trackmeter final : public guibase {
    bool bRenderHorizontal = false;
    DAW::rmsmeter* const meter;
    textlabel_dynamic_t label;
public:
    gui_trackmeter(DAW::rmsmeter* _meter)
        : meter(_meter)
    {
    }
    void render(NVGcontext* vg) override;
    void setRenderHorizontal(bool b) { bRenderHorizontal = b; }
};
