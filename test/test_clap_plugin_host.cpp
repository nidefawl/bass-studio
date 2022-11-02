#include "TestBase.hpp"
#include "appsettings.h"
#include "host/daw/mainctrl.h"
#include "host/plugin/clap/clap-plugin.h"
#include "host/graph/effect_graph.h"
#include "str_util.h"
#include "common/test_common.h"
#include "host/host_pluginmanager.h"
#include "host/host.h"
#include "tls.h"
#include "appconfig.h"
#include "thread.h"
#include <memory>

namespace test_clap_plugin_host {

    std::shared_ptr<DawInstance> initDaw(const sampleformat_t sampleformat) {
        auto dawInstance = std::make_shared<DawInstance>();
        log_out("Testing Samplerate %uHz at Blocksize %u\n", sampleformat.sampleRate, sampleformat.blockSize);
        auto& settings      = *daw_tls::getTls().settings;
        settings.saveOnExit = false;
        settings.iosettings.midiconfigs.clear();
        settings.iosettings.configs.clear();
        settings.iosettings.asioConfig         = {};
        settings.iosettings.internalSamplerate = sampleformat.sampleRate;
        settings.iosettings.internalBlocksize  = sampleformat.blockSize;
        settings.iosettings.blocksize          = sampleformat.blockSize;
        settings.dawsettings.audioEnabled      = false;

        dawInstance->initDaw();
        dbgassert(dawInstance->getHost()->m_sampleFormatInternal == sampleformat);
        dawInstance->startDaw();
        dawInstance->initProcessingResources();
        return dawInstance;
    }
    void test_clap_plugin(DawInstance* daw) {
        TEST_BEGIN("test_clap_plugin_loader");
        auto host       = daw->getHost();
        String filepath = "/data/dev/clap/clap-plugins/builds/ninja-headless/plugins/Debug"
                          "/"
                          "clap-plugins.clap";
        auto res        = host->loadPlugin({filepath, 0, 0, 0, 1});
        TEST_ASSERT_THROW(res.library.isSuccess());
        TEST_ASSERT_THROW(res.clapPlugin != nullptr);
        host->onTick();
        host->unloadPlugin(res.plugin);
        host->unload();
        host->destroy();

        TEST_END();
    }

}// namespace test_clap_plugin_host

int main() {
    setExceptionHandler();
    App::Platform::initPlatformEnvironment("daw");
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
    daw_tls::initNewTls();
    auto sf  = sampleformat_t{ 44100, 512 };
    auto daw = test_clap_plugin_host::initDaw(sf);
    test_clap_plugin_host::test_clap_plugin(daw.get());
    return 0;
}
