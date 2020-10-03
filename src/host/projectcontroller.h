#pragma once
#include "track.h"
#include "vst_host.h"

class project_controller_t {

	project_t* project;
	project_globals_t* projectGlobals;
public:
	project_controller_t(project_t* const _project, project_globals_t* const _projectGlobals)
	  : project(_project), projectGlobals(_projectGlobals)
	{

	}
	virtual ~project_controller_t() {

	}
	tick_t& getCursorPos() {
		return projectGlobals->cursor.cursorPos;
	}
	float getCurrentTempoBPM() {
		return projectGlobals->tempo100 / 100.0f;
	}
	int32_t getCurrentTempo() {
		return static_cast<int32_t>(projectGlobals->tempo100);
	}
	virtual void setTempo(int32_t _tempo100) {
		projectGlobals->tempo100 = static_cast<uint32_t>(_tempo100);
	}
	tick_t& getPlaybackPos() {
		return projectGlobals->playbackPos;
	}
	uint32_t sigNum() {
		return projectGlobals->signatureNum;
	}
	uint32_t sigDen() {
		return 1<<projectGlobals->signatureDenom;
	}
	uint32_t sigDenExp() {
		return projectGlobals->signatureDenom;
	}
	void setNum(uint32_t n) {
		projectGlobals->signatureNum = CLAMP_I(n, 1, 32);
	}
	void setDen(uint32_t d) {
		for (uint32_t i = 0; i <= 4; i++) {
			if (d < (1u << (i + 1u))) {
				projectGlobals->signatureDenom = i;
				return;
			}
		}
	}
	int32_t tickToSamples(tick_t ticks);
	tick_t samplesToTicks(int32_t sample);
    beatbar16th_t toBeatBar16th(int32_t tick);

	static project_controller_t* get();
	double getProjectWorkingArea() {
		return 1000.0;
	}
	virtual inline void addTrackImpl(int32_t trackInsertPos, track_t* newTrack, int flags) {
		project->trackList.addTrack(trackInsertPos, newTrack);
		if ((flags&FLG_TRK_CHANGE_HISTORY_UNDO) != 0) {
			dbgassert(newTrack->audio);
		} else {
			dbgassert(!newTrack->audio);
			vsthost* host = vsthost::getInstance();
			host->createAudio(newTrack);
		}
	}
	project_globals_t& getGlobals() {

		return *projectGlobals;
	}
	trackallcontainer_t& getTracks() {
		return project->trackList;
	}
	project_t* getProject() {
		return project;
	}
};
