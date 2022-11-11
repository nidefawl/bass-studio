#pragma once

#include "config.h"
#include "math/seq_math.h"
#include "gui/gui.h"
#include "theme.h"
#include "dsp_util.h"
#include "host/meter/meter.h"

void renderMeterAt(NVGcontext* vg, guitheme_t* theme, const ivec2& pos, const ivec2& size, DAW::rmsmeter* meter);
void renderMeterHorizontal(NVGcontext *vg, guitheme_t *theme, const vec2 &pos, const vec2 &size, DAW::rmsmeter *meter);

class gui_trackmeter : public guibase {
    DAW::rmsmeter* const meter;
public:
    gui_trackmeter(DAW::rmsmeter* _meter)
        : meter(_meter)
    {
    }
    void render(NVGcontext* vg) override;
};
