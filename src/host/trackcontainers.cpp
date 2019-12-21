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
	for (auto track : tracksFlat) {
		releaseTrackResources(track, NULL);
	}
	for (auto track : tracksFlat) {
		delete track;
	}
	clear();
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
	assertUniqueEntries(trackAllCtr.tracksFlat);
	assertUniqueEntries(trackAllCtr.tracksTree);
	assertUniqueEntries(trackMidiAudioCtr.tracksFlat);
	assertUniqueEntries(trackMidiAudioCtr.tracksTree);
	assertUniqueEntries(trackReturnCtr.tracksFlat);
	assertUniqueEntries(trackReturnCtr.tracksTree);
	assertUniqueEntries(trackMasterCtr.tracksFlat);
	assertUniqueEntries(trackMasterCtr.tracksTree);
	assertUniqueEntries(tracksBottom.tracksFlat);
	assertUniqueEntries(tracksBottom.tracksTree);

	for (track_t* trackTop : trackAllCtr.tracksTree) {
		dbgassert(!trackTop->parent);
	}

	track_vector allTracksParent;
	size_t numTracksRoot = 0;
	for (track_t* trackTop : trackAllCtr.tracksFlat) {
		if (!trackTop->parent) {
			dbgassert(STL_CONTAINS(trackAllCtr.tracksTree, trackTop));
			numTracksRoot++;
		} else {
			allTracksParent.push_back(trackTop);
		}
		for (track_t* trackChild : trackTop->children) {
			dbgassert(trackChild->parent == trackTop);
		}
	}
	dbgassert(numTracksRoot == trackAllCtr.tracksTree.size());
	assertUniqueEntries(allTracksParent);

	track_vector newTracks;
	std::deque<track_t*> stack;
	for (track_t* trackTop : trackAllCtr.tracksTree) {
		dbgassert(stack.empty());
		stack.push_back(trackTop);
		while (!stack.empty()) {
			track_t* current = stack.front();
			stack.pop_front();
			if (current->children.size()) {
				int32_t treeIdx = 0;
				for (track_t* tChildTest : current->children) {
					dbgassert(tChildTest->childIdxTree == treeIdx);
					treeIdx++;
				}
				stack.insert(stack.begin(), current->children.cbegin(), current->children.cend());
			}
			newTracks.push_back(current);
		}
	}
	dbgassert(newTracks == trackAllCtr.tracksFlat);


	// assert tracks are stored in correct
	int32_t localIdxFlat = 0;
	for (track_t* t : trackMidiAudioCtr.tracksFlat) {
		dbgassert(t->localIdxFlat == localIdxFlat);
		localIdxFlat++;
	}
	localIdxFlat = 0;
	for (track_t* t : trackReturnCtr.tracksFlat) {
		dbgassert(t->localIdxFlat == localIdxFlat);
		localIdxFlat++;
	}
	localIdxFlat = 0;
	for (track_t* t : trackMasterCtr.tracksFlat) {
		dbgassert(t->localIdxFlat == localIdxFlat);
		localIdxFlat++;
	}
	// assert tracks are stored in correct
	int32_t treeIdx = 0;
	for (track_t* t : trackMidiAudioCtr.tracksTree) {
		dbgassert(t->childIdxTree == treeIdx);
		treeIdx++;
	}
	treeIdx = 0;
	for (track_t* t : trackReturnCtr.tracksTree) {
		dbgassert(t->childIdxTree == treeIdx);
		treeIdx++;
	}
	treeIdx = 0;
	for (track_t* t : trackMasterCtr.tracksTree) {
		dbgassert(t->childIdxTree == treeIdx);
		treeIdx++;
	}

	treeIdx = 0;
	for (track_t* t : trackAllCtr.tracksFlat) {
		dbgassert(t->idx == treeIdx);
		treeIdx++;
	}
}
void trackallcontainer_t::addTrack(int trackInsertPos, track_t* newTrack) {
	auto it = std::find(trackAllCtr.tracksFlat.begin(), trackAllCtr.tracksFlat.end(), newTrack);
	if (it != trackAllCtr.tracksFlat.end()) {
		dbgassert(0);
		throw applogicexception("attempt to add track twice");
	}

	// trackInsertPos is tracktype-container index
	trackcontainer_tracktype_t* trackTypeCtr = trackTypeCtrs[newTrack->type];

	// only add root tracks (nodes with no parent) to root list
	// children have to have to set their parent reference outside
	newTrack->localIdxFlat = trackInsertPos;
	trackTypeCtr->add(newTrack);

	int32_t idx = 0;
	for (track_t* tr : trackTypeCtr->tracksFlat) {
		tr->localIdxFlat = idx++;
	}
	rebuildTrackList();
	checkConsistency();
}

