#pragma once
#include "basectrl.h"
#include "project.h"
#include "snapshot/project-snapshot.h"
#include "track.h"
#include "snapshot/track-snapshot.h"
#include "gui/container/container_layout_snapshot.h"
#include "types.h"

struct project_snapshot_t {
    trackcontainer_snapshot_t trackCtr;
    trackcontainer_snapshot_t trackReturnCtr;
    trackcontainer_snapshot_t trackMasterCtr;
    project_globals_t globals;
    export_settings_t exportSettings;
    quantize_settings quantizeSettings;
    samplerate_t samplerate = 0;
    dawview_layout_t layoutPrimary;
    dawview_layout_t layoutSecondary;
};
