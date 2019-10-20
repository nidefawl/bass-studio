#include <algorithm>


#include "exceptions.h"
#include "seq_util.h"
#include "seq_time.h"
#include "str_util.h"


#include "track.h"
#include "track_impl.h"
#include "vst_host.h"

#include "mainctrl.h"
#include "../threads/playbackthread.h"

trackbasecontainer_t::~trackbasecontainer_t() {
	for (auto track : tracks) {
		releaseTrackResources(track, NULL);
	}
	for (auto track : tracks) {
		delete track;
	}
	tracks.clear();
}
void fn(std::vector<track_t*>& vecTracks, track_t* t) {
	vecTracks.push_back(t);
	for (track_t* child : t->children) {
		fn(vecTracks, child);
	}
}
void assertUniqueEntries(const track_vector& vector) {
	track_vector tracksCopy = vector;
	auto it = std::unique( tracksCopy.begin(), tracksCopy.end() );
	bool wasUnique = (it == tracksCopy.end() );
	dbgassert(wasUnique);
}
void trackallcontainer_t::checkConsistency() {
	assertUniqueEntries(tracks);
	assertUniqueEntries(tracksRoot);
	assertUniqueEntries(trackCtr.tracks);
	assertUniqueEntries(trackReturnCtr.tracks);
	assertUniqueEntries(trackMasterCtr.tracks);
	assertUniqueEntries(tracksBottom.tracks);

	for (track_t* trackTop : tracksRoot) {
		dbgassert(!trackTop->parent);
	}

	track_vector allTracksParent;
	size_t numTracksRoot = 0;
	for (track_t* trackTop : tracks) {
		if (!trackTop->parent) {
			dbgassert(STL_CONTAINS(tracksRoot, trackTop));
			numTracksRoot++;
		} else {
			allTracksParent.push_back(trackTop);
		}
		for (track_t* trackChild : trackTop->children) {
			dbgassert(trackChild->parent == trackTop);
		}
	}
	dbgassert(numTracksRoot == tracksRoot.size());
	assertUniqueEntries(allTracksParent);

	track_vector newTracks;
	std::deque<track_t*> stack;
	for (track_t* trackTop : tracksRoot) {
		dbgassert(stack.empty());
		stack.push_back(trackTop);
		while (!stack.empty()) {
			track_t* current = stack.front();
			stack.pop_front();
			if (current->children.size())
				stack.insert(stack.begin(), current->children.cbegin(), current->children.cend());
			newTracks.push_back(current);
		}
	}
	dbgassert(newTracks == tracks);


	// assert tracks are stored in correct
	int32_t locIdx = 0;
	for (track_t* t : trackCtr) {
		dbgassert(t->localIdx == locIdx);
		locIdx++;
	}
	locIdx = 0;
	for (track_t* t : trackReturnCtr) {
		dbgassert(t->localIdx == locIdx);
		locIdx++;
	}
	locIdx = 0;
	for (track_t* t : trackMasterCtr) {
		dbgassert(t->localIdx == locIdx);
		locIdx++;
	}

	locIdx = 0;
	for (track_t* t : trackCtr) {
		dbgassert(t->idx == locIdx);
		locIdx++;
	}
	for (track_t* t : trackReturnCtr) {
		dbgassert(t->idx == locIdx);
		locIdx++;
	}
	for (track_t* t : trackMasterCtr) {
		dbgassert(t->idx == locIdx);
		locIdx++;
	}
}
void trackallcontainer_t::addTrack(int trackInsertPos, track_t* newTrack) {
	auto it = std::find(tracks.begin(), tracks.end(), newTrack);
	if (it != tracks.end()) {
		dbgassert(0);
		throw applogicexception("attempt to add track twice");
	}

	// trackInsertPos is tracktype-container index
	trackcontainer_tracktype_t* subCtr = trackTypeCtrs[newTrack->type];

	// only add root tracks (nodes with no parent) to root list
	// children have to have to set their parent reference outside
	if (!newTrack->parent) {
		// insert in correct position on trackTypeCtr
		track_vector& vec = subCtr->tracks;
		if (trackInsertPos < 0 || trackInsertPos >= (int)vec.size()) {
			vec.push_back(newTrack);
		} else {
			vec.insert(vec.begin() + trackInsertPos, newTrack);
		}

		// reassign local track indices in correct order
		int32_t locIdx = 0;
		for (track_t* t : vec) {
			t->localIdx = locIdx++;
		}
	} else {
		log_printf("adding track with parent %X\n", reinterpret_cast<int64_t>(newTrack->parent));
	}
	rebuildTrackList();
	checkConsistency();
}

