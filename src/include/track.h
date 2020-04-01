#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <list>
#include <deque>
#include "assert_dbg.h"
#include "exceptions.h"
#include "seq_time.h"
#include "cursor.h"
#include "note.h"
#include "clip.h"
#include "str_util.h"
#include "logging.h"
#include "project.h"
#include "automation.h"
#include "snapshot.h"
#include "host/daw_channel.h"
#include "track_types.h"


class track_t;
struct track_impl_t;
struct track_clipboard_t;
class trackdata_midi_t;

class gui_track;
class gui_track_subtrack;
class gui_track_automationlane;
class gui_track_controls;

class delete_cb;
using track_vector = std::vector<track_t*>;

void deleteTrackContents(trackdata_midi_t* tr, delete_cb *cb);
void releaseTrackResources(track_t* tr, delete_cb *cb);
void releaseClipResources(clip_t* cl, delete_cb *cb);

struct track_tree_pos_t {
	int32_t trackTypeCtr;
	track_t* parent;
	int32_t treeIdx;
};
inline const struct track_tree_pos_t TrackTreePosNULL() {
	return {-1, nullptr, -1};
}
class trackdata_midi_t {
public:
	friend void resizeOtherClips(trackdata_midi_t& midi, clip_t* clip);
	friend void copyClipsInRange(const trackdata_midi_t& in, track_clipboard_t& out, int32_t srcPos, int32_t dstPos, int32_t len);
	friend void cutIntersectingClips(trackdata_midi_t& midi, tick_t tickBegin, tick_t tickEnd, delete_cb *cb);
	friend void muteIntersectingClips(trackdata_midi_t& midi, tick_t tickBegin, tick_t tickEnd);

private:
	std::vector<clip_t*> clips;
public:
	trackdata_midi_t() {
	}
	trackdata_midi_t(const trackdata_midi_t &a) {
		deepcopy(a);
	}
	trackdata_midi_t &operator =(const trackdata_midi_t &a) {
		deepcopy(a);
		return *this;
	}
	void deepcopy( const trackdata_midi_t &obj) {
		//this shouldn't be down here
		for (clip_t* clip : obj.clips) {
			releaseClipResources(clip, nullptr);
			delete clip;
		}
		clips.clear();
		clips.reserve(obj.clips.size());
		for (clip_t* clip : obj.clips) {
			addClip(new clip_t(*clip));
		}
		sortClips();
	}
	~trackdata_midi_t() {
		dbgassert(clips.empty());
	}
	const std::vector<clip_t*>& getConstClips() const {
		return clips;
	}
	std::vector<clip_t*>& getClips() {
		return clips;
	}
	std::vector<clip_t*>::iterator removeClip(clip_t* clip);
	void addClip(clip_t* clip) {
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
				for (int i = 0; i < (int)clips.size(); i++) {
					my_printf("clip[%d] = %d\n", i, clips[i]->start());
				}
			}
			dbgassert(clips[0]->start() < clips[1]->start());
		}
	}
	void addClipSort(clip_t* clip) {
		addClip(clip);
		sortClips();
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
	void deleteEmptyClips(delete_cb *cb);
	void deleteClips(delete_cb *cb);
	void getClipsInRange(tick_t start, tick_t end, std::vector<clip_t*>& clips);
	void getNotesInRange(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, std::vector<note_t>& notes);
};
struct clip_layout_t {
	clip_t* clip;
	tick_t time;
	tick_t len;
	tick_t lenSamples;
	tick_t offsetStart;
	int32_t offsetSamples;
	tick_t loopLen;
	clip_layout_t(clip_t* clip, tick_t time, tick_t len, tick_t lenSamples, tick_t offsetStart, int32_t offsetSamples, tick_t loopLen) :
			clip(clip),
			time(time), len(len), lenSamples(lenSamples), offsetStart(offsetStart), offsetSamples(offsetSamples), loopLen(loopLen) {
	}
};
struct track_snapshot_t;
struct trackstate_t {
//	track_vector tracks;
	std::vector<track_snapshot_t*> tracks;
	DAW::Cursor cursor;
	trackstate_t copy();
	trackstate_t() = default;
	trackstate_t(const trackstate_t& ref) = delete;
	trackstate_t& operator=(const trackstate_t& ref) = delete;
	trackstate_t(trackstate_t&& ref) noexcept = default;
	trackstate_t& operator=(trackstate_t&& ref) noexcept = default;
	~trackstate_t();
	void reset();
};
class tracklayout_t {
public:
	std::list<clip_layout_t> clips;
	tracklayout_t() {
	}
	tracklayout_t(const trackdata_midi_t &a) {
		copy(a);
	}
	tracklayout_t &operator =(const trackdata_midi_t &a) {
		copy(a);
		return *this;
	}
	void copy(const trackdata_midi_t &a) {
		clips.clear();
		for (clip_t* clip : a.getConstClips()) {
			clips.emplace_back(clip, clip->time, clip->len, clip->lenSamples, clip->offsetStart, clip->offsetSamples, clip->loopLen);
		}
	}
	void apply(track_t* tr) {
		for (clip_layout_t& clipLayout : clips) {
			clip_t* clip = clipLayout.clip;
			clip->time = clipLayout.time;
			clip->len = clipLayout.len;
			clip->lenSamples = clipLayout.lenSamples;
			clip->offsetStart = clipLayout.offsetStart;
			clip->offsetSamples = clipLayout.offsetSamples;
			clip->loopLen = clipLayout.loopLen;
		}
	}
#define __CLPFLDEQAL(fldName) if (clip->fldName != clipLayout.fldName) return true;
	bool diff(track_t* tr) {
		for (clip_layout_t& clipLayout : clips) {
			clip_t* clip = clipLayout.clip;
			if (clip->getLen() != clipLayout.len) return true;
			__CLPFLDEQAL(time)
			__CLPFLDEQAL(len)
			__CLPFLDEQAL(lenSamples)
			__CLPFLDEQAL(offsetStart)
			__CLPFLDEQAL(offsetSamples)
			__CLPFLDEQAL(loopLen)
		}
		return false;
	}
};
struct tracksettings_t {
	String name = "INVALID";
	int type = -1; //CONST!
	int rgb = -1;
};
struct tracklayout_settings_t {
	int height = 4;
	bool hideTrack = false;
	bool hideSubtracks = false;
};

