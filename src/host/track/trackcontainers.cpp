#include <algorithm>
#include "exceptions.hpp"
#include "seq_util.hpp"
#include "seq_time.hpp"
#include "host/track/track.hpp"
#include "host/track/track_impl.hpp"
#include "host/daw/mainctrl.hpp"

void releaseTrackContainer(track_vector& vec) {
    for (auto track : vec) {
        releaseTrackResources(track, nullptr);
    }
    for (auto track : vec) {
        delete track;
    }
}

trackbasecontainer_t::~trackbasecontainer_t() {
    releaseTrackContainer(tracksFlat);
    clear();
}

trackallcontainer_t::~trackallcontainer_t() {
    releaseTrackContainer(trackAllCtr.tracksFlat);
    clear();
}

void assertUniqueEntries(const track_vector& vector) {
#ifndef NDEBUG
    track_vector tracksCopy = vector;

    bool wasUnique = (std::unique(tracksCopy.begin(), tracksCopy.end()) == tracksCopy.end());
    dbgassert(wasUnique);
#endif
}

void trackallcontainer_t::checkConsistency() {
#ifndef NDEBUG
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
            dbgassert(STL_CONTAINS(trackTop->parent->children, trackTop));
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
            if (!current->children.empty()) {
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
        dbgassert(t->projectIdx == treeIdx);
        treeIdx++;
    }
#endif
}

void trackallcontainer_t::addTrack(int trackInsertPos, track_t* newTrack) {
    auto it = std::find(trackAllCtr.tracksFlat.begin(), trackAllCtr.tracksFlat.end(), newTrack);
    if (it != trackAllCtr.tracksFlat.end()) {
        dbgassert(0);
        throw applogicexception("attempt to add track twice");
    }

    // trackInsertPos is tracktype-container index
    trackcontainer_tracktype_t* trackTypeCtr = trackTypeCtrs[newTrack->type];

     // TODO: I think this line is not required and argument trackInsertPos has no effect
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

    // repopulate tracksBottom
    tracksBottom.clear();

    addAll(tracksBottom.tracksFlat, trackReturnCtr.tracksFlat);
    addAll(tracksBottom.tracksFlat, trackMasterCtr.tracksFlat);

    addAll(tracksBottom.tracksTree, trackReturnCtr.tracksTree);
    addAll(tracksBottom.tracksTree, trackMasterCtr.tracksTree);

    // repopulate trackAllCtr
    trackAllCtr.clear();

    addAll(trackAllCtr.tracksFlat, trackMidiAudioCtr.tracksFlat);
    addAll(trackAllCtr.tracksFlat, tracksBottom.tracksFlat);

    addAll(trackAllCtr.tracksTree, trackMidiAudioCtr.tracksTree);
    addAll(trackAllCtr.tracksTree, tracksBottom.tracksTree);

    // reassign global track indices in correct order
    int32_t idx = 0;
    for (track_t* t : trackAllCtr) {
        t->projectIdx = idx++;
    }
}

void trackallcontainer_t::removeTrack(track_t* track) {
    // trackInsertPos is tracktype-container index
    trackcontainer_tracktype_t* trackTypeCtr = trackTypeCtrs[track->type];

    if (!assert_expr(trackTypeCtr->remove(track))) {
        return; // attempt to remove non-present element
    }

    int32_t idx = 0;
    for (track_t* tr : trackTypeCtr->tracksFlat) {
        tr->localIdxFlat = idx++;
    }

    rebuildTrackList();
    checkConsistency();
}

