#pragma once
#include "track.h"
#include "daw_channel.h"
#include "assert_dbg.h"
#include "track_types.h"
#include <vector>

class track_t;
namespace DAW {

/**
 * track_node_t - represents a node in the audio chain dependency graph
 *
 */
struct track_source_t {
	channel_ref_t channel;
	float gain;
	samplerate_t latency = 0U;
	audiostageflags_t flags;
};
enum track_node_type {
	TRACK, GROUP
};
struct track_node_t {
	audiostageid_i32 stageId = TRACKID_INVALID_I32;
	std::vector<audiostageid_i32> dependencies;
	std::vector<track_source_t> pulls;
	std::vector<track_source_t> pushs;
	std::vector<track_node_t*> parents;
	std::vector<track_node_t*> children;
	samplerate_t internalLatency = INVALID_SAMPLE_OFFSET_U32;
	samplerate_t inputLatency = INVALID_SAMPLE_OFFSET_U32;

	track_node_t() = default;
	track_node_t(audiostageid_i32 _stageId, samplerate_t _internalLatency)
	: stageId(_stageId), internalLatency(_internalLatency)
	{

	}
};
struct processing_track_node_t : public track_node_t {
	processing_track_node_t() = default;
	track_t* trackOptional = nullptr;
};

//using track_node_ptr = std::unique_ptr<track_node_t>;
using track_node_ptr = track_node_t*;
using processing_track_node_ptr = processing_track_node_t*;
/**
 * track_graph_t - represents the audio chain dependency graph build from I/O configuration of all loaded tracks
 *
 */
struct track_graph_t {
	std::vector<track_node_t*> roots; // output nodes (Master, )
	std::vector<track_node_ptr> nodes;
	uint64_t maxLatency = 0U;
	~track_graph_t() {
		for (auto ptr : nodes) {
			delete ptr;
		}
	}
};
struct processing_graph_t {
	std::vector<processing_track_node_t*> nodesSolo;
	std::vector<processing_track_node_t*> nodesFlatOrdered;
	std::vector<processing_track_node_t*> roots;
	std::vector<processing_track_node_ptr> nodes;
	std::shared_ptr<track_graph_t> trackGraph;
	~processing_graph_t() {
		for (auto ptr : nodes) {
			delete ptr;
		}
	}
};


bool removeTrackRoutings(const track_vector& tracksFlat, const audiostageid_i32 stageId);
bool buildTrackRoutingGraph(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat, std::shared_ptr<track_graph_t>& out_graph);
bool buildProcessingGraph(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat, std::shared_ptr<processing_graph_t>& out_procgraph);
bool validateTrackRoutings(const vsthost* const host, const track_vector& tracksFlat);
}
