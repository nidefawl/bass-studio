#include <algorithm>


#include "exceptions.h"
#include "seq_util.h"
#include "seq_time.h"


#include "track.h"
#include "track_impl.h"
#include "vst_host.h"

#include "leak_detect.h"
#include "mainctrl.h"
#include "../threads/playbackthread.h"

trackbasecontainer_t::~trackbasecontainer_t() {
	for (auto it = tracks.begin(); it != tracks.end(); it++) {
		track_t* tr = *it;
		deleteTrack(tr, NULL);
	}
	tracks.clear();
}
void trackallcontainer_t::addTrack(int trackInsertPos, track_t* newTrack) {
	auto it = std::find(tracks.begin(), tracks.end(), newTrack);
	if (it != tracks.end()) {
		assert(0);
		throw applogicexception("attempt to add track twice");
	}
	tracks.push_back(newTrack);
	vsthost* host = vsthost::getInstance();
	host->createAudio(newTrack);
	tracksubcontainer_t* subCtr = trackTypeCtrs[newTrack->type];
	track_vector& vec = subCtr->tracks;
	if (trackInsertPos < 0 || trackInsertPos >= (int)vec.size()) {
		vec.push_back(newTrack);
	} else {
		vec.insert(vec.begin() + trackInsertPos, newTrack);
	}
	int32_t locIdx = 0;
	for (track_t* t : vec) {
		t->localIdx = locIdx++;
	}
	int32_t idx = 0;
	for (track_t* t : tracks) {
		t->idx = idx++;
	}
	tracksBottom.tracks.clear();
	addAll(tracksBottom.tracks, trackReturnCtr.tracks);
	addAll(tracksBottom.tracks, trackMasterCtr.tracks);
}
void trackallcontainer_t::removeTrack(track_t* track) {
	if (!removeEntry(tracks, track)) {
		assert(0);
		throw applogicexception("trackcontainer_t - attempt to remove non-present element");
	}
	tracksubcontainer_t* subCtr = trackTypeCtrs[track->type];
	track_vector& vec = subCtr->tracks;
	removeEntry(vec, track);
	int32_t locIdx = 0;
	for (track_t* t : vec) {
		t->localIdx = locIdx++;
	}
	int32_t idx = 0;
	for (track_t* t : tracks) {
		t->idx = idx++;
	}
	tracksBottom.tracks.clear();
	addAll(tracksBottom.tracks, trackReturnCtr.tracks);
	addAll(tracksBottom.tracks, trackMasterCtr.tracks);
	vsthost* host = vsthost::getInstance();
	host->releaseAudio(track);
}