class track_t : public tracksettings_t {
	trackdata_midi_t midi;
public:
#ifndef NDEBUG
	//helper indicator in gdb.
	//gdb cannot display std::string when built without clib-debug flag (SLOW)
	const char* szName = NULL;
#endif
	trackdata_midi_t& getMidi() {
		return midi;
	}
	const trackdata_midi_t& getConstMidi() {
		return midi;
	}
	const trackdata_midi_t& getConstMidi() const {
		return midi;
	}
	tick_minmax_t getMinMaxEvents() {
		tick_t evtMin = INVALID_TICK;
		tick_t evtMax = INVALID_TICK;
		if (type == TRACK_TYPE_MIDI) {
			auto minMax = midi.getMinMax();
			if (minMax.first) {
				evtMin = minMax.first->start();
				evtMax = minMax.second->end();
			}
		}
		//TODO: go thru automation
		return {evtMin, evtMax};
	}
	track_t(const track_t &a) = delete;
//	: localIdx(a.localIdx) {
//		dbgassert(midi.getConstClips().empty());
//		copy(a);
//		content = NULL;
//		mixer = NULL;
//		audio = NULL;
//	}
	track_t(const track_snapshot_t &a);
	track_t &operator =(const track_snapshot_t &a);
	track_t &operator =(const track_t &a) = delete;
	track_impl_t* getStage() {
		return this->audio;
	}
//	{
//		dbgassert(midi.getConstClips().empty());
//		midi.deepcopy(a.midi);
//		copy(a);
//		return *this;
//	}
	track_t(int _type, String _name, bool state) {
		this->type = _type;
		this->name = _name;
		rgb = 0;
//		height = 4;
#ifndef NDEBUG
		this->szName = this->name.c_str();
#endif
	}
	void copy( const track_t &obj) {
//		idx = obj.idx;
		name = obj.name;
		type = obj.type;
//		height = obj.height;
		rgb = obj.rgb;
		scrolloffset = 0;
#ifndef NDEBUG
		this->szName = this->name.c_str();
#endif
	}
	void releaseTrackContent();
//	void loadPluginAutomationParameters(const track_impl_snapshot_t& snap);
	void loadSnapshot(const track_snapshot_t& snap);
	void loadSubtrackLayout(const track_snapshot_t& snap);
	bool validSubtrack(int32_t idx) const {
		return idx >= 0 && idx < (int32_t)subtracks.size();
	}
	int32_t idx = -1; // global flat idx (skips invisible tracks)
	int32_t childIdxTree = -1; // index in parent child list (position in parents child list)
	int32_t localIdxFlat = -1; // index in type-container (midi/return/master group)
	gui_track* content = nullptr;
//	std::vector<gui_track_automationlane*> automationLanes;
	std::vector<gui_track_subtrack*> subtracks;
	track_t* parent = nullptr;
	std::vector<track_t*> children;
	track_impl_t* audio = nullptr;
	int scrolloffset = 0;
	void addChild(track_t* track) {
		dbgassert(!track->parent);
		dbgassert(!STL_CONTAINS(children, track));
//		children.push_back(track);
		if (track->childIdxTree < 0 || track->childIdxTree >= (int)children.size()) {
			children.push_back(track);
			track->childIdxTree = children.size() - 1;
		} else {
			children.insert(children.begin() + static_cast<size_t>(track->childIdxTree), track);
			int32_t childIdx = 0;
			for (auto track : children) {
				track->childIdxTree = childIdx++;
			}
		}
		track->parent = this;
	}
	void removeChild(track_t* track) {
		dbgassert(track->parent == this);
		dbgassert(STL_CONTAINS(children, track));
		dbgassert(track->childIdxTree < children.size());
		dbgassert(children.at(track->childIdxTree) == track);
		children.erase(std::remove(children.begin(), children.end(), track));
		int32_t childIdx = 0;
		for (auto track : children) {
			track->childIdxTree = childIdx++;
		}
		track->childIdxTree = -1;
		track->parent = nullptr;
	}
	int32_t getChildLvl() const {
		int32_t lvl = 0;
		auto p = parent;
		while (p) {
			lvl++;
			p = p->parent;
		}
		return lvl;
	}
};
struct trackcontainer_snapshot_t;

