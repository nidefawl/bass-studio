#include "guimeter.h"
#include "guimeter_render.h"


template<>
void gui_trackmeter<16000, 1>::render(NVGcontext* vg) {
    if (meter)
        renderMeterAt<1>(vg, theme, pos, size, meter);
    else
        renderMeterAt<1>(vg, theme, pos, size, meterImpl);
}

template<>
void gui_trackmeter<16000, 2>::render(NVGcontext* vg) {
    if (meter)
        renderMeterAt<2>(vg, theme, pos, size, meter);
    else
        renderMeterAt<2>(vg, theme, pos, size, meterImpl);
}

template<>
void gui_trackmeter<16000, 4>::render(NVGcontext* vg) {
    if (meter)
        renderMeterAt<4>(vg, theme, pos, size, meter);
    else
        renderMeterAt<4>(vg, theme, pos, size, meterImpl);
}

template<>
void gui_trackmeter<16000, 6>::render(NVGcontext* vg) {
    if (meter)
        renderMeterAt<6>(vg, theme, pos, size, meter);
    else
        renderMeterAt<6>(vg, theme, pos, size, meterImpl);
}
