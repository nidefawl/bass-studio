#include "TestBase.hpp"
#include "str_util.h"
#include "common/test_common.h"
#include "../host/vst_host.h"
#include "tls.h"
#include "appconfig.h"


int main(int argc, char* argv[]) {
    auto host = std::make_unique<vsthost>();
    vsthost::assignMasterCallback(host.get());
    daw_tls::initNewTls().host = host.get();
    TEST_ASSERT_EQUAL(vsthost::getInstance(), host.get());
    host->onTick();
    host->unload();
    host->destroy();
}
