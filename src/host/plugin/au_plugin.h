#pragma once
#ifdef __APPLE__
#endif
#include <stdint.h>
#include <stdbool.h>
#include "math/vec.h"
#include "str_util.h"
#include "seq_time.h"

#include "automation.h"
#include "logging.h"
#include "platform.h"
#include "meter.h"
#include "snapshot.h"
#include "base_plugin.h"

class vsthost;
struct AudioBlock;
struct handles_t;
class track_t;
class guibase;
struct track_impl_t;
class vst_window;

class auplugin : public effectbase {
public:
	handles_t* const handle;
	String sDir;
	bool bEditOpen = false;
	bool bInEditIdle = false;
	int pluginCategory = 0;
	int vstVersion = 0;
	int uId = 0;
	vst_window* window = NULL;
//	std::vector<vst_param_category> paramsCategories;
	std::vector<String> programNames;
//	std::vector<vst_param> vstParams;

	std::vector<String> inputNames;
	std::vector<String> outputNames;
	auplugin(handles_t* _handle, int32_t globalId, String sDir, String sName)
	: effectbase(sName, PLUGIN_TYPE_AU, globalId), handle(_handle) {
		this->sDir = sDir;
	}
	~auplugin();
	void resume();
	void sleep();
protected:
	void onEnable();
	void onDisable();
public:
	virtual int getModuleType() override { return PLUGIN_TYPE_AU; };

	const char* getDir() {
		return sDir.c_str();
	}
//	bool updateWindow();
	String getInfo(std::vector<String>& list) override;
//	long dispatch(
//		long opcode = 0,
//		long index = 0,
//		long value = 0,
//		void *ptr = 0,
//		float opt = 0);
	bool getNameString(char* szBuf);
	void printNames();
	bool onClose();
	void onWindowDestroy();
	bool onShow(vst_window* window);
	bool updateWindowSize();
	bool onResize(vst_window* window, ivec2 size);
	ivec2 constrainSize(vst_window* window, ivec2& size);
	bool show() override;
	bool close() override;
	void unload(vsthost* host) override;
	void load(vsthost* host) override;
//	vst_param_category* getCategory(int idx);
//	void recvPluginEditParamUpdate(int32_t idx);

	//automatable_t
	String getAutomatableName() override;
	float getParamValue(int32_t idx) override;
	String getParamValueDisplay(int32_t idx) override;
	void setParamValue(int32_t idx, float val, int flags) override;
	automationlane_snapshot_t toRef() override;
	void postSetParameter(int32_t idx, float preVal, float val, int flags) override;

	void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) override;
	void loadSnapshot(const plugin_snapshot_t& pluginSnapshot) override;
	guiplugin* makeGui() override;
	guiplugin* getGui() override;
	void process(AudioBlock* in, AudioBlock* out, int32_t samplePos, int32_t numSamples, playback_state state) override;
	int32_t getDelay() override;
};