class project_t;
class trackbasecontainer_t {
public:
	/** tracks are only ordered on track-type specific containers **/
	track_vector tracksFlat;
	track_vector tracksTree;
//	track_vector tracksVisibleFlat;
	trackbasecontainer_t() = default;
	~trackbasecontainer_t();
	trackbasecontainer_t(const trackbasecontainer_t &a) = delete;
	trackbasecontainer_t &operator =(const trackbasecontainer_t &a) = delete;

	const track_vector& getTracksFlatVec() {
		return tracksFlat;
	}
	const track_vector& getTracksTreeVec() {
		return tracksTree;
	}
//	const track_vector& getTracksVisibleFlatVec() {
//		return tracksVisibleFlat;
//	}

	size_t size() const {
		return tracksFlat.size();
	}

	bool empty() const {
		return tracksTree.empty();
	}

    track_vector::iterator begin() { return tracksFlat.begin(); }
    track_vector::const_iterator begin() const { return tracksFlat.cbegin(); }
    track_vector::const_iterator cbegin() const { return tracksFlat.cbegin(); }
    track_vector::iterator end() { return tracksFlat.end(); }
    track_vector::const_iterator end() const { return tracksFlat.cend(); }
    track_vector::const_iterator cend() const { return tracksFlat.cend(); }
    track_vector::reverse_iterator rbegin() { return tracksFlat.rbegin(); }
    track_vector::reverse_iterator rend() { return tracksFlat.rend(); }
    track_vector::const_reverse_iterator crbegin() const { return tracksFlat.crbegin(); }
    track_vector::const_reverse_iterator crend() const { return tracksFlat.crend(); }
    track_vector::reference back() { return tracksFlat.back(); }
    track_vector::reference front() { return tracksFlat.front(); }
	void clear() {
		tracksFlat.clear();
		tracksTree.clear();
//		tracksVisibleFlat.clear();
	}
	track_t* operator [](const size_t i) {
		if (i >= tracksFlat.size()) {
			return NULL;
		}
		return tracksFlat[i];
	}
	const track_t* at(const size_t i) const {
		if (i >= tracksFlat.size()) {
			return NULL;
		}
		return tracksFlat.at(i);
	}

//	void updateTracksVisible() {
//		tracksVisibleFlat.clear();
//		/** turn tree structure into linear pointer array with trackTop at the beginning and the deepest child at the end **/
//		track_vector vecNewTracksFlat;
//		std::deque<track_t*> stack;
//		stack.insert(stack.begin(), tracksTree.cbegin(), tracksTree.cend());
//		while (!stack.empty()) {
//			track_t* current = stack.front();
//			stack.pop_front();
//			if (!current->hideTrack && current->children.size())
//				stack.insert(stack.begin(), current->children.cbegin(), current->children.cend());
//			vecNewTracksFlat.push_back(current);
//		}
//
//		tracksVisibleFlat = vecNewTracksFlat;
//	}

