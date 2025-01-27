#include "TestBase.hpp"
#include "host/graph/effect_graph.hpp"
#include "str_util.hpp"
#include "common/test_common.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/host.hpp"
#include "thread.hpp"
#include "tls.hpp"
#include "appconfig.hpp"
#include "buildinfo.h"
#include <memory>


int main(int argc, char* argv[]) {
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
    App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
    auto host = std::make_unique<DAW::Host::Host>();
    auto pluginMgr = host.get();
    DAW::Host::PluginManager::assignMasterCallback(pluginMgr);
    host->setSampleFormat(sampleformat_t{ static_cast<samplerate_t>(48000), 512, sampleformat_bits_t::FLOAT_32 });
    auto& tls = daw_tls::initNewTls();
    tls.host = host.get();
    tls.pluginManager = pluginMgr;
    host->setTls(tls);
    
    TEST_ASSERT_EQUAL(DAW::Host::getInstance(), host.get());
    host->onTick();
    host->unload();
    host->destroy();
}
