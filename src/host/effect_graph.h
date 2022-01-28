#pragma once
#include "track.h"
#include "track_graph.h"
#include "daw_channel.h"
#include "assert_dbg.h"
#include "track_types.h"
#include <vector>

class track_t;
class effectbase;
namespace DAW {

    using effect_node_t              = track_node_t;
    using effect_node_ptr            = effect_node_t*;
    using effect_source_t            = track_source_t;
    using processing_effect_node_t   = processing_track_node_t;
    using processing_effect_node_ptr = processing_effect_node_t*;
    using effect_graph_t             = track_graph_t;
    using effect_processing_graph_t  = processing_graph_t;


    bool buildEffectRoutingGraph(const vsthost* host, const project_t* project, const audio_stage_t* stage, std::shared_ptr<effect_graph_t>& out_graph);
    bool buildEffectProcessingGraph(const vsthost* host, const project_t* project, const audio_stage_t* stage, std::shared_ptr<effect_processing_graph_t>& out_procgraph);
    bool resolveEffectDefaultConnection(const vsthost* host, const project_t* project, const audio_stage_t* stage, effectbase* effect, channel_ref_t& out);
    bool validateEffectRoutings(const vsthost* host, audio_stage_t* tracksFlat);
}// namespace DAW