	void repopulateFlatTracks() {

		tracksFlat.clear();
		std::deque<track_t*> stack;
		stack.insert(stack.begin(), tracksTree.cbegin(), tracksTree.cend());
		while (!stack.empty()) {
			auto* node = stack.front();
			stack.pop_front();
			if (node->children.size()) {
				stack.insert(stack.begin(), node->children.cbegin(), node->children.cend());
			}
			tracksFlat.push_back(node);
		}
//		updateTracksVisible(); // keep this seperate
	}
	void setFromVectorTree(const track_vector& vecTree) {
		tracksTree = vecTree;
		repopulateFlatTracks();
	}
	void add(track_t* trackAdd) {
		dbgassert(trackAdd);
		dbgassert(trackAdd->childIdxTree >= -1);
		//TODO: check that track isn't contained already;
		if (!trackAdd->parent) {
			if (trackAdd->childIdxTree < 0 || trackAdd->childIdxTree >= (int)tracksTree.size()) {
				tracksTree.push_back(trackAdd);
				trackAdd->childIdxTree = tracksTree.size() - 1;
			} else {
				tracksTree.insert(tracksTree.begin() + static_cast<size_t>(trackAdd->childIdxTree), trackAdd);
				int32_t childIdx = 0;
				for (auto track : tracksTree) {
					track->childIdxTree = childIdx++;
				}
			}
		} else {
			// a child track is expected to get added to its parent by the caller
			const bool trackInParent = STL_CONTAINS(trackAdd->parent->children, trackAdd);
			dbgassert(trackInParent);
			if (!trackInParent) {
				throw applogicexception("failed adding track. track->parent != null but track not parents list");
			}

		}
		repopulateFlatTracks();
	}
	bool remove(track_t* trackRemove) {
		dbgassert(trackRemove);
		dbgassert(trackRemove->childIdxTree >= 0);

		if (trackRemove->parent) {
			trackRemove->parent->removeChild(trackRemove);
		} else {
			dbgassert(trackRemove->childIdxTree < tracksTree.size());
			auto itRemoveTreeRoot = std::find(tracksTree.cbegin(), tracksTree.cend(), trackRemove);
			if (itRemoveTreeRoot != tracksTree.cend()) {
				tracksTree.erase(itRemoveTreeRoot);
			}
			int32_t childIdx = 0;
			for (auto track : tracksTree) {
				track->childIdxTree = childIdx++;
			}
		}
		repopulateFlatTracks();
		return true;
	}
};
class trackallcontainer_t;
class trackcontainer_tracktype_t : public trackbasecontainer_t {
public:
	trackcontainer_tracktype_t(trackbasecontainer_t *a = NULL) :
		trackbasecontainer_t()
	{

	}
	~trackcontainer_tracktype_t() = default;
	trackcontainer_tracktype_t(const trackcontainer_tracktype_t &a) = delete;
	trackcontainer_tracktype_t &operator =(const trackcontainer_tracktype_t &a) = delete;
	void copyTo(trackcontainer_snapshot_t& out);
	void copyFrom(trackcontainer_snapshot_t& in);
	void loadPlugins(trackcontainer_snapshot_t& in);

};
struct project_snapshot_t;
class trackallcontainer_t {
	friend class project_t;
	trackbasecontainer_t trackAllCtr;
	trackcontainer_tracktype_t trackMidiAudioCtr;
	trackcontainer_tracktype_t trackReturnCtr;
	trackcontainer_tracktype_t trackMasterCtr;
	trackbasecontainer_t tracksBottom;
	trackcontainer_tracktype_t* const trackTypeCtrs[4] = {&trackMasterCtr, &trackReturnCtr, &trackMidiAudioCtr, &trackMidiAudioCtr};
	const std::array<trackcontainer_tracktype_t*,3> trackTypeUniqueCtrs = {&trackMasterCtr, &trackReturnCtr, &trackMidiAudioCtr};
public:
	void rebuildTrackList();
	trackallcontainer_t()
//:   trackbasecontainer_t()
	{

	}
	~trackallcontainer_t() = default;
	trackallcontainer_t(const trackallcontainer_t &a) = delete;
	trackallcontainer_t &operator =(const trackallcontainer_t &a) = delete;
	void clear() {
		trackReturnCtr.clear();
		trackMasterCtr.clear();
		tracksBottom.clear();
		trackMidiAudioCtr.clear();
		trackAllCtr.clear();
	}

	/**
	 * inserts a track at position trackInsertPos of its track-type specific container
	 * children have to have to set their parent reference outside
	 */
	void addTrack(int trackInsertPos, track_t* newTrack);
	void removeTrack(track_t* track);
	void moveTrack(track_t* track, int32_t newIdx);
	bool moveTracks(const std::vector<track_t*>& tracks, /*const*/ track_tree_pos_t& treePos);
	void copyTo(project_snapshot_t& out);
	void copyFrom(project_snapshot_t& in);
	void copyTracks(int32_t trackBegin, int32_t trackLen, trackstate_t& _out);
	void loadPlugins(project_snapshot_t& project);
	void loadSubtrackLayouts(project_snapshot_t& project);
	void checkConsistency();


