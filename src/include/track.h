#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <list>
#include <assert.h>
#include "exceptions.h"
#include "seq_time.h"
#include "cursor.h"
#include "note.h"
#include "clip.h"
#include "str_util.h"
#include "logging.h"
#include "project.h"

#define TRACK_TYPE_MASTER 0
#define TRACK_TYPE_RETURN 1
#define TRACK_TYPE_MIDI 2
const char* TrackTypeToName(int type);
struct track_plugins_t;
class gui_trackcontent;
class gui_trackmixer;
class delete_cb;
class trackcontents_t;
class track_t;
using track_vector = std::vector<track_t*>;
void deleteTrackContents(trackcontents_t* tr, delete_cb *cb);
void deleteTrack(track_t* tr, delete_cb *cb);
void deleteClip(clip_t* cl, delete_cb *cb);
class trackcontents_t {
public:
	std::vector<clip_t*> clips;
	trackcontents_t() {
	}
	trackcontents_t(const trackcontents_t &a) {
		deepcopy(a);
	}
	trackcontents_t &operator =(const trackcontents_t &a) {
		deepcopy(a);
		return *this;
	}
	void deepcopy( const trackcontents_t &obj) {
		for (clip_t* clip : clips) {
			clip->tr = NULL;
		}
		clips.clear();
		for (clip_t* clip : obj.clips) {
			addClip(new clip_t(*clip));
		}
		sortClips();
	}
	virtual ~trackcontents_t() {
		assert(clips.empty());
	}
	virtual std::vector<clip_t*>::iterator removeClip(clip_t* clip) {
		auto it = std::find(clips.begin(), clips.end(), clip);
		if (it == clips.end()) {
			throw applogicexception("track - attempt to remove non-present clip");
		}
		return clips.erase(it);
	}
	virtual void addClip(clip_t* clip) {
		auto it = std::find(clips.begin(), clips.end(), clip);
		if (it != clips.end()) {
			throw applogicexception("track - attempt to add clip twice");
		}
		clips.push_back(clip);
	}
	void sortClips() {
		std::stable_sort(clips.begin(), clips.end(), [](const clip_t* a, const clip_t* b) {
			return a->time < b->time;
		});
		if (clips.size() > 1) {
			if (!(clips[0]->start() < clips[1]->start())) {
				for (int i = 0; i < clips.size(); i++) {
					my_printf("clip[%d] = %d\n", i, clips[i]->start());
				}
			}
			assert(clips[0]->start() < clips[1]->start());
		}
	}
	tick_t start();
	tick_t end();
	std::pair<clip_t*, clip_t*> getMinMax();
	bool hasClip(clip_t* c) {
		auto it = std::find(clips.begin(), clips.end(), c);
		return it != clips.end();
	}
	clip_t* getNextClip(clip_t* c) {
		bool matched = false;
		for (clip_t* clip : clips) {
			if (matched)
				return clip;
			matched = clip == c;
		}
		return NULL;
	}
	clip_t* getPrevClip(clip_t* c) {
		clip_t* clipBefore = NULL;
		for (clip_t* clip : clips) {
			if (clip == c)
				return clipBefore;
			clipBefore = clip;
		}
		return NULL;
	}
	clip_t* getClipAt(time_t time) {
		for (clip_t* clip : clips) {
			if (clip->time == time)
				return clip;
		}
		return NULL;
	}
	void deleteEmptyClips();
};
struct clip_layout_t {
	clip_t* clip;
	tick_t time;
	tick_t len;
	tick_t offsetStart;
	tick_t loopLen;
	clip_layout_t(clip_t* clip, tick_t time, tick_t len, tick_t offsetStart, tick_t loopLen) :
			clip(clip),
			time(time), len(len), offsetStart(offsetStart), loopLen(loopLen) {
	}
};
struct trackstate_t {
	track_vector tracks;
	Cursor cursor;
	trackstate_t get() {
		trackstate_t t = *this;
		tracks.clear();
		return t;
	}
	~trackstate_t() {
	}
	void reset() {
		for (track_t* track : tracks) {
			deleteTrack(track, NULL);
		}
		tracks.clear();
	}
};
class tracklayout_t {
public:
	std::list<clip_layout_t> clips;
	tracklayout_t() {
	}
	tracklayout_t(const trackcontents_t &a) {
		copy(a);
	}
	tracklayout_t &operator =(const trackcontents_t &a) {
		copy(a);
		return *this;
	}
	void copy(const trackcontents_t &a) {
		clips.clear();
		for (clip_t* clip : a.clips) {
			clips.emplace_back(clip, clip->time, clip->len, clip->offsetStart, clip->loopLen);
		}
	}
	void apply(track_t* tr) {
		for (clip_layout_t& clipLayout : clips) {
			clip_t* clip = clipLayout.clip;
			clip->time = clipLayout.time;
			clip->len = clipLayout.len;
			clip->offsetStart = clipLayout.offsetStart;
			clip->loopLen = clipLayout.loopLen;
		}
	}
#define __CLPFLDEQAL(fldName) if (clip->fldName != clipLayout.fldName) return true;
	bool diff(track_t* tr) {
		for (clip_layout_t& clipLayout : clips) {
			clip_t* clip = clipLayout.clip;
			__CLPFLDEQAL(time)
			__CLPFLDEQAL(len)
			__CLPFLDEQAL(offsetStart)
			__CLPFLDEQAL(loopLen)
		}
		return false;
	}
};
struct tracksettings_t {
	int32_t idx = -1;
	String name = "INVALID";
	bool enabled = true;
	int type = -1; //CONST!
	int height = -1;
	int rgb = -1;
};