bool trackallcontainer_t::moveTracks(const std::vector<track_t*>& tracks, track_tree_pos_t& treePos) {
    // remove all tracks, reinsert them as consecutive range at position treePos
    // precondition: no loops are generated by making tracks child of node at treePos
    // precondition2: track layout changes by doing that move (skip move to idx or idx+)

    checkConsistency();

    dbgassert(!tracks.empty());

    if (tracks.size() > 1) {
        dbgassert(0 && "NOT IMPLEMENTED");
    }

    dbgassert(treePos.treeIdx >= 0);
    auto* p = treePos.parent;
    while (p) {
        if (STL_CONTAINS(tracks, p)) {
            log_printf("cannot move here\n");
            return false;
        }
        p = p->parent;
    }


    // all tracks must be of same type
    const int32_t trackTypeCtrIdx = TRACKTYPE_TO_CTR(tracks.front()->type);
#ifndef NDEBUG
    for (track_t* track : tracks) {
        dbgassert(trackTypeCtrIdx == TRACKTYPE_TO_CTR(track->type));
    }
#endif // NDEBUG
    if (treePos.trackTypeCtr != trackTypeCtrIdx) {
        log_printf("cannot move here\n");
        return false;
    }
    trackcontainer_tracktype_t* const trackTypeCtr = trackTypeUniqueCtrs[trackTypeCtrIdx];

    int32_t childIdx  = treePos.treeIdx;
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

void trackallcontainer_t::copyTo(project_snapshot_t& project) {
    checkConsistency();
    trackMidiAudioCtr.copyTo(project.trackCtr);
    trackMasterCtr.copyTo(project.trackMasterCtr);
    trackReturnCtr.copyTo(project.trackReturnCtr);
}

void trackallcontainer_t::copyFrom(project_snapshot_t& project) {
    log_lf(Log::L_DEBUG, "project.tracks: audio/midi: %zu return: %zu master: %zu\n",
              project.trackCtr.tracks.size(),
              project.trackReturnCtr.tracks.size(),
              project.trackMasterCtr.tracks.size());


    dbgassert(trackAllCtr.empty());
    dbgassert(trackMidiAudioCtr.empty());
    dbgassert(trackReturnCtr.empty());
    dbgassert(trackMasterCtr.empty());
    dbgassert(tracksBottom.empty());

    trackMidiAudioCtr.copyFrom(project.trackCtr);
    dbgassert(trackMidiAudioCtr.size() == project.trackCtr.tracks.size());

    trackReturnCtr.copyFrom(project.trackReturnCtr);
    dbgassert(trackReturnCtr.size() == project.trackReturnCtr.tracks.size());

    trackMasterCtr.copyFrom(project.trackMasterCtr);
    dbgassert(trackMasterCtr.size() == project.trackMasterCtr.tracks.size());

    rebuildTrackList();
    checkConsistency();
}

void trackallcontainer_t::loadProjectSnapshot(DAW::Host::PluginManager* host, project_snapshot_t& project) {
    trackMidiAudioCtr.loadTrackSnapshots(host, project.trackCtr);
    trackReturnCtr.loadTrackSnapshots(host, project.trackReturnCtr);
    trackMasterCtr.loadTrackSnapshots(host, project.trackMasterCtr);
}

void trackallcontainer_t::copyTracks(int32_t trackBegin, int32_t trackEnd, trackstate_t& _out) {
    _out.reset();
    for (track_t* t : trackAllCtr) {
        if (t->projectIdx >= trackBegin && t->projectIdx <= trackEnd) {
            auto* trackCopy = new track_snapshot_t(t, tracksnapshot_store_opts_t::NoPluginPresets());
            _out.tracks.push_back(trackCopy);
        }
    }
}

void serializeTracks(const track_vector& tracksTree, trackcontainer_snapshot_t& out) {
    out.tracks.reserve(tracksTree.size());//not enough
    std::vector<const track_t*> tracksFlat;
    std::deque<const track_t*> stack;
    for (const track_t* trackTop : tracksTree) {
        dbgassert(stack.empty());
        //stack.clear();
        stack.push_back(trackTop);
        while (!stack.empty()) {
            const track_t* current = stack.front();
            stack.pop_front();
            if (!current->children.empty()) {
                stack.insert(stack.begin(), current->children.cbegin(), current->children.cend());
            }
            track_snapshot_t trackCopy(current, tracksnapshot_store_opts_t::All());
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
#ifndef NDEBUG
    for (size_t idx = 0; idx < in.hierachy.size(); ++idx) {
        auto parentIdx = hierachyIndices[idx];
        dbgassert(parentIdx == -1 || parentIdx < static_cast<int32_t>(in.tracks.size()));
    }
#endif // NDEBUG
    track_vector allTracks;
    for (track_snapshot_t& snapshot : in.tracks) {
        auto* trackCopy = new track_t(snapshot);
        snapshot.trackLoaded = trackCopy;
        allTracks.push_back(trackCopy);
    }
    for (size_t idx = 0; idx < hierachyIndices.size(); ++idx) {
        track_t* track    = allTracks[idx];
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
        in.hierachy.resize(in.tracks.size(), -1);
        std::fill(in.hierachy.begin(), in.hierachy.end(), -1);
    }
    track_vector newTrackVecTree;
    deserializeTrackTree(in, newTrackVecTree);
    setFromVectorTree(std::move(newTrackVecTree));
    int32_t idx = 0;
    for (track_t* tr : tracksFlat) {
        tr->localIdxFlat = idx++;
    }
}

void trackcontainer_tracktype_t::loadTrackSnapshots(DAW::Host::PluginManager* host, trackcontainer_snapshot_t& in) {
    for (track_snapshot_t& trackStatic : in.tracks) {
        track_t* trackLoaded = trackStatic.trackLoaded;
        trackLoaded->loadSnapshot(host, trackStatic);
    }
    for (track_t* track : this->tracksFlat) {
        if (!track->audio) {
            host->createAudio(track);
        }
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

track_t* trackallcontainer_t::resolveTrack(const audio_stage_ref_t& ref) const {
    if (ref.stageId == TRACKID_INVALID_I32)
        return nullptr;

    dbgassert((int32_t) ref.stageId > -1);
    auto it = std::find_if(trackAllCtr.begin(), trackAllCtr.end(), [ref](const track_t* ptr) {
        return ptr->getStage() && audioStageIdMatches(ptr->getStage()->stageId, ref.stageId);
    });
    //dbgassert(it != trackAllCtr.end());
    if (it != trackAllCtr.end()) {
        return *it;
    }
    log_lf(Log::L_DEBUG, "null track for %d\n", static_cast<int32_t>(ref.stageId));
    return nullptr;
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
        auto* snapshotCopy = new track_snapshot_t(*thisSnapshot);
        dbgassert(snapshotCopy->clips.size() == thisSnapshot->clips.size());
        t.tracks.push_back(snapshotCopy);
    }
    return t;
}
