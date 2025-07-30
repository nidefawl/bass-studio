#include "TestBase.hpp"
#include "appsettings.hpp"
#include "fileio.hpp"
#include "host/daw/mainctrl.hpp"
#include "host/host_plugin_loadresult.hpp"
#include "host/plugin/clap/clap-plugin.hpp"
#include "host/graph/effect_graph.hpp"
#include "host/plugin/modules.hpp"
#include "str_util.hpp"
#include "common/test_common.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/host.hpp"
#include "tls.hpp"
#include "appconfig.hpp"
#include "thread.hpp"
#include <memory>
#include <vector>

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
#if defined(__linux__)
#define PLATFORM_TEST_CLAP_EXT "clap"
#elif defined(__APPLE__)
#define PLATFORM_TEST_CLAP_EXT "clap"
#else
#define PLATFORM_TEST_CLAP_EXT "clap"
#endif
        std::vector<FileFound> files;
        findFilesWithExt(TEST_PATH("plugins/clap"), PLATFORM_TEST_CLAP_EXT, true, files);
        for (const auto& file : files) {
            auto loadresult = host->loadPlugin({file.path, 0, 0, 0, ModuleType::MODULE_TYPE_CLAP});
            auto res = *loadresult;
            TEST_ASSERT_THROW(res.library.isSuccess());
            TEST_ASSERT_THROW(res.clapPlugin != nullptr);
            host->onTick();
            host->unloadPlugin(res.plugin);
        }
        if (files.empty()) {
            log_out("No clap plugins found in %s\n", TEST_PATH("plugins/clap"));
        }
        host->unload();
        host->destroy();

        TEST_END();
    }

}// namespace test_clap_plugin_host

int main() {
    setExceptionHandler();
    App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
    daw_tls::initNewTls();
    auto sf  = sampleformat_t{ 44100, 512 };
    auto daw = test_clap_plugin_host::initDaw(sf);
    test_clap_plugin_host::test_clap_plugin(daw.get());
    return 0;
}
