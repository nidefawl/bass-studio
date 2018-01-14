#pragma once
#include "config.h"
#include "str_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include <vector>
#include <atomic>
#include <stdint.h>
#include <stdbool.h>
#include "../vst_sdk_2.4/aeffectx.h"
#include "../util/readerwriterqueue.h"
#include "note.h"
#include "hires_timer.h"
#include "project.h"
#include <memory>


//-------------------------------------------------------------------------------------------------------
/*! hostCanDos strings Plug-in -> Host */
namespace HostCanDos
{
	extern const char* canDoSendVstEvents; ///< Host supports send of Vst events to plug-in
	extern const char* canDoSendVstMidiEvent; ///< Host supports send of MIDI events to plug-in
	extern const char* canDoSendVstTimeInfo; ///< Host supports send of VstTimeInfo to plug-in
	extern const char* canDoReceiveVstEvents; ///< Host can receive Vst events from plug-in
	extern const char* canDoReceiveVstMidiEvent; ///< Host can receive MIDI events from plug-in 
	extern const char* canDoReportConnectionChanges; ///< Host will indicates the plug-in when something change in plug-in´s routing/connections with #suspend/#resume/#setSpeakerArrangement 
	extern const char* canDoAcceptIOChanges; ///< Host supports #ioChanged ()
	extern const char* canDoSizeWindow; ///< used by VSTGUI
	extern const char* canDoOffline; ///< Host supports offline feature
	extern const char* canDoOpenFileSelector; ///< Host supports function #openFileSelector ()
	extern const char* canDoCloseFileSelector; ///< Host supports function #closeFileSelector ()
	extern const char* canDoStartStopProcess; ///< Host supports functions #startProcess () and #stopProcess ()
	extern const char* canDoShellCategory; ///< 'shell' handling via uniqueID. If supported by the Host and the Plug-in has the category #kPlugCategShell
	extern const char* canDoSendVstMidiEventFlagIsRealtime; ///< Host supports flags for #VstMidiEvent
}

//-------------------------------------------------------------------------------------------------------
/*! plugCanDos strings Host -> Plug-in */
namespace PlugCanDos
{
	extern const char* canDoSendVstEvents; ///< plug-in will send Vst events to Host
	extern const char* canDoSendVstMidiEvent; ///< plug-in will send MIDI events to Host
	extern const char* canDoReceiveVstEvents; ///< plug-in can receive MIDI events from Host
	extern const char* canDoReceiveVstMidiEvent; ///< plug-in can receive MIDI events from Host 
	extern const char* canDoReceiveVstTimeInfo; ///< plug-in can receive Time info from Host 
	extern const char* canDoOffline; ///< plug-in supports offline functions (#offlineNotify, #offlinePrepare, #offlineRun)
	extern const char* canDoMidiProgramNames; ///< plug-in supports function #getMidiProgramName ()
	extern const char* canDoBypass; ///< plug-in supports function #setBypass ()
}

class vstplugin;
struct track_impl_t;
typedef void PaStream;

typedef AEffect*(VSTPluginMain_t)(audioMasterCallback audioMasterCB);


VstIntPtr VSTCALLBACK audioMaster(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);

class vstpluginloadres {
public:
	vstpluginloadres(int32_t _result, vstplugin* _plugin) :
		result(_result), plugin(_plugin) { };
	int32_t result;
	vstplugin* plugin;
};
struct AudioBlock;
struct AudioBuffer {
	AudioBlock* output;
	AudioBlock* input;
	std::atomic<bool> inUse;
	bool submitted;
};
struct plugin_notes_t {
	std::vector<note_t> notes;
};
#define RING_BUF_SIZE 16
#define RING_BUF_MASK 15
struct audiothread_ringbuffer_t {
	int32_t readPos = 0;
	int32_t writePos = 0;
	AudioBuffer* buffers[RING_BUF_SIZE] = { 0 };
};
class vsthost {
private:
	class ModuleManager;
	ModuleManager* moduleMgr;
	project_globals_t& project;
public:
	samplerate_t lSampleRate;
	uint16_t lBlockSize;
private:

	double lastTickEndPos = 0;
	playback_state lastState = playback_state::status_stop;
	uint8_t numChannels;
	VstTimeInfo timeinfo{0};

	std::vector<vstplugin*> list;

	std::atomic<PaStream*> stream{NULL};
	audiothread_ringbuffer_t ringbuffer;
public:
	moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueue;
public:
	vsthost(project_globals_t& _project, uint32_t _sampleRate = 44100, uint16_t _blockSize = 512);
	vsthost(vsthost const&) = delete;
	~vsthost();
	void operator=(vsthost const&) = delete;
	static vsthost* getInstance();
	static void setInstance(std::unique_ptr<vsthost> host);

	uint32_t blockReads = 0;
	uint32_t bufferUnderuns = 0;
	hires_timer_t timer;
	std::atomic<int32_t> pluginId{100};

	void updateTime(int32_t samplePos, tick_t pos, playback_state state);
	int32_t processPlayback(int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround);
	void processAudio(track_impl_t* channel, AudioBlock* input, AudioBlock* output, unsigned long samples);
	void sendNotesOff(vstplugin* plugin);

	VstTimeInfo* getTimeInfo() {
		return &this->timeinfo;
	}
	bool startAudio();
	bool stopAudio();
	void updateDisplay();
	void destroy();
	void unload();
	bool postInit();
	bool onTick();
	void onStreamEnd();
	void onStartPlayback(int32_t block);
	void onStopPlayback();
	bool unloadAllPlugins();
	bool isStreaming() {
		return this->stream != NULL;
	}

	bool canDo(const char *ptr)
	{
		if ((!strcmp(ptr, HostCanDos::canDoSendVstEvents)) ||
			(!strcmp(ptr, HostCanDos::canDoSendVstMidiEvent)) ||
			(!strcmp(ptr, HostCanDos::canDoReceiveVstEvents)) ||
			(!strcmp(ptr, HostCanDos::canDoReceiveVstMidiEvent)) ||
			(!strcmp(ptr, HostCanDos::canDoSizeWindow)) ||
			(!strcmp(ptr, HostCanDos::canDoSendVstMidiEventFlagIsRealtime)) ||
			(!strcmp(ptr, HostCanDos::canDoStartStopProcess)) ||
			0)
			return true;
		return false;
	}
	vstplugin* getPlugin(AEffect* aeffect);
	void unloadPlugin(vstplugin* plugin);
	void unloadTrack(track_t* track);
	uint32_t pluginCount();
	vstplugin* getPluginIdx(uint32_t i);
	vstpluginloadres loadPlugin(String filepath, int32_t globalId = 0);
	track_impl_t* createAudio(track_t* track);
	bool movePlugin(track_t* dstTr, track_impl_t* trp, int32_t src, int32_t dst);
	bool swapEffects(track_impl_t* trp, int32_t src, int32_t dst);
	bool insertNewPlugin(track_impl_t* trp, vstplugin* plugin, int32_t dst);
};
