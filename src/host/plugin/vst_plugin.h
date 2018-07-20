#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "str_util.h"
#include "seq_time.h"
//#include "../vst_sdk_2.4/aeffectx.h"
#include "../vst_window.h"
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
//-------------------------------------------------------------------------------------------------------
/** Flags used in #VstParameterProperties. */
//-------------------------------------------------------------------------------------------------------
enum vst_param_flags
{
//-------------------------------------------------------------------------------------------------------
	ParamIsSwitch				 = 1 << 0,	///< parameter is a switch (on/off)
	ParamUsesIntegerMinMax		 = 1 << 1,	///< minInteger, maxInteger valid
	ParamUsesFloatStep			 = 1 << 2,	///< stepFloat, smallStepFloat, largeStepFloat valid
	ParamUsesIntStep			 = 1 << 3,	///< stepInteger, largeStepInteger valid
	ParamSupportsDisplayIndex 	 = 1 << 4,	///< displayIndex valid
	ParamSupportsDisplayCategory = 1 << 5,	///< category, etc. valid
	ParamCanRamp				 = 1 << 6,	///< set if parameter value can ramp up/down
	ParamIsAdvanced				 = 1 << 31	///< set if parameter value can ramp up/down
//-------------------------------------------------------------------------------------------------------
};

union param_step_fi {
	float valFloat;
	int32_t valInt;
};
struct vst_param_category {
	int32_t idx;
	int16_t numParametersInCategory;			///< number of parameters in category
	String label; //24
};
struct vst_param {
	int32_t idx;
	float value;
	int32_t flags;

	param_step_fi min;
	param_step_fi max;
	param_step_fi stepSmall;
	param_step_fi step;
	param_step_fi stepLarge;

	String shortLabel;//8
	String label;//64

	//if kVstParameterSupportsDisplayIndex
	int16_t displayIndex;		///< index where this parameter should be displayed (starting with 0)

	//if kVstParameterSupportsDisplayCategory
	int16_t category;			///< 0: no category, else group index + 1
};
class vstplugin : public effectbase {
public:
#ifndef NDEBUG
	//helper indicator in gdb.
	//gdb cannot display std::string when built without clib-debug flag (SLOW)
	const char* szName = NULL;
#endif
	String sName;
	String sDir;
	bool bEditOpen = false;
	bool bInEditIdle = false;
	int pluginCategory = 0;
	bool isSynth = false;
	int vstVersion = 0;
	int uId = 0;
	vst_window* window = NULL;
	handles_t* const handle;
	std::vector<vst_param_category> paramsCategories;
	std::vector<vst_param> params;
	std::vector<automated_param_t> automatedParams;

	std::vector<String> inputNames;
	std::vector<String> outputNames;
	vstplugin(handles_t* _handle, int32_t globalId, String sDir, String sName) : effectbase(globalId), handle(_handle) {
		this->sDir = sDir;
		this->sName = sName;
#ifndef NDEBUG
		this->szName = this->sName.c_str();
#endif
	}
	~vstplugin();
	bool resume();
	bool sleep();


	const char* getDir() {
		return sDir.c_str();
	}
	bool updateDisplay() {
		if (this->window != NULL) {
			this->window->updateDisplay();
			return true;
		}
		return false;
	}
	String getInfo(std::vector<String>& list) override;
	long dispatch(
		long opcode = 0,
		long index = 0,
		long value = 0,
		void *ptr = 0,
		float opt = 0);
	bool getNameString(const char* szBuf);
	void printNames();
	bool onClose();
	void onWindowDestroy();
	bool onShow(vst_window* window);
	bool updateWindowSize();
	bool onResize(vst_window* window, Size size);
	Size constrainSize(vst_window* window, Size& size);
	bool show() override;
	bool close() override;
	void unload() override;
	void load(vsthost* host) override;
	vst_param_category* getCategory(int idx);
	automated_param_t* getRegisteredAutomation(int32_t idx);

	int32_t getNumParameters() override;
	String getParamName(int32_t paramIdx) override;
	String getAutomatableName() override;
	float getParamValue(int32_t idx);
	void setParamValue(int32_t idx, float val);
	void recvPluginEditParamUpdate(int32_t idx);
	void updateAutomatedParameters(tick_t pos) override;
	automation_t* getAutomation(int32_t paramIdx) override;
	void deactivateAutomation(int32_t paramIdx) override;
	void getAutomated(std::vector<int32_t>& targets) override;
	automationlane_snapshot_t toRef() override;
	void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) override;
	guiplugin* makeGui() override;
	void setSlot(int i) override;
	int32_t getSlot() override;
	void breakTrackLink() override;
	void setTrackLink(audio_stage_t* trImpl) override;
	audio_stage_t* getTrackLink() override;
	guiplugin* getGui() override;
	void process(AudioBlock* in, AudioBlock* out, int32_t samples) override;
	int32_t getDelay() override;
	bool hasParam(int32_t idx) override;
};
