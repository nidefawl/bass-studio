#pragma once
class audiohost;
class midihost;
class vsthost;
class waveformrender;
class MainCtrl;
class audiocache;
class plugindatabase_t;
class project_controller_t;
namespace daw_tls {
	struct tlsinstance {
		bool tlsInitialized = false;
		vsthost* host = nullptr;
		audiohost* audioHost = nullptr;
		midihost* midiHost = nullptr;
		waveformrender* waveform = nullptr;
		MainCtrl* mainCtrl = nullptr;
		audiocache* audioCache = nullptr;
		plugindatabase_t* pluginDatabase = nullptr;
		project_controller_t* project = nullptr;
	};
	void setTls(tlsinstance& tls);
	tlsinstance& getTls();
};
