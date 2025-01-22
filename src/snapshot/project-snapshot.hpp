#pragma once
#include "basectrl.hpp"
#include "host/project/project.hpp"
#include "snapshot/project-snapshot.hpp"
#include "host/track/track.hpp"
#include "snapshot/track-snapshot.hpp"
#include "gui/container/container_layout_snapshot.hpp"
#include "types.hpp"
#include <cstdint>

struct project_snapshot_t {
    trackcontainer_snapshot_t trackCtr;
    trackcontainer_snapshot_t trackReturnCtr;
    trackcontainer_snapshot_t trackMasterCtr;
    std::vector<int32_t> solodTracks;
    std::vector<int32_t> recordArmedTracks;
    project_globals_t globals;
    export_settings_t exportSettings;
    quantize_settings quantizeSettings;
    samplerate_t samplerate = 0;
};
