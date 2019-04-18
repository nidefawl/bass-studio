#include "tls.h"
#include "mainctrl.h"
#include "vst_host.h"
namespace daw_tls {
	static thread_local tlsinstance tls;

	void setTls(tlsinstance& _tls) {
		tls = _tls;
	}
	tlsinstance& getTls() {
		return tls;
	}
}

plugindatabase_t* plugindatabase_t::getInstance() {
	assert(daw_tls::tls.pluginDatabase);
	return daw_tls::tls.pluginDatabase;
}
vsthost* vsthost::getInstance()
{
	assert(daw_tls::tls.host);
	return daw_tls::tls.host;
}
project_controller_t* project_controller_t::get()
{
	assert(daw_tls::tls.project);
	return daw_tls::tls.project;
}
waveformrender* waveformrender::getInstance()
{
	assert(daw_tls::tls.waveform);
	return daw_tls::tls.waveform;
}
audiocache* audiocache::getInstance()
{
	assert(daw_tls::tls.audioCache);
	return daw_tls::tls.audioCache;
}
MainCtrl* MainCtrl::get() {
//	assert(daw_tls::tls.mainCtrl);
	return daw_tls::tls.mainCtrl;
}
