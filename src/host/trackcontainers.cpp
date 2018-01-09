#include <algorithm>


#include "exceptions.h"
#include "seq_util.h"
#include "seq_time.h"


#include "track.h"
#include "track_impl.h"
#include "vst_host.h"

#include "leak_detect.h"

trackbasecontainer_t::~trackbasecontainer_t() {
	for (auto it = tracks.begin(); it != tracks.end(); it++) {
		track_t* tr = *it;
		deleteTrack(tr, NULL);
	}
	tracks.clear();
}
void trackallcontainer_t::addTrack(int trackInsertPos, track_t* newTrack) {
	int numTracks = (int) this->tracks.size();
	auto it = std::find(tracks.begin(), tracks.end(), newTrack);
	if (it != tracks.end()) {
		assert(0);
		throw applogicexception("attempt to add track twice");
	}
	if (trackInsertPos < 0 || trackInsertPos >= numTracks) {
		tracks.push_back(newTrack);
	} else {
		tracks.insert(tracks.begin() + trackInsertPos, newTrack);
	}
	vsthost* host = vsthost::getInstance();
	newTrack->audio = host->createAudio(newTrack);
	tracksubcontainer_t* subCtr = trackTypeCtrs[newTrack->type];
	track_vector& vec = subCtr->tracks;
	vec.push_back(newTrack);
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

	std::sort(tracks.begin(), tracks.end(), [](track_t* const & a, track_t* const & b) {
		return a->idx < b->idx;
	});
	tracksBottom.tracks.clear();
	addAll(tracksBottom.tracks, trackReturnCtr.tracks);
	addAll(tracksBottom.tracks, trackMasterCtr.tracks);
	vsthost* host = vsthost::getInstance();
	for (track_t* t : tracks) {
		t->audio = host->createAudio(t);
	}
}
void trackallcontainer_t::loadPlugins(project_snapshot_t& project) {
	trackCtr.loadPlugins(this, project.trackCtr);
	trackReturnCtr.loadPlugins(this, project.trackReturnCtr);
	trackMasterCtr.loadPlugins(this, project.trackMasterCtr);
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
	int32_t idx = 0;
	for (track_t* track : tracks) {
		track_snapshot_t trackCopy(track, true);
		out.tracks.push_back(std::move(trackCopy));
		trackCopy.idx = idx++;
	}
}
void tracksubcontainer_t::copyFrom(trackcontainer_snapshot_t& in) {
	assert(tracks.empty());
	for (track_snapshot_t& trackStatic : in.tracks) {
		track_t* trackCopy = new track_t(trackStatic);
		trackStatic.trackLoaded = trackCopy;
		this->tracks.push_back(trackCopy);
	}
	std::sort(tracks.begin(), tracks.end(), [](track_t* const & a, track_t* const & b) {
		return a->idx < b->idx;
	});
}
void tracksubcontainer_t::loadPlugins(trackallcontainer_t* all, trackcontainer_snapshot_t& in) {
	for (track_snapshot_t& trackStatic : in.tracks) {
		track_t* trackLoaded = trackStatic.trackLoaded;
		auto audio = trackLoaded->audio;
		assert(audio);
		const track_impl_snapshot_t& trackImplSnapshot = trackStatic.plugins;
		audio->mixer.loadSnapshot(trackImplSnapshot.trackParams);
		const std::vector<plugin_snapshot_t>& trPluginList = trackImplSnapshot.plugins;
		audio->loadPlugins(trPluginList);
		const std::vector<automationlane_snapshot_t>& atl = trackStatic.automationLanes;
		trackLoaded->subtracks.clear();
		bool showSubtracks = !trackLoaded->hideAutomation && !trackLoaded->hideTrack;
		if (!showSubtracks) {
			audio->atl = atl;
			audio->atlStored = true;
		} else {
			audio->atlStored = false;
			audio->atl.clear();
			audio->loadAutomationLanes(atl);
		}
	}
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

