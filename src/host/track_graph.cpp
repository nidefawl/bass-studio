#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "dsp_util.h"

#include "project.h"
#include "vst_host.h"
#include "track.h"
#include "audio_host.h"
#include "assert_dbg.h"
#include "track_impl.h"
#include "track_graph.h"
#include "daw_channel.h"
#include <vector>
#include <deque>


namespace DAW {
    bool gEnableLog = 0;
    bool validateTrackRoutings(const vsthost* const host, const track_vector& tracksFlat) {
        size_t numRemoved = 0;
        for (track_t* track : tracksFlat) {
            track_impl_t* trackImpl  = track->getStage();
            const auto inputChannel  = trackImpl->inputChannel;
            const auto outputChannel = trackImpl->outputChannel;
            if (inputChannel.getType() == channel_input_type::INPUT_EXTERNAL_AUDIO) {
                int32_t idx             = inputChannel.externalInputIdx;
                String name             = "External " + AudioIO::getTrackNameShort(inputChannel.externalInputType, idx, stagebuffer_point::INPUT);
                trackImpl->inputChannel = ChannelAudioInput(idx, inputChannel.inputChannelOffset, name, inputChannel.externalInputType);
            } else if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
                auto* stage = host->getAudioStage(inputChannel.stage.stageRef);
                if (!stage) {
                    log_printf("Input audiostage with id %d not found\n", inputChannel.stage.stageRef);
                    trackImpl->inputChannel = ChannelNone();
                    numRemoved++;
                } else {
                    trackImpl->inputChannel = DAW::ChannelStage(stage, stagebuffer_point::OUTPUT_POST);
                }
            } else {
                dbgassert(inputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
                //inputChannel.stage.stageRef.stageId = TRACKID_INVALID_I32; //FIX: old project files have stageId == 0
            }
            if (outputChannel.getType() == channel_input_type::INPUT_EXTERNAL_AUDIO) {
                int32_t idx = outputChannel.externalInputIdx;
                String name = "External " + AudioIO::getTrackNameShort(outputChannel.externalInputType, idx, stagebuffer_point::OUTPUT_POST);

                trackImpl->outputChannel = ChannelAudioInput(idx, outputChannel.inputChannelOffset, name, outputChannel.externalInputType);
            } else if (outputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
                auto* stage = host->getAudioStage(outputChannel.stage.stageRef);
                if (!stage) {
                    log_printf("Output audiostage with id %d not found\n", outputChannel.stage.stageRef);
                    trackImpl->outputChannel = ChannelNone();
                    numRemoved++;
                } else {
                    trackImpl->outputChannel = DAW::ChannelStage(stage, stagebuffer_point::INPUT);
                }
            } else {
                dbgassert(outputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
                //outputChannel.stage.stageRef.stageId = TRACKID_INVALID_I32; //FIX: old project files have stageId == 0
            }
        }
        return numRemoved == 0;
    }
    bool removeTrackRoutings(const track_vector& tracksFlat, const audiostageid_i32 stageId) {
        size_t numRemoved = 0;
        for (track_t* track : tracksFlat) {

            track_impl_t* trackImpl  = track->getStage();
            const auto inputChannel  = trackImpl->inputChannel;
            const auto outputChannel = trackImpl->outputChannel;
            if (inputChannel.getType() != channel_input_type::INPUT_DEFAULT && isChannelConnected(inputChannel)) {
                if (inputChannel.stage.stageRef.stageId == stageId) {
                    trackImpl->inputChannel = ChannelNone();
                    numRemoved++;
                }
            } else {
                dbgassert(inputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
            }
            if (outputChannel.getType() != channel_input_type::INPUT_DEFAULT && isChannelConnected(outputChannel)) {
                if (inputChannel.stage.stageRef.stageId == stageId) {
                    trackImpl->outputChannel = ChannelNone();
                    numRemoved++;
                }
            } else {
                dbgassert(outputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
            }
        }
        return numRemoved > 0;
    }
    struct dependency_graph_flattened_t {
        std::vector<track_node_t*> resolved;
        std::vector<track_node_t*> unresolved;
    };
    /**
     * Detect loops in graph
     *
     * @param ctxt
     * @param node
     * @return
     */
    bool dep_resolve(dependency_graph_flattened_t& ctxt, track_node_t* node) {
        ctxt.unresolved.push_back(node);
        for (auto child : node->children) {
            if (STL_CONTAINS(ctxt.unresolved, child)) {
                return false;
            }
            if (!dep_resolve(ctxt, child)) {
                return false;
            }
        }
        if (node->stageId != TRACKID_INVALID_I32) {
            ctxt.resolved.push_back(node);
        }

        removeEntry(ctxt.unresolved, node);
        return true;
    }
    
    bool buildProcessingGraph(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat, std::shared_ptr<processing_graph_t>& out_procgraph) {
        std::shared_ptr<DAW::track_graph_t> dependencyGraph;
        if (!DAW::buildTrackRoutingGraph(host, project, tracksFlat, dependencyGraph)) {
            log_printf("Failed building track graph\n");
            return false;
        }
        track_node_t root;
        root.children.insert(root.children.begin(), dependencyGraph->roots.begin(), dependencyGraph->roots.end());
        dependency_graph_flattened_t graphFlattened;
        if (!dep_resolve(graphFlattened, &root)) {
            log_printf("Failed flattening track graph\n");
            return false;
        }

        track_vector tracksVisited;
        std::shared_ptr<processing_graph_t> shrdPtrProcGraph = std::make_shared<processing_graph_t>();
        //std::shared_ptr<processing_graph_t> shrdPtrProcGraph(new processing_graph_t(), [](processing_graph_t *gr) {
        //  log_lf(Log::L_DEBUG, "free proc_graph %08X\n", reinterpret_cast<uint64_t>(gr));
        //});

        shrdPtrProcGraph->nodes.reserve(dependencyGraph->nodes.size());
        shrdPtrProcGraph->trackGraph = dependencyGraph;
        for (track_node_ptr trackNode : dependencyGraph->nodes) {
            audio_stage_t* audioStage = host->getAudioStage(audio_stage_ref_t{ trackNode->stageId });
            dbgassert(audioStage);
            track_t* const track = audioStage->getTrack();
            dbgassert(track);
            auto procTrackNode = new processing_track_node_t();//std::make_unique<processing_track_node_t>();

            //procTrackNode->children = trackNode->children;
            //procTrackNode->parents = trackNode->parents;


            procTrackNode->type            = trackNode->type;
            procTrackNode->pushs           = trackNode->pushs;
            procTrackNode->pulls           = trackNode->pulls;
            procTrackNode->dependencies    = trackNode->dependencies;
            procTrackNode->stageId         = trackNode->stageId;
            procTrackNode->internalLatency = trackNode->internalLatency;
            procTrackNode->inputLatency    = trackNode->inputLatency;
            procTrackNode->trackOptional   = track;
            shrdPtrProcGraph->nodes.push_back(procTrackNode);
        }
        processing_graph_t& graph = *(shrdPtrProcGraph);

        for (const track_node_t* const ptrTrackNode : graphFlattened.resolved) {
            const DAW::track_node_t& trackNode = *ptrTrackNode;
            audiostageid_i32 nodeIdx           = trackNode.stageId;
            audio_stage_t* audioStage          = host->getAudioStage(audio_stage_ref_t{ nodeIdx });
            dbgassert(audioStage);
            track_t* const track = audioStage->getTrack();
            dbgassert(track);
            if (STL_CONTAINS(tracksVisited, track)) {
                // expected
                continue;
            }
            tracksVisited.push_back(track);
            auto itProcGraphNode = std::find_if(graph.nodes.begin(), graph.nodes.end(), [nodeIdx](const processing_track_node_ptr& ptr) {
                return ptr->stageId == nodeIdx;
            });
            dbgassert(itProcGraphNode != graph.nodes.end());
            processing_track_node_ptr& procTrackNode = *itProcGraphNode;

            for (track_node_t* tnChild : trackNode.children) {
                auto itProcGraphNodeChild = std::find_if(graph.nodes.begin(), graph.nodes.end(), [nodeIdx = tnChild->stageId](const processing_track_node_ptr& ptr) {
                    return ptr->stageId == nodeIdx;
                });
                dbgassert(itProcGraphNodeChild != graph.nodes.end());
                processing_track_node_ptr& procTrackNodeChild = *itProcGraphNodeChild;
                dbgassert(procTrackNodeChild->stageId != trackNode.stageId);
                procTrackNode->children.push_back(procTrackNodeChild);
            }


            for (track_node_t* tnParent : trackNode.parents) {
                auto itProcGraphNodeParent = std::find_if(graph.nodes.begin(), graph.nodes.end(), [nodeIdx = tnParent->stageId](const processing_track_node_ptr& ptr) {
                    return ptr->stageId == nodeIdx;
                });
                dbgassert(itProcGraphNodeParent != graph.nodes.end());
                processing_track_node_ptr& procTrackNodeParent = *itProcGraphNodeParent;
                dbgassert(procTrackNodeParent->stageId != trackNode.stageId);
                procTrackNode->parents.push_back(procTrackNodeParent);
            }
            if (trackNode.dependencies.empty()) {
                graph.roots.push_back(procTrackNode);
            }
            graph.nodesFlatOrdered.push_back(procTrackNode);
        }

#ifndef NDEBUG
        /* assert: dependency must lay before parent */
        for (auto itStageIdx = graph.nodesFlatOrdered.begin(); itStageIdx < graph.nodesFlatOrdered.end(); ++itStageIdx) {
            const DAW::track_node_t& trackNode = *(*itStageIdx);
            for (auto depNodeIdx : trackNode.dependencies) {
                auto itDependency = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [depNodeIdx](const track_node_t* ptr) {
                    return ptr->stageId == depNodeIdx;
                });

                if (itDependency >= itStageIdx) {
                    log_printf("unexpected: dependecy index >= this index!!\n");
                }
                auto itDependencyChildren = std::find_if(trackNode.children.begin(), trackNode.children.end(), [depNodeIdx](const track_node_t* ptr) {
                    return ptr->stageId == depNodeIdx;
                });
                dbgassert(itDependencyChildren != trackNode.children.end());
            }
        }
        for (auto itNodes = graph.nodes.begin(); itNodes != graph.nodes.end(); itNodes++) {
            auto ptrNode          = *itNodes;
            ptrNode->inputLatency = INVALID_SAMPLE_OFFSET_U32;
        }
#endif

        /**
         * Determine nodes accumulated inputLatency (max_output_latency(all_children))
         * Where child_output_latency = pChild->inputLatency + pChild->internalLatency
         * This has to be done in bottom up/child first order
         */
        for (auto itNodes = graph.nodesFlatOrdered.begin(); itNodes != graph.nodesFlatOrdered.end(); itNodes++) {
            auto ptrNode          = *itNodes;
            ptrNode->inputLatency = 0;
            dbgassert(ptrNode->children.size() == ptrNode->dependencies.size());
            for (DAW::track_node_t* trNodeChild : ptrNode->children) {
                dbgassert(ptrNode->stageId != trNodeChild->stageId);
                auto itProcNode = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [nodeIdx = trNodeChild->stageId](processing_track_node_t* ptr) {
                    return ptr->stageId == nodeIdx;
                });
                dbgassert(itProcNode != graph.nodesFlatOrdered.end());
                auto ptrChNode = *itProcNode;
                dbgassert(ptrChNode == trNodeChild);
            }
            for (audiostageid_i32 nodeIdx : ptrNode->dependencies) {
                auto itProcNode = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [nodeIdx](processing_track_node_t* ptr) {
                    return ptr->stageId == nodeIdx;
                });
                dbgassert(itProcNode != graph.nodesFlatOrdered.end());
                auto ptrChNode = *itProcNode;
#ifndef NDEBUG
                dbgassert(ptrChNode->inputLatency != INVALID_SAMPLE_OFFSET_U32);
#endif
                ptrNode->inputLatency = std::max(ptrNode->inputLatency, ptrChNode->inputLatency + ptrChNode->internalLatency);
            }
            auto stage = host->getAudioStage(audio_stage_ref_t{ ptrNode->stageId });
            dbgassert(stage);
            stage->latencyInput = ptrNode->inputLatency;
            stage->latencyOuput = stage->latencyInput + stage->getInternalLatency();
            for (audiostageid_i32 nodeIdx : ptrNode->dependencies) {
                auto itProcNode = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [nodeIdx](processing_track_node_t* ptr) {
                    return ptr->stageId == nodeIdx;
                });
                dbgassert(itProcNode != graph.nodesFlatOrdered.end());
                auto ptrChNode = *itProcNode;
                dbgassert(ptrNode->inputLatency >= ptrChNode->inputLatency + ptrChNode->internalLatency);
            }
            for (DAW::track_node_t* trNodeChild : ptrNode->children) {
                dbgassert(STL_CONTAINS(ptrNode->dependencies, trNodeChild->stageId));

                dbgassert(ptrNode->inputLatency >= trNodeChild->inputLatency + trNodeChild->internalLatency);
            }
        }
        /* Assign the resolved latencies to the previously populated push/pull inputs of each node */
        for (auto itNodes = graph.nodesFlatOrdered.begin(); itNodes != graph.nodesFlatOrdered.end(); itNodes++) {
            auto ptrNode = *itNodes;
            for (auto& push : ptrNode->pushs) {
                if (DAW::isChannelConnected(push.channel) && push.channel.getType() == DAW::channel_input_type::INPUT_AUDIOSTAGE) {
                    const auto nodeIdx = push.channel.stage.stageRef.stageId;
                    auto itStageIdx    = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [nodeIdx](processing_track_node_t* ptr) {
                        return ptr->stageId == nodeIdx;
                       });
                    dbgassert(itStageIdx != graph.nodesFlatOrdered.end());
                    auto ptrChNode = *itStageIdx;
                    push.latency   = ptrChNode->inputLatency + ptrChNode->internalLatency;
                }
            }
            for (auto& pulls : ptrNode->pulls) {
                if (DAW::isChannelConnected(pulls.channel) && pulls.channel.getType() == DAW::channel_input_type::INPUT_AUDIOSTAGE) {
                    const auto nodeIdx = pulls.channel.stage.stageRef.stageId;
                    auto itStageIdx    = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [nodeIdx](processing_track_node_t* ptr) {
                        return ptr->stageId == nodeIdx;
                       });
                    dbgassert(itStageIdx != graph.nodesFlatOrdered.end());
                    auto ptrChNode = *itStageIdx;
                    pulls.latency  = ptrChNode->inputLatency + ptrChNode->internalLatency;
                }
            }
        }
        out_procgraph = shrdPtrProcGraph;
        return true;
    }
    track_node_ptr makeTrackNode(audiostageid_i32 a, samplerate_t b) {
        return new track_node_t(track_node_type_t::TRACK, a, b);
    }
    processing_track_node_ptr makeProcTrackNode() {
        return new processing_track_node_t();
    }
    template<typename M, typename I>
    inline track_node_t& getNode(M map, I idx) {
        return *map[idx];
    }
    void updateSoloFlag(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat) {
        std::shared_ptr<DAW::track_graph_t> dependencyGraph;
        if (!DAW::buildTrackRoutingGraph(host, project, tracksFlat, dependencyGraph)) {
            log_printf("Failed building track graph\n");
            return;
        }
        for (track_t* track : tracksFlat) {
            audiostageflags_t& flags = track->getStage()->flags;
            flags &= ~audiostageflags_t::SOLO_PARENT;
        }
        for (track_node_ptr& node : dependencyGraph->nodes) {
            auto stage = host->getAudioStage(audio_stage_ref_t{ node->stageId });
            dbgassert(stage);
            if ((stage->flags & audiostageflags_t::SOLO) != audiostageflags_t::NONE) {
                std::deque<track_node_t*> parents;
                parents.insert(parents.end(), begin(node->parents), end(node->parents));
                while (!parents.empty()) {
                    track_node_t* fr = parents.front();
                    parents.pop_front();
                    auto stage2 = host->getAudioStage(audio_stage_ref_t{ fr->stageId });
                    dbgassert(stage2);
                    stage2->flags |= audiostageflags_t::SOLO_PARENT;
                    parents.insert(parents.end(), begin(fr->parents), end(fr->parents));
                }
            }
        }
    }
    bool buildTrackRoutingGraph(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat, std::shared_ptr<track_graph_t>& out_graph) {
        uint32_t trackEdgeId = 0;
        std::map<audiostageid_i32, track_node_ptr> map;
        for (track_t* track : tracksFlat) {
            track_impl_t* trackImpl = track->getStage();
            auto stageId            = trackImpl->stageId.stageId;
            if (!map.count(stageId)) {
                map[stageId] = makeTrackNode(stageId, trackImpl->getInternalLatency());
            }
            track_node_t& trackCfg = getNode(map, stageId);

            auto inputChannel  = trackImpl->inputChannel;
            auto outputChannel = trackImpl->outputChannel;

            if (inputChannel.type == channel_input_type::INPUT_DEFAULT) {
                channel_ref_t tmp;
                if (DAW::resolveDefaultConnection(host, project, trackImpl, true, tmp)) {
                    inputChannel = tmp;
                } else {
                    //log_printf("Default input of stage #%d cannot be mapped\n", stageId);
                }
            }
            if (outputChannel.type == channel_input_type::INPUT_DEFAULT) {
                channel_ref_t tmp;
                if (DAW::resolveDefaultConnection(host, project, trackImpl, false, tmp)) {
                    outputChannel = tmp;
                } else {
                    //log_printf("Default output of stage #%d cannot be mapped\n", stageId);
                }
            }
            if (isChannelConnected(inputChannel)) {
                if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
                    audio_stage_t* src = host->getAudioStage(inputChannel.stage.stageRef);
                    dbgassert(src);
                    auto srcStageId = src->stageId.stageId;
                    if (!map.count(srcStageId)) {
                        map[srcStageId] = makeTrackNode(srcStageId, src->getInternalLatency());
                    }
                    track_node_t& trackSrcCfg = getNode(map, srcStageId);
                    trackCfg.dependencies.push_back(srcStageId);
                    trackCfg.pulls.push_back(DAW::track_source_t{ trackEdgeId++, inputChannel, DAW::AutomationConstant(dsp_util::gainToLinScale(1.0f)), 0, src->flags });
                    trackCfg.children.push_back(&trackSrcCfg);
                    trackSrcCfg.parents.push_back(&trackCfg);
                } else if (inputChannel.getType() == channel_input_type::INPUT_EXTERNAL_AUDIO) {
                    trackCfg.pulls.push_back(DAW::track_source_t{ trackEdgeId++, inputChannel, DAW::AutomationConstant(dsp_util::gainToLinScale(1.0f)), 0, audiostageflags_t::NONE });
                } else if (inputChannel.type != channel_input_type::INPUT_DEFAULT) {
                    log_printf("missing track input routing on track %s\n", StringAsCStr(track->name));
                }
            }
            if (isChannelConnected(outputChannel)) {
                if (outputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE && trackImpl->mixer.isEnabled()) {
                    audio_stage_t* dst = host->getAudioStage(outputChannel.stage.stageRef);
                    if (!dst) {
                        log_printf("missing track output routing on track %s\n", StringAsCStr(track->name));
                    } else {
                        auto dstStageId = dst->stageId.stageId;
                        if (!map.count(dstStageId)) {
                            map[dstStageId] = makeTrackNode(dstStageId, dst->getInternalLatency());
                        }
                        track_node_t& trackDstCfg = getNode(map, dstStageId);
                        trackDstCfg.dependencies.push_back(stageId);
                        trackDstCfg.pushs.push_back(DAW::track_source_t{ trackEdgeId++, ChannelStage(trackImpl, stagebuffer_point::OUTPUT_POST), DAW::AutomationConstant(dsp_util::gainToLinScale(1.0f)), 0, trackImpl->flags });
                        trackDstCfg.children.push_back(&trackCfg);
                        trackCfg.parents.push_back(&trackDstCfg);
                    }
                }
            }
            if (TRACKTYPE_TO_CTR(track->type) == TRACK_CTR_MIDIAUDIO && trackImpl->mixer.isEnabled()) {
                /* Feed audio/midi tracks output into returns input */
                for (track_t* trackReturn : project->trackReturnCtr) {
                    int32_t paramIdx         = PARAM_OFFSET_SEND + trackReturn->localIdxFlat;
                    auto sendLevelGainVal    = trackImpl->mixer.getParamValue(paramIdx);
                    auto automationRef       = DAW::AutomationConstant(sendLevelGainVal);
                    auto sendLevelAutomation = trackImpl->mixer.getRegisteredConstAutomation(paramIdx);
                    if (sendLevelAutomation) {
                        automationRef = DAW::AutomationRef(&trackImpl->mixer, paramIdx);
                    } else {
                        /* Calculate send gain level */
                        float fGainRaw = dsp_util::linScaleToGain(sendLevelGainVal);
                        if (fGainRaw < dsp_util::GAIN_DBFLOOR) {
                            continue;
                        }
                    }

                    track_impl_t* audioReturn = trackReturn->audio;
                    dbgassert(audioReturn);
                    auto srcStageId = audioReturn->stageId.stageId;

                    if (!map.count(srcStageId)) {
                        map[srcStageId] = makeTrackNode(srcStageId, audioReturn->getInternalLatency());
                    }
                    track_node_t& trackReturnCfg = getNode(map, srcStageId);
                    trackReturnCfg.dependencies.push_back(trackImpl->stageId.stageId);
                    trackReturnCfg.pushs.push_back(DAW::track_source_t{ trackEdgeId++, ChannelStage(trackImpl, stagebuffer_point::OUTPUT_POST), automationRef, 0, trackImpl->flags });
                    trackReturnCfg.children.push_back(&trackCfg);
                    trackCfg.parents.push_back(&trackReturnCfg);
                }
            }
        }
        auto trackGraph = std::make_shared<track_graph_t>();
        //std::shared_ptr<track_graph_t> trackGraph(new track_graph_t(), [](track_graph_t *gr) {
        //  log_lf(Log::L_DEBUG, "free track_graph %08X\n", reinterpret_cast<uint64_t>(gr));
        //});
        trackGraph->nodes.reserve(map.size());
        for (auto mapIt = map.begin(); mapIt != map.end(); ++mapIt) {
            track_node_ptr node = mapIt->second;
            if (node->parents.empty()) {
                trackGraph->roots.push_back(node);
            }
            trackGraph->nodes.push_back(std::move(mapIt->second));
        }
        if (gEnableLog) {
            for (track_node_ptr& ptr : trackGraph->nodes) {
                for (audiostageid_i32 src : ptr->dependencies) {
                    log_lf(Log::L_DEBUG, "%d => %d\n", src, ptr->stageId);
                }
            }
        }
        out_graph = trackGraph;

        return true;
    }
    processing_graph_t::~processing_graph_t() {
        for (auto ptr : nodes) {
            delete ptr;
        }
    }
}
