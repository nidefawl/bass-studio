#include "duk_daw_interface.h"
#include "host/mainctrl.h"

#include <dukglue/dukglue.h>

namespace NU {
namespace SCRIPTING {

	void registerInterfaceToContext(duk_context* ctx) {

		dukglue_register_method(ctx, &MainCtrl::setEmptyProject, "setEmptyProject");
		dukglue_register_method(ctx, &MainCtrl::stopPlaying, "stop");
		dukglue_register_method(ctx, &MainCtrl::startPlaying, "start");
		dukglue_register_method(ctx, &MainCtrl::loadFile, "loadFile");
		dukglue_register_method(ctx, &MainCtrl::loadFileCStr, "loadFileCStr");
		dukglue_register_method(ctx, &MainCtrl::menuCommand, "menuCommand");
		dukglue_register_method(ctx, &MainCtrl::saveFile, "saveFile");

	}
	void setGlobalInstance(duk_context* ctx, MainCtrl* pInterfaceInstance) {

//		dukglue_register_method(ctx, &MainCtrl::getPlaybackPos, "getPlaybackPos");7
		dukglue_register_global(ctx, pInterfaceInstance, "dawInstance");
//		dukglue_register_method(ctx, &MainCtrl::getPlaybackPos, "getPlaybackPos");
	}
}
}
