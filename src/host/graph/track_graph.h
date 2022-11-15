#pragma once
#include "host/track/track.h"
#include "host/daw_channel.h"
#include "assert_dbg.h"
#include "host/track/track_types.h"
#include "types.h"
#include <vector>

class track_t;
class effectbase;
struct audio_stage_t;
namespace DAW {

    /**
     * track_node_t - represents a node in the audio chain dependency graph
     */
    struct track_source_t {
        uint32_t trackEdgeId = 0;
        channel_ref_t channel{};
        automation_routing_t gainAutomation{};
        automation_routing_t panAutomation{};
        samplerate_t latency     = 0U;
        audiostageflags_t flags  = audiostageflags_t::NONE;
    };

    enum class track_node_type_t : int32_t {
        TRACK = 0,
        AUDIOSTAGE,
        EFFECT
    };
    struct track_node_t {
        track_node_type_t type   = track_node_type_t::TRACK;
        audiostageid_i32 stageId = TRACKID_INVALID_I32;
        std::vector<audiostageid_i32> dependencies;
        std::vector<track_source_t> pulls;
        std::vector<track_source_t> pushs;
        std::vector<track_node_t*> parents;
        std::vector<track_node_t*> children;
        samplecount_t internalLatency = INVALID_SAMPLE_OFFSET_U32;
        samplecount_t inputLatency    = INVALID_SAMPLE_OFFSET_U32;

        track_node_t() = default;
        track_node_t(track_node_type_t _type, audiostageid_i32 _stageId, samplecount_t _internalLatency)
            : type(_type), stageId(_stageId), internalLatency(_internalLatency) {
        }
    };
    enum class processing_track_node_state_t {
        UNPROCESSED = 0,
        PROCESSING,
        PROCESSED
    };
    struct processing_track_node_t final : public track_node_t {
        processing_track_node_t()  = default;
        track_t* trackOptional     = nullptr;
        effectbase* effectOptional = nullptr;
        audio_stage_t* stage       = nullptr;
        processing_track_node_state_t state = processing_track_node_state_t::UNPROCESSED;
    };

    using track_node_ptr            = track_node_t*;
    using processing_track_node_ptr = processing_track_node_t*;

    /**
     * track_graph_t - represents the audio chain dependency graph build from I/O configuration of all loaded tracks
     *
     */
    struct track_graph_t {
        std::vector<track_node_t*> roots;// output nodes (Master, )
        std::vector<track_node_ptr> nodes;
        std::vector<track_source_t> externalOutputRouting;
        samplecount_t maxLatencySamples = 0U;

        track_graph_t() = default;
        track_graph_t(const track_graph_t& graph) = delete;
        track_graph_t& operator=(const track_graph_t& graph) = delete;
        ~track_graph_t() {
            for (auto ptr : nodes) {
                delete ptr;
            }
        }
    };
    struct processing_graph_t {
        std::vector<processing_track_node_t*> nodesSolo;
        std::vector<processing_track_node_t*> nodesFlatOrdered;
        std::vector<processing_track_node_t*> roots; // audio_stage output buffer
        std::vector<processing_track_node_ptr> nodes;// audio_stage input buffer, effects, audio_stage output buffer
        std::shared_ptr<track_graph_t> trackGraph;
        ~processing_graph_t();
        processing_graph_t()                                = default;
        processing_graph_t(const processing_graph_t& graph) = delete;
        processing_graph_t& operator=(const processing_graph_t& graph) = delete;
    };

    inline const processing_track_node_t* getNodeConst(const std::vector<processing_track_node_t*>& nodes, audiostageid_i32 stageId) {
        for (const auto* ptr : nodes) {
            if (ptr->stageId == stageId) {
                return ptr;
            }
        }
        return nullptr;
    }
    inline processing_track_node_t* getNode(const std::vector<processing_track_node_t*>& nodes, audiostageid_i32 stageId) {
        for (auto* ptr : nodes) {
            if (ptr->stageId == stageId) {
                return ptr;
            }
        }
        return nullptr;
    }


    bool removeTrackRoutings(const track_vector& tracksFlat, audiostageid_i32 stageId);
    /**
     * @brief Builds the Directed acyclic graph using track list and track routings
     * 
     * @param host 
     * @param project 
     * @param tracksFlat 
     * @param out_graph 
     * @return true graph successfully build
     * @return false Failed building graph: Cycles detected or something else went wrong
     */
    bool buildTrackRoutingGraph(const Host::Host* host, const project_t* project, const track_vector& tracksFlat, std::shared_ptr<track_graph_t>& out_graph);
    
    /**
     * @brief Builds the Directed acyclic graph using track list and track routings
     *        and converts it into a structure for processing
     * @param host 
     * @param project 
     * @param tracksFlat 
     * @param out_procgraph 
     * @return true graph successfully build
     * @return false Failed building graph: Cycles detected or something else went wrong
     */
    bool buildProcessingGraph(const Host::Host* host, const project_t* project, const track_vector& tracksFlat, std::shared_ptr<processing_graph_t>& out_procgraph);
    bool validateTrackRoutings(const Host::Host* host, const track_vector& tracksFlat);
    int32_t GetUnqiueProcessingNodeId(const DAW::processing_track_node_t& node);

    void updateSoloFlag(const Host::Host* host, const project_t* project, const track_vector& tracksFlat);
    void unsoloAll(const Host::Host* host, const project_t* project, const track_vector& tracksFlat);
}// namespace DAW
