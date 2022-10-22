#include "TestBase.hpp"
#include "host/effect_graph.h"
#include "str_util.h"
#include "common/test_common.h"
#include "host/host_pluginmanager.h"
#include "host/host.h"
#include "tls.h"
#include "appconfig.h"
#include <memory>

namespace {

void test_clap_plugin_loader() {
  TEST_BEGIN("test_clap_plugin_loader");
    auto host = std::make_unique<DAW::Host::Host>();
    auto pluginMgr = host.get();
    DAW::Host::PluginManager::assignMasterCallback(pluginMgr);
    host->setSampleFormat(sampleformat_t{ static_cast<samplerate_t>(48000), 512, sampleformat_bits_t::FLOAT_32 });
    auto& tls = daw_tls::initNewTls();
    tls.host = host.get();
    tls.pluginManager = pluginMgr;
    host->setTls(tls);
    String filepath = "/data/dev/daw-deps/clap-plugins/builds/ninja-headless/plugins/Debug" "/" "clap-plugins.clap";
    uint32_t uId = 0;
    pluginMgr->loadPlugin(filepath, uId);
    
    TEST_ASSERT_EQUAL(DAW::Host::getInstance(), host.get());
    host->onTick();
    host->unload();
    host->destroy();

  TEST_END();
}

} // namespace

int main() {
  test_clap_plugin_loader();
  return 0;
}
