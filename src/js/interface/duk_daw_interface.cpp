#include "duk_daw_interface.h"
#include "host/mainctrl.h"
#include "host/track_graph.h"
#include "host/vst_host.h"

#include <dukglue/dukglue.h>

namespace NU {
namespace SCRIPTING {


//DAW::ScriptWrapper_track_graph_t* getAudioGraph() {
//	return &vsthost::getInstance()->lastTrackGraph;
//}
void registerInterfaceToContext(duk_context* ctx) {
	dukglue_register_method(ctx, &DawInstance::setEmptyProject, "setEmptyProject");
	dukglue_register_method(ctx, &DawInstance::stopPlaying, "stop");
	dukglue_register_method(ctx, &DawInstance::startPlaying, "start");
	dukglue_register_method(ctx, &DawInstance::loadFile, "loadFile");
	dukglue_register_method(ctx, &DawInstance::loadFileCStr, "loadFileCStr");
	dukglue_register_method(ctx, &DawInstance::menuCommand, "menuCommand");
	dukglue_register_method(ctx, &DawInstance::saveFile, "saveFile");
//	dukglue_register_function(ctx, &getAudioGraph, "getAudioGraph");
}
void setGlobalInstance(duk_context* ctx, DawInstance* pInterfaceInstance) {

//		dukglue_register_method(ctx, &MainCtrl::getPlaybackPos, "getPlaybackPos");7
	dukglue_register_global(ctx, pInterfaceInstance, "dawInstance");
//		dukglue_register_method(ctx, &MainCtrl::getPlaybackPos, "getPlaybackPos");
}


}
}
