#pragma once
#include "track.h"
#include "vst_host.h"

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
	beatbar16th_t toBeatBar16th(int32_t tick) {
		beatbar16th_t t;
		uint8_t denom = 4-signatureDenom;
		uint8_t num = signatureNum;
		tick = tick / TICKS_16TH;
		t.th = tick & ((1<<denom) - 1);
		int32_t quarters = (tick>>denom);
		t.beat = uint32_t(quarters) % num;
		t.bar = quarters / num;
		if (tick < 0) {
			t.bar -= 1;
		}
		return t;
	}

	static project_controller_t* get();
	double getProjectWorkingArea() {
		return 1000.0;
	}
	virtual inline void addTrackImpl(int32_t trackInsertPos, track_t* newTrack, int flags) {
		trackList.addTrack(trackInsertPos, newTrack);
		if ((flags&FLG_TRK_CHANGE_HISTORY_UNDO) != 0) {
			dbgassert(newTrack->audio);
		} else {
			dbgassert(!newTrack->audio);
			vsthost* host = vsthost::getInstance();
			host->createAudio(newTrack);
		}
	}
};