void trackallcontainer_t::moveTrack(track_t* track, int32_t dst) {
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	tracksubcontainer_t* subCtr = trackTypeCtrs[track->type];
	int32_t src = indexOfCtr(subCtr->tracks, track);
	if ((int32_t)subCtr->tracks.size() == dst) dst--;
	assert(src >= 0 && dst >= 0);
	assert(src != dst);

	track_vector curOrder = subCtr->tracks;
	track_vector newOrder;
	newOrder.resize(curOrder.size());
	auto itIn = curOrder.cbegin();
	auto itOut = newOrder.begin();
	for (;itOut!=newOrder.cend();) {
		if (curOrder.cbegin()+src == itIn) {
			itIn++;
		} else if (newOrder.cbegin()+dst == itOut) {
			*itOut++ = curOrder[src];
		} else {
			*itOut++ = *itIn++;
		}
	}
	subCtr->tracks = (newOrder);
	int32_t locIdx = 0;
	for (track_t* t : subCtr->tracks) {
		t->localIdx = locIdx++;
	}
	int32_t idx = 0;
	for (track_t* t : tracks) {
		t->idx = idx++;
	}
	tracksBottom.tracks.clear();
	addAll(tracksBottom.tracks, trackReturnCtr.tracks);
	addAll(tracksBottom.tracks, trackMasterCtr.tracks);
}
void trackallcontainer_t::copyTo(project_snapshot_t& project) {
	for (track_t* t : *this) {
		my_printf("TRACK[%d] = %s\n", t->idx, StringAsCStr(t->name));
	}
	trackCtr.copyTo(project.trackCtr);
	trackMasterCtr.copyTo(project.trackMasterCtr);
	trackReturnCtr.copyTo(project.trackReturnCtr);
}
void trackallcontainer_t::copyFrom(project_snapshot_t& project) {
	my_printf("project.tracks: midi: %d returN: %d master: %d\n",
			project.trackCtr.tracks.size(),
			project.trackReturnCtr.tracks.size(),
			project.trackMasterCtr.tracks.size());
	assert(tracks.empty());
	trackCtr.copyFrom(project.trackCtr);
	assert(trackCtr.size()==project.trackCtr.tracks.size());

	trackReturnCtr.copyFrom(project.trackReturnCtr);
	assert(trackReturnCtr.size()==project.trackReturnCtr.tracks.size());

	trackMasterCtr.copyFrom(project.trackMasterCtr);
	assert(trackMasterCtr.size()==project.trackMasterCtr.tracks.size());


	addAll(tracks, trackCtr.tracks);
	addAll(tracks, trackReturnCtr.tracks);
	addAll(tracks, trackMasterCtr.tracks);
	int32_t idx = 0;
	for (track_t* track : tracks) {
		track->idx = idx++;
	}
	assert(tracks.size()==(project.trackCtr.tracks.size()+project.trackMasterCtr.tracks.size()+project.trackReturnCtr.tracks.size()));

	tracksBottom.tracks.clear();
	addAll(tracksBottom.tracks, trackReturnCtr.tracks);
	addAll(tracksBottom.tracks, trackMasterCtr.tracks);
	vsthost* host = vsthost::getInstance();
	for (track_t* t : tracks) {
		host->createAudio(t);
	}
}
void trackallcontainer_t::loadPlugins(project_snapshot_t& project) {
	trackCtr.loadPlugins(project.trackCtr);
	trackReturnCtr.loadPlugins(project.trackReturnCtr);
	trackMasterCtr.loadPlugins(project.trackMasterCtr);
}
void trackallcontainer_t::copyTracks(int32_t trackBegin, int32_t trackEnd, trackstate_t& _out) {
	_out.reset();
	for (track_t* t: tracks) {
		if (t->idx >= trackBegin && t->idx <= trackEnd) {
			my_printf("copy track %d\n", t->idx);
			track_snapshot_t* trackCopy = new track_snapshot_t(t, false);
			_out.tracks.push_back(trackCopy);
		} else {

			my_printf("NOT copy track %d\n", t->idx);
		}
	}
}
void tracksubcontainer_t::copyTo(trackcontainer_snapshot_t& out) {
	out.tracks.reserve(tracks.size());
	for (track_t* track : tracks) {
		track_snapshot_t trackCopy(track, true);
		out.tracks.push_back(std::move(trackCopy));
	}
}
void tracksubcontainer_t::copyFrom(trackcontainer_snapshot_t& in) {
	assert(tracks.empty());
	bool reassignIdx = false;
	for (track_snapshot_t& trackStatic : in.tracks) {
		track_t* trackCopy = new track_t(trackStatic);
		trackStatic.trackLoaded = trackCopy;
		reassignIdx |= trackCopy->localIdx < 0;
		this->tracks.push_back(trackCopy);
	}
	if (reassignIdx) {
		int32_t idx = 0;
		for (track_t* tr2 : this->tracks) {
			tr2->localIdx = idx++;
		}

	} else {
		std::sort(tracks.begin(), tracks.end(), [](track_t* const & a, track_t* const & b) {
			return a->localIdx < b->localIdx;
		});
	}
}
void tracksubcontainer_t::loadPlugins(trackcontainer_snapshot_t& in) {
	for (track_snapshot_t& trackStatic : in.tracks) {
		track_t* trackLoaded = trackStatic.trackLoaded;
		trackLoaded->loadSnapshot(trackStatic);
	}
}
bool trackallcontainer_t::validTrackTypeIdx(int32_t type, int32_t idx) const {
	if (type >= TRACK_TYPE_MASTER && type <= TRACK_TYPE_AUDIO) {
		const tracksubcontainer_t* trackTypeCtr = trackTypeCtrs[type];
		return idx >= 0 && idx < (int32_t) trackTypeCtr->size();
	}
	return false;
}
track_t* trackallcontainer_t::getTrackTypeIdx(int32_t type, int32_t idx) {
	track_vector& vec = trackTypeCtrs[type]->tracks;
	return vec[idx];
}

trackstate_t::~trackstate_t() {
	for (track_snapshot_t* track : tracks) {
		delete track;
	}
}
void trackstate_t::reset() {
	for (track_snapshot_t* track : tracks) {
		delete track;
	}
	tracks.clear();
}
trackstate_t trackstate_t::copy() {
	trackstate_t t;
	for (track_snapshot_t* track : tracks) {
		track_snapshot_t* trackCopy = new track_snapshot_t(*track);
		t.tracks.push_back(trackCopy);
	}
	tracks.clear();
	return t;
}

