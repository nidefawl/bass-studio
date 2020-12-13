#pragma once
#include "track.h"
#include "track_graph.h"
#include "daw_channel.h"
#include "assert_dbg.h"
#include "track_types.h"
#include <vector>

class track_t;
namespace DAW {
//
///**
// * track_node_t - represents a node in the audio chain dependency graph
// *
// */
//struct effect_source_t {
//	uint32_t trackEdgeId;
//	channel_ref_t channel;
//	float gain;
//	samplerate_t latency = 0U;
//	audiostageflags_t flags;
//};
//
//struct effect_node_t {
//	int32_t projectGlobalId = 0;
//	std::vector<int32_t> dependencies;
//	std::vector<effect_source_t> pulls;
//	std::vector<effect_node_t*> parents;
//	std::vector<effect_node_t*> children;
//	samplerate_t internalLatency = INVALID_SAMPLE_OFFSET_U32;
//	samplerate_t inputLatency = INVALID_SAMPLE_OFFSET_U32;
//
//	effect_node_t() = default;
//	effect_node_t(int32_t _projectGlobalId, samplerate_t _internalLatency)
//	: projectGlobalId(_projectGlobalId), internalLatency(_internalLatency)
//	{
//
//	}
//};
//struct processing_effect_node_t : public effect_node_t {
//	processing_effect_node_t() = default;
//	effectbase* effectOptional = nullptr;
//	audio_stage_t* stage = nullptr;
//};
//
////using track_node_ptr = std::unique_ptr<track_node_t>;
using effect_node_t = track_node_t;
using effect_node_ptr = effect_node_t*;
using effect_source_t = track_source_t;
using processing_effect_node_t = processing_track_node_t;
using processing_effect_node_ptr = processing_effect_node_t*;
using effect_graph_t = track_graph_t;
using effect_processing_graph_t = processing_graph_t;


bool buildEffectRoutingGraph(const vsthost* const host, const project_t* const project, const audio_stage_t* stage, std::shared_ptr<effect_graph_t>& out_graph);
bool buildEffectProcessingGraph(const vsthost* const host, const project_t* const project, const audio_stage_t* stage, std::shared_ptr<effect_processing_graph_t>& out_procgraph);

//bool removeTrackRoutings(const track_vector& tracksFlat, const int32_t stageId);
//bool buildTrackRoutingGraph(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat, std::shared_ptr<effect_graph_t>& out_graph);
//bool buildProcessingGraph(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat, std::shared_ptr<effect_processing_graph_t>& out_procgraph);
//bool validateTrackRoutings(const vsthost* const host, const track_vector& tracksFlat);
}
