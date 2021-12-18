#include "tls.h"
#include "host/mainctrl.h"
#include "host/vst_host.h"
#include "host/audio_host.h"
#include "host/midi_host.h"
#include "host/plugindatabase.h"
#include "host/projectcontroller.h"
#include "audiocache.h"
#include "gui/drawwaveform.h"

namespace daw_tls {
	static thread_local tlsinstance tls;

	void setTls(tlsinstance& _tls) {
		dbgassert(_tls.tlsInitialized);
		tls = _tls;
	}
	tlsinstance& getTls() {
		tlsinstance& localTls = tls;
		dbgassert(localTls.tlsInitialized);
		return localTls;
	}
}

plugindatabase_t* plugindatabase_t::getInstance() {
	dbgassert(daw_tls::tls.pluginDatabase);
	return daw_tls::tls.pluginDatabase;
}
vsthost* vsthost::getInstance()
{
	dbgassert(daw_tls::tls.host);
	return daw_tls::tls.host;
}
project_controller_t* project_controller_t::get()
{
	dbgassert(daw_tls::tls.project);
	return daw_tls::tls.project;
}
waveformrender* waveformrender::getInstance()
{
//	dbgassert(daw_tls::tls.waveform);
	return daw_tls::tls.waveform;
}
audiocache* audiocache::getInstance()
{
	dbgassert(daw_tls::tls.audioCache);
	return daw_tls::tls.audioCache;
}
MainCtrl* MainCtrl::get() {
//	dbgassert(daw_tls::tls.mainCtrl);
	return daw_tls::tls.mainCtrl;
}
DawInstance* DawInstance::get() {
//	dbgassert(daw_tls::tls.mainCtrl);
	if (daw_tls::tls.mainCtrl)
		return &daw_tls::tls.mainCtrl->daw;
	return nullptr;
}
audiohost* audiohost::getInstance()
{
	dbgassert(daw_tls::tls.audioHost);
	return daw_tls::tls.audioHost;
}
midihost* midihost::getInstance()
{
	dbgassert(daw_tls::tls.midiHost);
	return daw_tls::tls.midiHost;
}
