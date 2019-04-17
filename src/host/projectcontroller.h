#pragma once
#include "track.h"
class project_controller_t : public project_t {
public:
	virtual ~project_controller_t() {

	}
	float getCurrentTempoBPM() {
		return tempo100 / 100.0f;
	}
	int32_t getCurrentTempo() {
		return tempo100;
	}
	virtual void setTempo(int32_t _tempo100) {
		this->tempo100 = _tempo100;
	}
	tick_t& getPlaybackPos() {
		return playbackPos;
	}
	uint32_t sigNum() {
		return signatureNum;
	}
	uint32_t sigDen() {
		return 1<<signatureDenom;
	}
	uint32_t sigDenExp() {
		return signatureDenom;
	}
	void setNum(uint32_t n) {
		this->signatureNum = CLAMP_I(n, 1, 32);
	}
	void setDen(uint32_t d) {
		for (int i = 0; i <= 4; i++) {
			if (d < (1u << (i + 1u))) {
				this->signatureDenom = i;
				return;
			}
		}
	}
	int32_t tickToSamples(tick_t ticks);
	tick_t samplesToTicks(int32_t sample);

};
