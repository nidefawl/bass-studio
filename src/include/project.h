#pragma once
#include "seq_time.h"
#include "cursor.h"
#include "logging.h"

struct project_globals_i {

};
struct project_globals_t {
	tick_t loopStart = 0;
	tick_t loopLen = TICKS_BAR*1;
	bool loopEnabled = true;
	uint32_t tempo100 = 12800;
	uint32_t signatureNum = 4;
	uint32_t signatureDenom = 2;
	tick_t playbackPos = 0;
	DAW::Cursor cursor;
	DAW::TrackSelection trackSelection;
	bool recordArmed = false;
	void operator=(project_globals_t const & other) {
		tempo100 = other.tempo100;
		signatureNum = other.signatureNum;
		signatureDenom = other.signatureDenom;
		loopStart = other.loopStart;
		loopLen = other.loopLen;
		loopEnabled = other.loopEnabled;
		playbackPos = other.playbackPos;
		cursor = other.cursor;
		recordArmed = other.recordArmed;
	}
};
