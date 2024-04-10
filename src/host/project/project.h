#pragma once
#include "seq_time.h"
#include "cursor.h"
#include <vector>

enum ProjectFileType {
    PROJECT_FILETYPE_JSON,
    PROJECT_FILETYPE_BUNDLE,
};

struct export_settings_t {
    tick_t exportPos  = 0;
    tick_t exportLen  = 0;
    String exportPath = "";
    bool isLocked     = false;
};

struct quantize_settings {
    tick_t quantizeStart = TICKS_16TH*2;
    tick_t quantizeEnd = TICKS_16TH*2;
};
struct groove_timing_data_t {
    std::vector<double> timePoints;
    std::vector<double> velocityPoints;
    double loopLength = 8.0;
};
struct groove_data_t {
    groove_timing_data_t timingData;
    String presetName;
    String grooveName;
    tick_t lenQuantization = TICKS_16TH;
    float strengthQuantization = 0.0f;
    float strengthGroove = 1.0f;
    float strengthVelocity = 0.4f;
    float randomTiming = 0.0f;
    float randomVelocity = 0.0f;
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
    int32_t tempo100 = 12800;
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
    /** projects groove settings */
    std::vector<groove_data_t> grooveData;
};
