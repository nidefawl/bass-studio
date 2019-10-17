#include "duk_daw_interface.h"
#include "host/mainctrl.h"

#include <dukglue/dukglue.h>

namespace NU {
namespace SCRIPTING {

	void registerInterfaceToContext(duk_context* ctx) {

		dukglue_register_method(ctx, &MainCtrl::setEmptyProject, "setEmptyProject");
	}
	void setGlobalInstance(duk_context* ctx, MainCtrl* pInterfaceInstance) {

//		dukglue_register_method(ctx, &MainCtrl::getPlaybackPos, "getPlaybackPos");7
		dukglue_register_global(ctx, pInterfaceInstance, "dawInstance");
//		dukglue_register_method(ctx, &MainCtrl::getPlaybackPos, "getPlaybackPos");
	}
}
}
