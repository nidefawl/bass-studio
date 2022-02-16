#pragma once
#include "project.h"
#include "track.h"
#include "track_snapshot.h"

struct project_snapshot_t {
    trackcontainer_snapshot_t trackCtr;
    trackcontainer_snapshot_t trackReturnCtr;
    trackcontainer_snapshot_t trackMasterCtr;
    project_globals_t globals;
    export_settings_t exportSettings;
};
