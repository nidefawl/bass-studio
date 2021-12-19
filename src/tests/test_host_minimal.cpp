


#include "str_util.h"
#include "test_common.h"
#include "../host/vst_host.h"
#include "../host/plugin/vst_plugin.h"
#include "tls.h"
#include "project.h"




int main(int argc, char* argv[]) {
    auto audiohost = std::make_unique<vsthost>();
	vsthost::assignMasterCallback(audiohost.get());
	daw_tls::tlsinstance _tls;
	_tls.tlsInitialized = true;
    _tls.host = audiohost.get();
	daw_tls::setTls(_tls);
	vsthost::getInstance()->onTick();
	vsthost::getInstance()->unload();
	vsthost::getInstance()->destroy();
}
