#pragma once

#include "config.h"
#include "math/seq_math.h"
#include "gui.h"
#include "theme.h"
#include "dsp_util.h"
#include "meter.h"

template<uint32_t T, uint32_t C = 2>
class gui_trackmeter : public guibase {
public:
	rmsmeter<T>* const meter;
	rmsmeterimpl<T, C>* const meterImpl;
	gui_trackmeter(rmsmeter<T>* _meter) :
		guibase(), meter(_meter), meterImpl(nullptr) {
	}
	gui_trackmeter(rmsmeterimpl<T, C>* _meterImpl) :
		guibase(), meter(nullptr), meterImpl(_meterImpl) {
	}
	void render(NVGcontext* vg);
};
