#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "dsp_util.h"

#include "vst_host.h"
#include "fileio.h"
#include "track.h"
#include "basectrl.h"
#include "mainctrl.h"

#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"

#include "../vstsdk-host-2.4/aeffectx.h"
#include "portaudio.h"

#include "logging.h"
#include "audioblock.h"
#include "audiobuffer.h"
#include "platform.h"

#include <stdlib.h>
#include <algorithm>
#include <stdlib.h>
#include <memory.h>
#include "track_impl.h"

#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __MINGW32__
#include "../platform/mingw/mingw.mutex.h"
#endif
#ifdef __linux__
#include <dlfcn.h>
#endif
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "plugins/advanced/adv-plugin.h"
#include "plugins/stereowidth/stereowidth-plugin.h"
#include "plugins/empty/empty-plugin.h"


//typedef AudioEffectX*(AEffectXMainProc)(audioMasterCallback audioMasterCB);
//AudioEffectX* createPluginTest (audioMasterCallback audioMaster);
//AudioEffectX* createPluginStereoWidth (audioMasterCallback audioMaster);
//
//namespace Plugin::StereoWidth {
//	ViewContainers* createView();
//	AudioEffectX* createPlugin (audioMasterCallback audioMaster);
//}
vstpluginloadres vsthost::loadInternalPlugin(int32_t moduleId, int32_t globalId) {
//	AEffectXMainProc* fn = NULL;
	AudioEffectX* axeffect = NULL;
	String name = "";
	switch (moduleId) {
	case PLUG_INT_STEREOWIDTH:
		axeffect = PluginStereoWidth::createPlugin(audioMaster);
		name = "StereoWidth";
		break;
	case PLUG_INT_TEST:
		axeffect = PluginTestAdv::createPlugin(audioMaster);
		name = "TestAdv";
		break;
	case PLUG_INT_CRASHVST:
		axeffect = PluginEmptyVST2::createPlugin(audioMaster);
//		axeffect = PluginTestAdv::createPlugin(audioMaster);
		name = "CrashVST2";
		break;
	default:
		assert(0);
		break;
	}
	if (!axeffect) {
		return vstpluginloadres(-1, NULL);
	}
	void* moduleHandle = nullptr;
	globalId = getNextGlobalModuleId(globalId);
//	const int bufLen = 512*4;
//	char* cwdbuf = _getcwd(NULL, bufLen);
//	String strPath = "??";
//	if (cwdbuf) {
//		strPath = cwdbuf;
//		free(cwdbuf);
//		replaceString(strPath, "\\", "/");
//		my_printf("getcwd: %s\n", StringAsCStr(strPath));
//	}
	AEffect* aeffect = axeffect->getAeffectHandle();
	vstplugin* plugin = new vstplugin(new handles_t(axeffect, aeffect, moduleHandle), globalId, "", name, moduleId);
	pluginInstancesVST2.push_back(plugin);
	pluginInstances.push_back(plugin);
	plugin->load(this);
	return vstpluginloadres(0, plugin);
}