struct param_snapshot_t {
	int32_t idx;
	float val;
};
struct plugin_snapshot_t {
	bool present;
	int32_t slot;
	int32_t uId;
	String name;
	std::vector<uint8_t> dataChunk;
	std::vector<uint8_t> dataChunk2;
	std::vector<param_snapshot_t> params;
};
struct track_plugins_snapshot_t {
	float gain = 1.0f;
	std::vector<plugin_snapshot_t> plugins;
	track_plugins_snapshot_t() = default;
	track_plugins_snapshot_t(const track_t &a);
};
struct track_snapshot_t : public tracksettings_t {
	track_t* trackLoaded = NULL;
	track_plugins_snapshot_t plugins;
	std::vector<clip_t> clips;
	track_snapshot_t() = default;
	track_snapshot_t(track_t* track);
};
class track_t : public trackcontents_t, public tracksettings_t {
public:
	track_t(const track_t &a) : trackcontents_t() {
		trackcontents_t::deepcopy(a);
		copy(a);
	}
	track_t(const track_snapshot_t &a);
	track_t &operator =(const track_t &a) {
		trackcontents_t::deepcopy(a);
		copy(a);
		return *this;
	}
	track_t(int _type, String _name, bool state) {
		this->type = _type;
		this->name = _name;
		this->enabled = state;
		rgb = 0;
		height = 4;
	}
	std::vector<clip_t*>::iterator removeClip(clip_t* clip) override;
	void addClip(clip_t* clip) override;
	void addClipSort(clip_t* clip);
	void copy( const track_t &obj) {
		idx = obj.idx;
		name = obj.name;
		enabled = obj.enabled;
		type = obj.type;
		height = obj.height;
		rgb = obj.rgb;
	}
	void getNotesInRange(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, std::vector<note_t>& notes);
	void releaseTrackContent();
	gui_trackcontent* content = NULL;
	gui_trackmixer* mixer = NULL;
	track_plugins_t* audio = NULL;
	int scrolloffset = 0;
};
struct trackcontainer_snapshot_t;

class project_t;
class trackbasecontainer_t {
public:
	track_vector tracks;
	trackbasecontainer_t() = default;
	~trackbasecontainer_t();
	trackbasecontainer_t(const trackbasecontainer_t &a) = delete;
	trackbasecontainer_t &operator =(const trackbasecontainer_t &a) = delete;

	track_vector& vec() {
		return tracks;
	}

	size_t size() {
		return tracks.size();
	}

