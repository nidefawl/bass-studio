#pragma once
#include "profiling.h"

class audiohost;
class midihost;
class vsthost;
class waveformrender;
class MainCtrl;
class audiocache;
class plugindatabase_t;
class project_controller_t;
struct app_config_t;
namespace daw_tls {
    struct tlsinstance {
        render_clip_cache_stats_t renderClipCacheStats{};
        prof_stats_render_t renderStats;
        prof_stats_render_t prevRenderStats;
        app_config_t* config             = nullptr;
        vsthost* host                    = nullptr;
        audiohost* audioHost             = nullptr;
        midihost* midiHost               = nullptr;
        MainCtrl* mainCtrl               = nullptr;
        audiocache* audioCache           = nullptr;
        plugindatabase_t* pluginDatabase = nullptr;
        project_controller_t* project    = nullptr;
        bool tlsInitialized              = false;
    };

    void setTls(tlsinstance& tls);
    tlsinstance& getTls();
};// namespace daw_tls
