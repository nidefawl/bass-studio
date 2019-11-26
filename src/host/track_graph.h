#pragma once
#include "track.h"
#include "daw_channel.h"
#include "assert_dbg.h"
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
};
struct track_node_t {
	audiostageid_i32 stageId = TRACKID_INVALID_I32;
	std::vector<audiostageid_i32> dependencies;
	std::vector<track_source_t> pulls; // fill latency
	std::vector<track_source_t> pushs; // fill latency
	int32_t numDependants = 0;
	samplerate_t internalLatency = 0U;
	samplerate_t inputLatency = 0U;
	track_node_t() = default;
	track_node_t(audiostageid_i32 _stageId, samplerate_t _internalLatency)
	: stageId(_stageId), internalLatency(_internalLatency)
	{

	}
};
/**
 * track_graph_t - represents the audio chain dependency graph build from I/O configuration of all loaded tracks
 *
 */
struct track_graph_t {
	std::vector<audiostageid_i32> roots; // outbut nodes (Master, )
	std::vector<track_node_t> nodes; // fill latency
	uint64_t maxLatency;
};
struct processing_track_node_t {
	audiostageid_i32 nodeIdx;
	track_node_t trackNode;
	track_t* track;
};
struct processing_list_t {
	std::vector<processing_track_node_t> nodes;
};

bool removeTrackRoutings(const track_vector& tracksFlat, const audiostageid_i32 stageId);
bool buildTrackRoutingGraph(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat, track_graph_t& out);
bool buildProcessingGraph(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat, track_graph_t& out_trackGraph, processing_list_t& out_processingList);
bool validateTrackRoutings(const vsthost* const host, const track_vector& tracksFlat);
}
