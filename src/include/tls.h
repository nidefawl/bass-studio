#pragma once

class audiohost;
class midihost;
class vsthost;
class waveformrender;
class MainCtrl;
class audiocache;
class plugindatabase_t;
class project_controller_t;
struct appruntime;
struct appsettings;
namespace daw_tls {
    struct tlsinstance {
        appruntime* runtime              = nullptr;
        appsettings* settings            = nullptr;
        vsthost* host                    = nullptr;
        audiohost* audioHost             = nullptr;
        midihost* midiHost               = nullptr;
        MainCtrl* mainCtrl               = nullptr;
        audiocache* audioCache           = nullptr;
        plugindatabase_t* pluginDatabase = nullptr;
        project_controller_t* project    = nullptr;
        bool tlsInitialized              = false;
    };

    tlsinstance& initNewTls();
    void setTls(tlsinstance& tls);
    tlsinstance& getTls();
    appsettings& getSettings();
};// namespace daw_tls
