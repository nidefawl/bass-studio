#pragma once

namespace DAW::Host {
    class Host;
    class PluginManager;
}
namespace DAW::UI {
    class CommandManager;
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
struct app_daw_settings;
namespace daw_tls {
    struct tlsinstance {
        appruntime* runtime                     = nullptr;
        appsettings* settings                   = nullptr;
        DawInstance* dawInstance                = nullptr;
        DAW::Host::Host* host                   = nullptr;
        DAW::Host::PluginManager* pluginManager = nullptr;
        audiohost* audioHost                    = nullptr;
        midihost* midiHost                      = nullptr;
        MainCtrl* mainCtrl                      = nullptr;
        audiocache* audioCache                  = nullptr;
        plugindatabase_t* pluginDatabase        = nullptr;
        project_controller_t* project           = nullptr;
        DAW::UI::CommandManager* commandManager = nullptr;
        bool tlsInitialized                     = false;
    };
    bool isTlsInitialized();
    tlsinstance& initNewTls();
    void setTls(tlsinstance& tls);
    tlsinstance& getTls();
    appsettings& getSettings();
    appruntime& getRuntime();
    app_daw_settings& getDawSettings();
};// namespace daw_tls
