#include "tls.h"
#include "appconfig.h"
#include "appsettings.h"
#include "host/daw/mainctrl.h"
#include "host/host_pluginmanager.h"
#include "host/host.h"
#include "host/audiohost/audio_host.h"
#include "host/midihost/midi_host.h"
#include "host/plugindatabase/plugindatabase.h"
#include "host/project/projectcontroller.h"
#include "host/audiocache/audiocache.h"
#include "wave/waveform_render_impl.h"
#include "thread.h"
#ifdef _WIN32
#include "platform/win/windowsize.h"
#endif
#ifdef __linux__
#include "platform/linux/windowsize.h"
#endif

namespace daw_tls {
    static thread_local tlsinstance tls;
    bool isTlsInitialized() {
        return tls.tlsInitialized;
    }
    daw_tls::tlsinstance& initNewTls() {
        if (!tls.tlsInitialized) {
            tlsinstance localTls{};
            localTls.tlsInitialized = true;
            localTls.runtime = new appruntime{};
            localTls.settings = new appsettings{};
            daw_tls::setTls(localTls);
        }
        return tls;
    }
    void setTls(tlsinstance& _tls) {
        dbgassert(_tls.tlsInitialized != tls.tlsInitialized);
        tls = _tls;
    }
    tlsinstance& getTls() {
        tlsinstance& localTls = tls;
        dbgassert(localTls.tlsInitialized);
        return localTls;
    }
    
    appsettings& getSettings() {
        dbgassert(tls.tlsInitialized);
        dbgassert(tls.settings);
        return *tls.settings;
    }
    appruntime& getRuntime() {
        dbgassert(tls.tlsInitialized);
        dbgassert(tls.runtime);
        return *tls.runtime;
    }
    app_daw_settings& getDawSettings() {
        dbgassert(tls.tlsInitialized);
        dbgassert(tls.settings);
        return tls.settings->dawsettings;
    }
}// namespace daw_tls

plugindatabase_t* plugindatabase_t::getInstance() {
    dbgassert(daw_tls::tls.tlsInitialized);
    dbgassert(daw_tls::tls.pluginDatabase);
    return daw_tls::tls.pluginDatabase;
}
DAW::Host::Host* DAW::Host::getInstance() {
    dbgassert(daw_tls::tls.tlsInitialized);
    dbgassert(daw_tls::tls.host);
    return daw_tls::tls.host;
}
project_controller_t* project_controller_t::get() {
    dbgassert(daw_tls::tls.tlsInitialized);
    dbgassert(daw_tls::tls.project);
    return daw_tls::tls.project;
}
audiocache* audiocache::getInstance() {
    dbgassert(daw_tls::tls.tlsInitialized);
    dbgassert(daw_tls::tls.audioCache);
    return daw_tls::tls.audioCache;
}
MainCtrl* MainCtrl::get() {
    dbgassert(daw_tls::tls.tlsInitialized);
    dbgassert(daw_tls::tls.mainCtrl);
    return daw_tls::tls.mainCtrl;
}
DawInstance* DawInstance::get() {
    dbgassert(daw_tls::tls.tlsInitialized);
    auto mainCtrl = daw_tls::tls.mainCtrl;
    dbgassert(mainCtrl);
    if (mainCtrl)
        return &mainCtrl->daw;
    return nullptr;
}
DawInstance* DawInstance::getOptional() {
    auto mainCtrl = daw_tls::tls.mainCtrl;
    if (mainCtrl)
        return &mainCtrl->daw;
    return nullptr;
}
audiohost* audiohost::getInstance() {
    dbgassert(daw_tls::tls.tlsInitialized);
    dbgassert(daw_tls::tls.audioHost);
    return daw_tls::tls.audioHost;
}
midihost* midihost::getInstance() {
    dbgassert(daw_tls::tls.tlsInitialized);
    dbgassert(daw_tls::tls.midiHost);
    return daw_tls::tls.midiHost;
}