void trackallcontainer_t::rebuildTrackList() {

	tracksRoot.clear();
	addAll(tracksRoot, trackCtr.tracks);
	addAll(tracksRoot, trackReturnCtr.tracks);
	addAll(tracksRoot, trackMasterCtr.tracks);

	// repopulate tracksBottom
	tracksBottom.tracks.clear();
	addAll(tracksBottom.tracks, trackReturnCtr.tracks);
	addAll(tracksBottom.tracks, trackMasterCtr.tracks);


	// reassign global track indices in correct order
	int32_t idx = 0;
	for (track_t* t : trackCtr) {
		t->idx = idx++;
	}
	for (track_t* t : tracksBottom) {
		t->idx = idx++;
	}





	/** turn tree structure into linear pointer array with trackTop at the beginning and the deepest child at the end **/
	track_vector newTracks;
	std::deque<track_t*> stack;
	for (track_t* trackTop : tracksRoot) {
		dbgassert(stack.empty());
//		stack.clear();
		stack.push_back(trackTop);
		while (!stack.empty()) {
			track_t* current = stack.front();
			stack.pop_front();
			if (current->children.size())
				stack.insert(stack.begin(), current->children.cbegin(), current->children.cend());
			newTracks.push_back(current);
		}
	}
	tracks = newTracks;
}
void trackallcontainer_t::removeTrack(track_t* track) {
	if (!removeEntry(tracks, track)) {
		dbgassert(0);
		throw applogicexception("trackcontainer_t - attempt to remove non-present element");
	}
	dbgassert(track->audio);
	trackcontainer_tracktype_t* subCtr = trackTypeCtrs[track->type];
	track_vector& vec = subCtr->tracks;
	removeEntry(vec, track);
	removeEntry(tracksRoot, track);
	int32_t locIdx = 0;
	for (track_t* t : vec) {
		t->localIdx = locIdx++;
	}
	rebuildTrackList();
	checkConsistency();
}

