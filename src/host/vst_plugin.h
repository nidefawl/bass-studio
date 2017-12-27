#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "str_util.h"
//#include "../vst_sdk_2.4/aeffectx.h"
#include "vst_window.h"
#include "automation.h"

class vsthost;
struct AudioBlock;
struct handles_t;

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

class vstplugin {
public:
	String sName;
	String sDir;
	bool bEditOpen = false;
	bool bInEditIdle = false;
	bool bIsEnabled = false;
	bool bIsSetup = false;
	bool bCanReceiveMidi = false;
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
	AudioBlock* blockInputs = NULL; // guaranteed to have at least 2 channels
	AudioBlock* blockOutputs = NULL; // guaranteed to have at least 2 channels
	vstplugin(handles_t* _handle, String sDir, String sName) : handle(_handle) {
		this->sDir = sDir;
		this->sName = sName;
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
	String getInfo();
	long dispatch(
		long opcode = 0,
		long index = 0,
		long value = 0,
		void *ptr = 0,
		float opt = 0);
	bool getNameString(const char* szBuf);
	void printNames();
	bool onClose();
	bool onShow(vst_window* window);
	bool onResize(vst_window* window, Size size);
	Size constrainSize(vst_window* window, Size& size);
	bool show();
	bool close();
	void unload();
	void load(vsthost* host);
	vst_param_category* getCategory(int idx);
	vst_param* getParam(int32_t idx);
	float getParamValue(int32_t idx);
	void setParamValue(int32_t idx, float val);
	automated_param_t* getRegisteredAutomation(int32_t idx);
	void registerAutomationSrc(automated_param_t& p);
	void unregisterAutomationSrc(automated_param_t& p);
	void updateAutomatedParameters(tick_t pos);
};