void trackallcontainer_t::rebuildTrackList() {

//	trackbasecontainer_t trackAllCtr;
//	trackcontainer_tracktype_t trackMidiAudioCtr;
//	trackcontainer_tracktype_t trackReturnCtr;
//	trackcontainer_tracktype_t trackMasterCtr;
//	trackbasecontainer_t tracksBottom;


	// repopulate tracksBottom
	tracksBottom.clear();

	addAll(tracksBottom.tracksFlat, trackReturnCtr.tracksFlat);
	addAll(tracksBottom.tracksFlat, trackMasterCtr.tracksFlat);

	addAll(tracksBottom.tracksTree, trackReturnCtr.tracksTree);
	addAll(tracksBottom.tracksTree, trackMasterCtr.tracksTree);

//	addAll(tracksBottom.tracksVisibleFlat, trackReturnCtr.tracksVisibleFlat);
//	addAll(tracksBottom.tracksVisibleFlat, trackMasterCtr.tracksVisibleFlat);



	// repopulate trackAllCtr
	trackAllCtr.clear();

	addAll(trackAllCtr.tracksFlat, trackMidiAudioCtr.tracksFlat);
	addAll(trackAllCtr.tracksFlat, tracksBottom.tracksFlat);

	addAll(trackAllCtr.tracksTree, trackMidiAudioCtr.tracksTree);
	addAll(trackAllCtr.tracksTree, tracksBottom.tracksTree);

//	addAll(trackAllCtr.tracksVisibleFlat, trackMidiAudioCtr.tracksVisibleFlat);
//	addAll(trackAllCtr.tracksVisibleFlat, tracksBottom.tracksVisibleFlat);

	// reassign global track indices in correct order
	int32_t idx = 0;
	for (track_t* t : trackAllCtr) {
		t->idx = idx++;
	}
}
void trackallcontainer_t::removeTrack(track_t* track) {
	// trackInsertPos is tracktype-container index
	trackcontainer_tracktype_t* trackTypeCtr = trackTypeCtrs[track->type];

	if (!trackTypeCtr->remove(track)) {
		dbgassert(0);
		throw applogicexception("trackcontainer_t - attempt to remove non-present element");
	}
	int32_t idx = 0;
	for (track_t* tr : trackTypeCtr->tracksFlat) {
		tr->localIdxFlat = idx++;
	}

//	dbgassert(track->audio);
//	trackcontainer_tracktype_t* subCtr = trackTypeCtrs[track->type];
//	track_vector& vec = subCtr->tracks;
//	removeEntry(vec, track);
//	removeEntry(tracksRoot, track);
	rebuildTrackList();
	checkConsistency();
}

