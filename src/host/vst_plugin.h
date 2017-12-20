#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "str_util.h"
//#include "../vst_sdk_2.4/aeffectx.h"
#include "vst_window.h"

class vsthost;
struct AudioBlock;
struct handles_t;
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

};