    track_vector::iterator begin() { return tracks.begin(); }
    track_vector::const_iterator begin() const { return tracks.cbegin(); }
    track_vector::iterator end() { return tracks.end(); }
    track_vector::const_iterator end() const { return tracks.cend(); }
    track_vector::reverse_iterator rbegin() { return tracks.rbegin(); }
    track_vector::reverse_iterator rend() { return tracks.rend(); }
    track_vector::reference back() { return tracks.back(); }
    track_vector::reference front() { return tracks.front(); }
};
class tracksubcontainer_t : public trackbasecontainer_t {
public:
	tracksubcontainer_t(trackbasecontainer_t *a = NULL) :
		trackbasecontainer_t()
	{

	}
	~tracksubcontainer_t() = default;
	tracksubcontainer_t(const tracksubcontainer_t &a) = delete;
	tracksubcontainer_t &operator =(const tracksubcontainer_t &a) = delete;
	void copyTo(trackcontainer_snapshot_t& out);
	void copyFrom(trackcontainer_snapshot_t& in);
	void loadPlugins(trackcontainer_snapshot_t& in);

};
struct trackcontainer_snapshot_t {
	std::vector<track_snapshot_t> tracks;
};

struct project_snapshot_t {
	trackcontainer_snapshot_t trackCtr;
	trackcontainer_snapshot_t trackReturnCtr;
	trackcontainer_snapshot_t trackMasterCtr;
	project_globals_t globals;
};
class trackallcontainer_t : public trackbasecontainer_t {
	friend class project_t;
	tracksubcontainer_t trackCtr;
	tracksubcontainer_t trackReturnCtr;
	tracksubcontainer_t trackMasterCtr;
	trackbasecontainer_t tracksBottom;
	tracksubcontainer_t* const trackTypeCtrs[3] = {&trackMasterCtr, &trackReturnCtr, &trackCtr};
public:
	trackallcontainer_t(trackbasecontainer_t *a = NULL) :
		trackbasecontainer_t()
	{

	}
	~trackallcontainer_t() = default;
	trackallcontainer_t(const trackallcontainer_t &a) = delete;
	trackallcontainer_t &operator =(const trackallcontainer_t &a) = delete;
	void clear() {
		trackReturnCtr.tracks.clear();
		trackMasterCtr.tracks.clear();
		tracksBottom.tracks.clear();
		trackCtr.tracks.clear();
		tracks.clear();
	}

	void addTrack(int trackInsertPos, track_t* newTrack);
	void removeTrack(track_t* track);
	void copyTo(project_snapshot_t& out);
	void copyFrom(project_snapshot_t& in);
	void copyTracks(int32_t trackBegin, int32_t trackLen, trackstate_t& _out);
	void loadPlugins(project_snapshot_t& project);



	int32_t clampTrackIdx(int32_t idx) const {
		return max(0, min((int32_t) this->tracks.size() - 1, idx));
	}

	bool validTrackIdx(int32_t idx) const {
		return idx >= 0 && idx < (int32_t) this->tracks.size();
	}

	void getTracks(const Cursor& cursor, std::vector<track_t*>& _out) const {
		for (track_t* t : tracks) {
			if (cursor.inTrackRange(t->idx)) {
				_out.push_back(t);
			}
		}
	}

	track_t* operator [](size_t i) {
		if (!validTrackIdx(i)) {
			return NULL;
		}
		return tracks[i];
	}


};
class project_t : public project_globals_t {
public:
	trackallcontainer_t trackList;
	tracksubcontainer_t& trackCtr;
	tracksubcontainer_t& trackReturnCtr;
	tracksubcontainer_t& trackMasterCtr;
	trackbasecontainer_t& tracksBottom;
	tracksubcontainer_t* const *trackTypeCtrs; //syntax win
	project_t() :
		trackCtr(trackList.trackCtr),
		trackReturnCtr(trackList.trackReturnCtr),
		trackMasterCtr(trackList.trackMasterCtr),
		tracksBottom(trackList.tracksBottom),
		trackTypeCtrs(trackList.trackTypeCtrs)
	{

	}
	void copyTo(project_snapshot_t& project) {
		trackList.copyTo(project);
		project.globals = *this;
	}
	void copyFrom(project_snapshot_t& project) {
		trackList.copyFrom(project);
		*this = project.globals;
	}
	void operator=(project_globals_t const & globals) {
		*static_cast<project_globals_t*>(this) = globals; //KILL ME
	}
};
class delete_cb {
public:
	virtual ~delete_cb() {
	}
	virtual void preClipDelete(clip_t* clip) = 0;
	virtual void preTrackDelete(track_t* clip) = 0;
};