	int32_t clampTrackIdx(int32_t idx) const {
		return math::max(0, math::min((int32_t) trackAllCtr.size() - 1, idx));
	}

	bool validTrackIdx(int32_t idx) const {
		return idx >= 0 && idx < (int32_t) trackAllCtr.size();
	}
	bool validTrackTypeIdx(int32_t type, int32_t idx) const;
	track_t* getTrackTypeIdx(int32_t type, int32_t idx);

	void getTracks(const DAW::Cursor& cursor, std::vector<track_t*>& _out) const {
		for (track_t* t : trackAllCtr) {
			if (cursor.inTrackRange(t->idx)) {
				_out.push_back(t);
			}
		}
	}
	size_t size() const {
		return trackAllCtr.size();
	}

	bool empty() const {
		return trackAllCtr.empty();
	}

    track_vector::iterator begin() { return trackAllCtr.begin(); }
    track_vector::const_iterator begin() const { return trackAllCtr.cbegin(); }
    track_vector::iterator end() { return trackAllCtr.end(); }
    track_vector::const_iterator end() const { return trackAllCtr.cend(); }
    track_vector::reverse_iterator rbegin() { return trackAllCtr.rbegin(); }
    track_vector::reverse_iterator rend() { return trackAllCtr.rend(); }
    track_vector::const_reverse_iterator crbegin() const { return trackAllCtr.crbegin(); }
    track_vector::const_reverse_iterator crend() const { return trackAllCtr.crend(); }
    track_vector::reference back() { return trackAllCtr.back(); }
    track_vector::reference front() { return trackAllCtr.front(); }
    track_vector::const_iterator cbegin() const { return trackAllCtr.cbegin(); }
    track_vector::const_iterator cend() const { return trackAllCtr.cend(); }
    track_vector::const_iterator cbeginTree() const { return trackAllCtr.tracksTree.cbegin(); }
    track_vector::const_iterator cendTree() const { return trackAllCtr.tracksTree.cend(); }

	track_t* operator [](size_t i) {
		if (!validTrackIdx(i)) {
			return NULL;
		}
		return trackAllCtr[i];
	}
	const track_t* at(const size_t i) const {
		if (!validTrackIdx(i)) {
			return NULL;
		}
		return trackAllCtr.at(i);
	}


    track_vector getMidiAudioTracksFlatVec() const { return trackMidiAudioCtr.tracksFlat; }
    track_vector getAllTracksFlatVec() const { return trackAllCtr.tracksFlat; }
    track_vector getAllTracksTreeVec() const { return trackAllCtr.tracksTree; }
};
struct project_layout_t {
	layout_grid_t layoutGrid;
	float scrollOffsetX;
};
class project_t : public project_globals_t {
public:
	trackallcontainer_t trackList;
	trackcontainer_tracktype_t& trackMidiAudioCtr;
	trackcontainer_tracktype_t& trackReturnCtr;
	trackcontainer_tracktype_t& trackMasterCtr;
	trackbasecontainer_t& tracksBottom;
	trackcontainer_tracktype_t* const *trackTypeCtrs;
	const std::array<trackcontainer_tracktype_t*,3>& trackTypeUniqueCtrs;
//	track_vector& tracksVisibleFlat;
	project_t() :
		trackList(),
		trackMidiAudioCtr(trackList.trackMidiAudioCtr),
		trackReturnCtr(trackList.trackReturnCtr),
		trackMasterCtr(trackList.trackMasterCtr),
		tracksBottom(trackList.tracksBottom),
		trackTypeCtrs(trackList.trackTypeCtrs),
		trackTypeUniqueCtrs(trackList.trackTypeUniqueCtrs)//,
//		tracksVisibleFlat(trackList.trackAllCtr.tracksVisibleFlat)
	{

	}
	void copyTo(project_snapshot_t& project);
	void copyFrom(project_snapshot_t& project);
	void operator=(project_globals_t const & globals) {
		*static_cast<project_globals_t*>(this) = globals;
	}
	const track_vector& getTracksFlatVec() {
		return trackList.trackAllCtr.getTracksFlatVec();
	}
};
class delete_cb {
public:
	virtual ~delete_cb() {
	}
	virtual void preClipDelete(clip_t* clip) = 0;
	virtual void preTrackDelete(track_t* clip) = 0;
};

void resizeOtherClips(trackdata_midi_t& midi, clip_t* clip);
