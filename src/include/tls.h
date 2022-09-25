#pragma once

namespace DAW {
    class pluginhost;
    class pluginmanager;
}
class audiohost;
class midihost;
class waveformrender;
class MainCtrl;
class audiocache;
class plugindatabase_t;
class project_controller_t;
class DawInstance;
struct appruntime;
struct appsettings;
namespace daw_tls {
    struct tlsinstance {
        appruntime* runtime              = nullptr;
        appsettings* settings            = nullptr;
        DawInstance* dawInstance         = nullptr;
        DAW::pluginhost* host             = nullptr;
        DAW::pluginmanager* pluginManager = nullptr;
        audiohost* audioHost             = nullptr;
        midihost* midiHost               = nullptr;
        MainCtrl* mainCtrl               = nullptr;
        audiocache* audioCache           = nullptr;
        plugindatabase_t* pluginDatabase = nullptr;
        project_controller_t* project    = nullptr;
        bool tlsInitialized              = false;
    };
    bool isTlsInitialized();
    tlsinstance& initNewTls();
    void setTls(tlsinstance& tls);
    tlsinstance& getTls();
    appsettings& getSettings();
};// namespace daw_tls
