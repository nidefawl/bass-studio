#pragma once
#include "seq_time.h"
#include "cursor.h"
#include "logging.h"

struct export_settings_t {
    tick_t exportPos  = 0;
    tick_t exportLen  = 0;
    String exportPath = "";
    bool isLocked     = false;
};

struct project_globals_t {
    /** (synced but not mutex locked) */
    tick_t loopStart = 0;
    /** (synced but not mutex locked) */
    tick_t loopLen = TICKS_BAR * 1;
    /** (synced but not mutex locked) */
    bool loopEnabled = true;
    /**
	 * (synced with threadsafe task)
	 * tempo100 = bpm*100
	 * Not automatable.
	 *
	 * Changes to tempo happen in task sent to playthread.
	 * This is applied between processing of blocks
	 * TODO: validate positioning of audio processing and note processing after tempo changes
	 **/
    uint32_t tempo100 = 12800;
    /** (synced but not mutex locked) Does not affect audio processing on host side, but time info structure sent to plugins */
    uint32_t signatureNum = 4;
    /** (synced but not mutex locked) Does not affect audio processing on host side, but time info structure sent to plugins */
    uint32_t signatureDenom = 2;
    /** (synced but not mutex locked) Read only parameter on UI/controller side. Write only on audio processing side */
    tick_t playbackPos = 0;
    /** (synced but not mutex locked)  */
    DAW::Cursor cursor;
    /** (synced but not mutex locked)  */
    DAW::TrackSelection trackSelection;
    /** (synced but not mutex locked)  */
    bool recordArmed = false;
    project_globals_t& operator    =(project_globals_t const& other) {
        tempo100       = other.tempo100;
        signatureNum   = other.signatureNum;
        signatureDenom = other.signatureDenom;
        loopStart      = other.loopStart;
        loopLen        = other.loopLen;
        loopEnabled    = other.loopEnabled;
        playbackPos    = other.playbackPos;
        cursor         = other.cursor;
        recordArmed    = other.recordArmed;
        return *this;
    }
};
