#include "str_util.h"
#include "tests/common/test_common.h"
#include "../host/vst_host.h"
#include "tls.h"
#include "appconfig.h"


int main(int argc, char* argv[]) {
    auto audiohost = std::make_unique<vsthost>();
    vsthost::assignMasterCallback(audiohost.get());
    daw_tls::tlsinstance _tls;
    _tls.tlsInitialized = true;
    _tls.config         = new app_config_t{};
    _tls.host           = audiohost.get();
    daw_tls::setTls(_tls);
    vsthost::getInstance()->onTick();
    vsthost::getInstance()->unload();
    vsthost::getInstance()->destroy();
}