bool trackallcontainer_t::moveTracks(const std::vector<track_t*>& tracks, track_tree_pos_t& treePos) {
	// remove all tracks, reinsert them as consecutive range at position treePos
	// precondition: no loops are generated by making tracks child of node at treePos
	// precondition2: track layout changes by doing that move (skip move to idx or idx+)

	checkConsistency();

	dbgassert(tracks.size());

	if (tracks.size()>1)
	{
		dbgassert(0&&"NOT IMPLEMENTED");
	}

	dbgassert(treePos.treeIdx >= 0);
	auto* p = treePos.parent;
	while (p) {
		if (STL_CONTAINS(tracks, p)) {
			log_printf("cannot move here\n", 0);
			return false;
		}
		p = p->parent;
	}


	// all tracks must be of same type
	const int32_t trackTypeCtrIdx = TRACKTYPE_TO_CTR(tracks.front()->type);
	for (track_t* track : tracks) {
		dbgassert(trackTypeCtrIdx == TRACKTYPE_TO_CTR(track->type));
	}
	if (treePos.trackTypeCtr != trackTypeCtrIdx) {
		log_printf("cannot move here\n", 0);
		return false;
	}
	trackcontainer_tracktype_t* const trackTypeCtr = trackTypeUniqueCtrs[trackTypeCtrIdx];

	int32_t childIdx = treePos.treeIdx;
	int32_t targetPos = childIdx;

	for (track_t* track : tracks) {
		//TODO: adjust childIdx when removing occurs on treePos.parent and idx <= childIdx
		dbgassert(track != treePos.parent);
		if (track->parent == treePos.parent && track->childIdxTree < targetPos) {
			dbgassert(targetPos > 0);
			targetPos--;
		}
		trackTypeCtr->remove(track);
	}
	{
		int32_t idx = 0;
		for (track_t* tr : trackTypeCtr->tracksFlat) {
			tr->localIdxFlat = idx++;
		}
		rebuildTrackList();
		checkConsistency();
	}
	for (track_t* track : tracks) {
		track->childIdxTree = targetPos++;
	}
	if (treePos.parent) {
		for (track_t* track : tracks) {
			treePos.parent->addChild(track);
		}
	}
	for (track_t* track : tracks) {
		if (!track->parent) {
			track->localIdxFlat = childIdx;
		}
		trackTypeCtr->add(track);
	}
	{
		int32_t idx = 0;
		for (track_t* tr : trackTypeCtr->tracksFlat) {
			tr->localIdxFlat = idx++;
		}
		rebuildTrackList();
		checkConsistency();
	}

	return true;
}
void trackallcontainer_t::moveTrack(track_t* track, int32_t dst) {
	dbgassert(0);//UNSAFE
	int32_t src = indexOfCtr(trackAllCtr.tracksFlat, track);
	if ((int32_t)trackAllCtr.tracksFlat.size() == dst) dst--;
	dbgassert(src >= 0 && dst >= 0);
	dbgassert(src != dst);
//
	track_vector curOrder = trackAllCtr.tracksFlat;
	track_vector newOrder;
	newOrder.resize(curOrder.size());
//	auto itIn = curOrder.cbegin();
//	auto itOut = newOrder.begin();
//	for (;itOut!=newOrder.cend();) {
//		if (curOrder.cbegin()+src == itIn) {
//			itIn++;
//		} else if (newOrder.cbegin()+dst == itOut) {
//			*itOut++ = curOrder[src];
//		} else {
//			*itOut++ = *itIn++;
//		}
//	}
//	this->tracks = (newOrder);
//	int32_t locIdx = 0;
//	for (track_t* t : this->tracks) {
//		t->localIdx = locIdx++;
//	}
	rebuildTrackList();
	checkConsistency();
}
void trackallcontainer_t::copyTo(project_snapshot_t& project) {
	checkConsistency();
	trackMidiAudioCtr.copyTo(project.trackCtr);
	trackMasterCtr.copyTo(project.trackMasterCtr);
	trackReturnCtr.copyTo(project.trackReturnCtr);
}
void trackallcontainer_t::copyFrom(project_snapshot_t& project) {
	my_printf("project.tracks: midi: %d returN: %d master: %d\n",
			project.trackCtr.tracks.size(),
			project.trackReturnCtr.tracks.size(),
			project.trackMasterCtr.tracks.size());


	dbgassert(trackAllCtr.empty());
	dbgassert(trackMidiAudioCtr.empty());
	dbgassert(trackReturnCtr.empty());
	dbgassert(trackMasterCtr.empty());
	dbgassert(tracksBottom.empty());

	trackMidiAudioCtr.copyFrom(project.trackCtr);
	dbgassert(trackMidiAudioCtr.size()==project.trackCtr.tracks.size());

	trackReturnCtr.copyFrom(project.trackReturnCtr);
	dbgassert(trackReturnCtr.size()==project.trackReturnCtr.tracks.size());

	trackMasterCtr.copyFrom(project.trackMasterCtr);
	dbgassert(trackMasterCtr.size()==project.trackMasterCtr.tracks.size());

	rebuildTrackList();
	checkConsistency();

}
void trackallcontainer_t::loadPlugins(project_snapshot_t& project) {
	trackMidiAudioCtr.loadPlugins(project.trackCtr);
	trackReturnCtr.loadPlugins(project.trackReturnCtr);
	trackMasterCtr.loadPlugins(project.trackMasterCtr);


	/*
	 * i/o track channel names are not included in serialized data.
	 * re-resolve track input/output configuration to assign channel names
	 */

	for (track_t* tr : trackAllCtr.tracksFlat) {
		track_impl_t* trImpl = tr->audio;
		if (DAW::isChannelConnected(trImpl->inputChannel) && trImpl->inputChannel.getType() == DAW::channel_input_type::INPUT_AUDIOSTAGE) {
			auto it = std::find_if(trackAllCtr.tracksFlat.begin(), trackAllCtr.tracksFlat.end(), [stageId = trImpl->inputChannel.stage.stageRef.stageId](track_t* trForStageId){
				return trForStageId->audio->stageId == stageId;
			});
			if (it != trackAllCtr.tracksFlat.end()) {
				trImpl->inputChannel = DAW::ChannelStage((*it)->audio, false);
			} else {
				trImpl->inputChannel = DAW::ChannelNone();
			}
		}
		if (DAW::isChannelConnected(trImpl->outputChannel) && trImpl->outputChannel.getType() == DAW::channel_input_type::INPUT_AUDIOSTAGE) {
			auto it = std::find_if(trackAllCtr.tracksFlat.begin(), trackAllCtr.tracksFlat.end(), [stageId = trImpl->outputChannel.stage.stageRef.stageId](track_t* trForStageId){
				return trForStageId->audio->stageId == stageId;
			});
			if (it != trackAllCtr.tracksFlat.end()) {
				trImpl->outputChannel = DAW::ChannelStage((*it)->audio, true);
			} else {
				trImpl->outputChannel = DAW::ChannelNone();
			}
		}
	}
}
void trackallcontainer_t::loadSubtrackLayouts(project_snapshot_t& project) {
	trackMidiAudioCtr.loadSubtrackLayouts(project.trackCtr);
	trackReturnCtr.loadSubtrackLayouts(project.trackReturnCtr);
	trackMasterCtr.loadSubtrackLayouts(project.trackMasterCtr);
}
void trackallcontainer_t::copyTracks(int32_t trackBegin, int32_t trackEnd, trackstate_t& _out) {
	_out.reset();
	for (track_t* t: trackMidiAudioCtr) {
		if (t->idx >= trackBegin && t->idx <= trackEnd) {
			my_printf("copy track %d\n", t->idx);
			track_snapshot_t* trackCopy = new track_snapshot_t(t, false);
			_out.tracks.push_back(trackCopy);
		} else {
			my_printf("NOT copy track %d\n", t->idx);
		}
	}
}
void serializeTracks(const track_vector& tracksTree, trackcontainer_snapshot_t& out) {
	out.tracks.reserve(tracksTree.size()); //not enough
	std::vector<const track_t*> tracksFlat;
	std::deque<const track_t*> stack;
	for (const track_t* trackTop : tracksTree) {
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
			tracksFlat.push_back(current);
			out.tracks.push_back(std::move(trackCopy));
		}
	}
	for (const track_t* trackTop : tracksFlat) {
		int32_t idx = -1;
		if (trackTop->parent) {
			idx = indexOfCtr(tracksFlat, trackTop->parent);
		}
		out.hierachy.push_back(idx);
	}
}
void deserializeTrackTree(trackcontainer_snapshot_t& in, track_vector& out) {
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
	int32_t childIdx = 0;
	for (track_t* trackRoot : out) {
		trackRoot->childIdxTree = childIdx++;
	}
}
void trackcontainer_tracktype_t::copyTo(trackcontainer_snapshot_t& out) {
	serializeTracks(tracksTree, out);
}
void trackcontainer_tracktype_t::copyFrom(trackcontainer_snapshot_t& in) {
	dbgassert(empty());
	// fix up old project files, assume all tracks are top level tracks with no parent
	if (in.hierachy.empty() && !in.tracks.empty()) {
		for (int i = 0; i < in.tracks.size(); ++i)
			in.hierachy.push_back(-1);
	}
	track_vector newTrackVecTree;
	deserializeTrackTree(in, newTrackVecTree);
//	bool reassignIdx = false;
//	for (track_t* track : newTrackVecTree) {
//		reassignIdx |= track->localIdx < 0;
//	}
//	if (reassignIdx) {
//		int32_t idx = 0;
//		for (track_t* tr2 : newTrackVecTree) {
//			tr2->localIdx = idx++;
//		}
//	} else {
//		std::sort(newTrackVecTree.begin(), newTrackVecTree.end(), [](track_t* const & a, track_t* const & b) {
//			return a->localIdx < b->localIdx;
//		});
//	}
	setFromVectorTree(std::move(newTrackVecTree));
	int32_t idx = 0;
	for (track_t* tr : tracksFlat) {
		tr->localIdxFlat = idx++;
	}
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
	track_vector& vec = trackTypeCtrs[type]->tracksFlat;
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

