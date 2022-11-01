#include "TestBase.hpp"
#include "host/graph/effect_graph.h"
#include "str_util.h"
#include "common/test_common.h"
#include "host/host_pluginmanager.h"
#include "host/host.h"
#include "tls.h"
#include "appconfig.h"
#include <memory>


int main(int argc, char* argv[]) {
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