void trackallcontainer_t::moveTrack(track_t* track, int32_t dst) {
	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	trackcontainer_tracktype_t* subCtr = trackTypeCtrs[track->type];
	int32_t src = indexOfCtr(subCtr->tracks, track);
	if ((int32_t)subCtr->tracks.size() == dst) dst--;
	dbgassert(src >= 0 && dst >= 0);
	dbgassert(src != dst);

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
	rebuildTrackList();
	checkConsistency();
}
void trackallcontainer_t::copyTo(project_snapshot_t& project) {
	checkConsistency();
	trackCtr.copyTo(project.trackCtr);
	trackMasterCtr.copyTo(project.trackMasterCtr);
	trackReturnCtr.copyTo(project.trackReturnCtr);
}
void trackallcontainer_t::copyFrom(project_snapshot_t& project) {
	my_printf("project.tracks: midi: %d returN: %d master: %d\n",
			project.trackCtr.tracks.size(),
			project.trackReturnCtr.tracks.size(),
			project.trackMasterCtr.tracks.size());




	dbgassert(tracks.empty());
	dbgassert(tracksRoot.empty());
	dbgassert(trackCtr.empty());
	dbgassert(trackReturnCtr.empty());
	dbgassert(trackMasterCtr.empty());
	dbgassert(tracksBottom.empty());

	trackCtr.copyFrom(project.trackCtr);
	dbgassert(trackCtr.size()==project.trackCtr.tracks.size());

	trackReturnCtr.copyFrom(project.trackReturnCtr);
	dbgassert(trackReturnCtr.size()==project.trackReturnCtr.tracks.size());

	trackMasterCtr.copyFrom(project.trackMasterCtr);
	dbgassert(trackMasterCtr.size()==project.trackMasterCtr.tracks.size());


//	addAll(tracks, trackCtr.tracks);
//	addAll(tracks, trackReturnCtr.tracks);
//	addAll(tracks, trackMasterCtr.tracks);
//	int32_t idx = 0;
//	for (track_t* track : tracks) {
//		track->idx = idx++;
//	}
//	dbgassert(tracks.size()==(project.trackCtr.tracks.size()+project.trackMasterCtr.tracks.size()+project.trackReturnCtr.tracks.size()));


	// reassign local track indices in correct order
	int32_t locIdx = 0;
	for (track_t* t : trackCtr) {
		t->localIdx = locIdx++;
	}
	locIdx = 0;
	for (track_t* t : trackReturnCtr) {
		t->localIdx = locIdx++;
	}
	locIdx = 0;
	for (track_t* t : trackMasterCtr) {
		t->localIdx = locIdx++;
	}

	tracksBottom.tracks.clear();
	addAll(tracksBottom.tracks, trackReturnCtr.tracks);
	addAll(tracksBottom.tracks, trackMasterCtr.tracks);
	tracksRoot.clear();
	addAll(tracksRoot, trackCtr.tracks);
	addAll(tracksRoot, tracksBottom.tracks);

	// reassign global track indices in correct order
	int32_t idx = 0;
	for (track_t* t : trackCtr) {
		t->idx = idx++;
	}
	for (track_t* t : tracksBottom) {
		t->idx = idx++;
	}

	/** turn tree structure into linear pointer array with trackTop at the beginning and the deepest child at the end **/
	track_vector newTracks;
	std::deque<track_t*> stack;
	for (track_t* trackTop : tracksRoot) {
		dbgassert(stack.empty());
//		stack.clear();
		stack.push_back(trackTop);
		while (!stack.empty()) {
			track_t* current = stack.front();
			stack.pop_front();
			if (current->children.size())
				stack.insert(stack.begin(), current->children.cbegin(), current->children.cend());
			newTracks.push_back(current);
		}
	}
	tracks = newTracks;
	checkConsistency();
}
void trackallcontainer_t::loadPlugins(project_snapshot_t& project) {
	trackCtr.loadPlugins(project.trackCtr);
	trackReturnCtr.loadPlugins(project.trackReturnCtr);
	trackMasterCtr.loadPlugins(project.trackMasterCtr);
}
void trackallcontainer_t::loadSubtrackLayouts(project_snapshot_t& project) {
	trackCtr.loadSubtrackLayouts(project.trackCtr);
	trackReturnCtr.loadSubtrackLayouts(project.trackReturnCtr);
	trackMasterCtr.loadSubtrackLayouts(project.trackMasterCtr);
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
void serializeTracks(const track_vector& tracks, trackcontainer_snapshot_t& out) {
	out.tracks.reserve(tracks.size());
	std::vector<const track_t*> newTracks;
	std::deque<const track_t*> stack;
	for (const track_t* trackTop : tracks) {
		dbgassert(stack.empty());
//		stack.clear();
		stack.push_back(trackTop);
		while (!stack.empty()) {
			const track_t* current = stack.front();
			stack.pop_front();
			if (current->children.size()) {
				stack.insert(stack.begin(), current->children.cbegin(), current->children.cend());
			}
			track_snapshot_t trackCopy(current, true);
			newTracks.push_back(current);
			out.tracks.push_back(std::move(trackCopy));
		}
	}
	for (const track_t* trackTop : newTracks) {
		int32_t idx = -1;
		if (trackTop->parent) {
			idx = indexOfCtr(newTracks, trackTop);
		}
		out.hierachy.push_back(idx);
	}
}
void deserializeTracks(trackcontainer_snapshot_t& in, track_vector& out) {
	dbgassert(in.hierachy.size() == in.tracks.size());
	auto& hierachyIndices = in.hierachy;
	for (size_t idx = 0; idx < in.hierachy.size(); ++idx) {
		auto parentIdx = hierachyIndices[idx];
		dbgassert(parentIdx == -1 || parentIdx < static_cast<int32_t>(in.tracks.size()));
	}
	track_vector allTracks;
	for (track_snapshot_t& snapshot : in.tracks) {
		track_t* trackCopy = new track_t(snapshot);
		snapshot.trackLoaded = trackCopy;
		allTracks.push_back(trackCopy);
	}
	for (size_t idx = 0; idx < hierachyIndices.size(); ++idx) {
		track_t* track = allTracks[idx];
		int32_t parentIdx = hierachyIndices[idx];
		if (parentIdx < 0) {
			out.push_back(track);
		} else {
			allTracks[parentIdx]->addChild(track);
		}
	}
}
void trackcontainer_tracktype_t::copyTo(trackcontainer_snapshot_t& out) {
	serializeTracks(tracks, out);
}
void trackcontainer_tracktype_t::copyFrom(trackcontainer_snapshot_t& in) {
	dbgassert(tracks.empty());
	// fix up old project files, assume all tracks are top level tracks with no parent
	if (in.hierachy.empty() && !in.tracks.empty()) {
		for (int i = 0; i < in.tracks.size(); ++i)
			in.hierachy.push_back(-1);
	}
	track_vector newTracks;
	deserializeTracks(in, newTracks);
	bool reassignIdx = false;
	for (track_t* track : newTracks) {
		reassignIdx |= track->localIdx < 0;
	}
	if (reassignIdx) {
		int32_t idx = 0;
		for (track_t* tr2 : newTracks) {
			tr2->localIdx = idx++;
		}
	} else {
		std::sort(newTracks.begin(), newTracks.end(), [](track_t* const & a, track_t* const & b) {
			return a->localIdx < b->localIdx;
		});
	}
	tracks = newTracks;
}
void trackcontainer_tracktype_t::loadPlugins(trackcontainer_snapshot_t& in) {
	for (track_snapshot_t& trackStatic : in.tracks) {
		track_t* trackLoaded = trackStatic.trackLoaded;
		trackLoaded->loadSnapshot(trackStatic);
	}
}
void trackcontainer_tracktype_t::loadSubtrackLayouts(trackcontainer_snapshot_t& in) {
	for (track_snapshot_t& trackStatic : in.tracks) {
		track_t* trackLoaded = trackStatic.trackLoaded;
		trackLoaded->loadSubtrackLayout(trackStatic);
		trackStatic.trackLoaded = nullptr;
	}
}
bool trackallcontainer_t::validTrackTypeIdx(int32_t type, int32_t idx) const {
	if (type >= TRACK_TYPE_MASTER && type <= TRACK_TYPE_AUDIO) {
		const trackcontainer_tracktype_t* trackTypeCtr = trackTypeCtrs[type];
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
	for (track_snapshot_t* thisSnapshot : this->tracks) {
		track_snapshot_t* snapshotCopy = new track_snapshot_t(*thisSnapshot);
		dbgassert(snapshotCopy->clips.size() == thisSnapshot->clips.size());
		t.tracks.push_back(snapshotCopy);
	}
	return t;
}

