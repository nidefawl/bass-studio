#include "host/automation/automation.hpp"
#include "math/seq_math.hpp"
#include "str_util.hpp"
#include "seq_util.hpp"
#include "seq_time.hpp"
#include "dsp_util.hpp"
#include "seq_util.hpp"
#include "host/project/project.hpp"
#include "host/track/track.hpp"
#include "host/audiohost/audio_host.hpp"
#include "assert_dbg.h"
#include "host/track/track_impl.hpp"
#include "track_graph.hpp"
#include "effect_graph.hpp"
#include "host/daw_channel.hpp"
#include "host/plugin/base/base-plugin.hpp"
#include "host/daw_channel.hpp"
#include "host/host_pluginmanager.hpp"
#include <vector>


namespace DAW {
    size_t validateEffectRouting(const Host::PluginManager* const host, const audio_stage_t* stage, const DAW::channel_desc& dstDesc, channel_ref_t& inputChannel) {
        size_t numRemoved = 0;
        if (inputChannel.getType() == stage_type::INPUT_DEFAULT) {
            inputChannel = ChannelDefaultNone();
        } else if (inputChannel.getType() == stage_type::INPUT_EMPTY) {
            inputChannel = ChannelDefaultNone();
        } else if (inputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO) {
            String name  = "External " + AudioIO::getExternalIOName(inputChannel.externalInputType, inputChannel.externalInputIdx, stage_bufferpoint::INPUT);
            inputChannel = ChannelAudioInput(inputChannel.externalInputIdx, inputChannel.srcChannelOffset, name, inputChannel.externalInputType);
        } else if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE) {
            auto* srcstage = host->getAudioStage(inputChannel.stage.stageRef);
            if (!srcstage) {
                log_lf(Log::L_WARN, "Input audiostage with id %d not found\n", static_cast<int32_t>(inputChannel.stage.stageRef.stageId));
                inputChannel = ChannelNone();
                numRemoved++;
            } else if (stage != srcstage) {
                log_lf(Log::L_WARN, "Input audiostage with id %d not on same track\n", static_cast<int32_t>(inputChannel.stage.stageRef.stageId));
                inputChannel = ChannelNone();
                numRemoved++;
            } else {
                inputChannel = ChannelStage(srcstage, inputChannel.stage.buffer, inputChannel.srcChannelOffset, inputChannel.dstChannelOffset);
            }
        } else if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE_EFFECT) {
            auto* eff = host->getPluginById(inputChannel.projectGlobalId);
            if (!eff) {
                log_lf(Log::L_WARN, "Input effect with id %d not found\n", inputChannel.projectGlobalId);
                inputChannel = ChannelNone();
                numRemoved++;
            } else if (stage != eff->getTrackLink()) {
                log_lf(Log::L_WARN, "Input effect with id %d not on same track\n", inputChannel.projectGlobalId);
                inputChannel = ChannelNone();
                numRemoved++;
            } else {
                auto& effChannels        = eff->outputChannelsDesc;
                auto it                  = std::find_if(effChannels.cbegin(), effChannels.cend(), [offset = inputChannel.srcChannelOffset](auto ch) {
                    return ch.offset == offset;
                });
                if (it != effChannels.cend()) {
                    inputChannel = ChannelAudioEffect(eff, stage_bufferpoint::OUTPUT_POST, *it, dstDesc);
                } else {
                    log_lf(Log::L_WARN, "Src Channel %d not found on effect %s\n", inputChannel.srcChannelOffset, StringAsCStr(eff->getName()));
                    inputChannel = ChannelNone();
                    numRemoved++;
                }
            }
        } else {
            dbgassert(inputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
            // inputChannel.stage.stageRef.stageId = TRACKID_INVALID_I32; //FIX: old project files have stageId == 0
        }
        return numRemoved;
    }
    bool validateEffectRoutings(const Host::PluginManager* const host, audio_stage_t* stage) {
        size_t numRemoved = 0;
        for (effectbase* effect : stage->effects) {
            for (auto& inputChannel : effect->inputChannels) {
                if (effect->inputChannelsDesc.empty()) {
                    log_lf(Log::L_WARN, "0 Input channels on effect %s\n", StringAsCStr(effect->getName()));
                    inputChannel = ChannelNone();
                    numRemoved++;
                    continue;
                }
                auto itOwnInput = std::find_if(effect->inputChannelsDesc.cbegin(), effect->inputChannelsDesc.cend(), [offset = inputChannel.dstChannelOffset](auto ch) {
                    return ch.offset == offset;
                });
                if (itOwnInput == effect->inputChannelsDesc.cend()) {
                    log_lf(Log::L_WARN, "Dst Channel %d not found on effect %s\n", inputChannel.dstChannelOffset, StringAsCStr(effect->getName()));
                    inputChannel = ChannelNone();
                    numRemoved++;
                    continue;
                }
                numRemoved += validateEffectRouting(host, stage, *itOwnInput, inputChannel);
            }
            auto it = std::remove_if(effect->inputChannels.begin(), effect->inputChannels.end(), [](auto& ch) {
                return ch.type == stage_type::INPUT_EMPTY;
            });
            effect->inputChannels.erase(it, effect->inputChannels.end());
        }
        DAW::channel_desc defaultDesc{};
        for (auto& inputChannel : stage->postEffectRouting) {
            numRemoved += validateEffectRouting(host, stage, defaultDesc, inputChannel);
        }
        auto it = std::remove_if(stage->postEffectRouting.begin(), stage->postEffectRouting.end(), [](auto& ch) {
            return ch.type == stage_type::INPUT_EMPTY;
        });
        stage->postEffectRouting.erase(it, stage->postEffectRouting.end());
        if (numRemoved) {
            log_lf(Log::L_WARN, "Removed %zu effect stage routings\n", numRemoved);
        }
        return numRemoved == 0;
    }
    struct dependency_effgraph_flattened_t {
        std::vector<effect_node_t*> resolved;
        std::vector<effect_node_t*> unresolved;
    };
    /**
     * Detect loops in graph
     *
     * @param ctxt
     * @param node
     * @return
     */
    bool dep_resolve_graph(dependency_effgraph_flattened_t& ctxt, effect_node_t* node) {
        ctxt.unresolved.push_back(node);
        for (auto child : node->children) {
            if (STL_CONTAINS(ctxt.unresolved, child)) {
                return false;
            }
            if (!dep_resolve_graph(ctxt, child)) {
                return false;
            }
        }
        /* do not add root node */
        if (node->stageId != TRACKID_INVALID_I32) {
            ctxt.resolved.push_back(node);
        }

        removeEntry(ctxt.unresolved, node);
        return true;
    }

    bool buildEffectProcessingGraph(const Host::PluginManager* const host, const project_t* const project, const audio_stage_t* stage, std::shared_ptr<effect_processing_graph_t>& out_procgraph) {
        std::shared_ptr<effect_graph_t> dependencyGraph;
        if (!buildEffectRoutingGraph(host, project, stage, dependencyGraph)) {
            // log_lf(Log::L_ERROR, "Failed building track graph\n");
            return false;
        }
        effect_node_t root;
        root.children.insert(root.children.begin(), dependencyGraph->roots.begin(), dependencyGraph->roots.end());
        dependency_effgraph_flattened_t graphFlattened;
        if (!dep_resolve_graph(graphFlattened, &root)) {
            // log_lf(Log::L_ERROR, "Failed flattening track graph\n");
            return false;
        }

        std::vector<const effect_node_t*> effsVisited;
        std::shared_ptr<effect_processing_graph_t> shrdPtrProcGraph = std::make_shared<effect_processing_graph_t>();
        processing_graph_t& graph = *(shrdPtrProcGraph);
        graph.trackGraph = dependencyGraph;
        graph.nodes.reserve(dependencyGraph->nodes.size());
        std::map<audiostageid_i32, processing_effect_node_t*> map;
        for (effect_node_ptr trackNode : dependencyGraph->nodes) {
            auto& procTrackNode = graph.memPoolProcNodes.push_back({});

            procTrackNode.type            = trackNode->type;
            procTrackNode.pulls           = trackNode->pulls;
            procTrackNode.dependencies    = trackNode->dependencies;
            procTrackNode.stageId         = trackNode->stageId;
            procTrackNode.internalLatency = trackNode->internalLatency;
            procTrackNode.inputLatency    = trackNode->inputLatency;
#ifdef VALIDATE_GRAPH_CORRECTNESS
            procTrackNode.inputLatency = INVALID_SAMPLE_OFFSET_U32;
#endif
            switch (trackNode->type) {
                case track_node_type_t::TRACK:
                    procTrackNode.trackOptional = nullptr;
                    dbgassert(0);
                    break;
                case track_node_type_t::AUDIOSTAGE:
                    procTrackNode.stage = host->getAudioStage(audio_stage_ref_t{ procTrackNode.stageId });
                    dbgassert(procTrackNode.stage);
                    // check if the input audio stage is on the same track
                    // pulling from other tracks is not supported yet
                    dbgassert(audioStageIdMatches(stage->stageId, procTrackNode.stageId));
                    break;
                case track_node_type_t::EFFECT:
                    procTrackNode.effectOptional = host->getPluginById(static_cast<int32_t>(trackNode->stageId));
                    dbgassert(procTrackNode.effectOptional);
                    break;
            }
            graph.nodes.push_back(&procTrackNode);
            map[trackNode->stageId] = &procTrackNode;
        }

        effsVisited.reserve(graph.nodes.size());
        for (const auto ptrTrackNode : graphFlattened.resolved) {
            if (STL_CONTAINS(effsVisited, ptrTrackNode)) {
                // expected
                continue;
            }
            effsVisited.push_back(ptrTrackNode);
            const auto& trackNode = *ptrTrackNode;
            dbgassert(map.contains(trackNode.stageId));
            auto procTrackNode = map[trackNode.stageId];
            dbgassert(procTrackNode);
            for (auto tnChild : trackNode.children) {
                auto procTrackNodeChild = map[tnChild->stageId];
                dbgassert(procTrackNodeChild);
                dbgassert(procTrackNodeChild->stageId != trackNode.stageId);
                procTrackNode->children.push_back(procTrackNodeChild);
            }

            for (auto tnParent : trackNode.parents) {
                auto procTrackNodeParent = map[tnParent->stageId];
                dbgassert(procTrackNodeParent);
                dbgassert(procTrackNodeParent->stageId != trackNode.stageId);
                procTrackNode->parents.push_back(procTrackNodeParent);
            }
            if (trackNode.dependencies.empty()) {
                graph.roots.push_back(procTrackNode);
            }
            graph.nodesFlatOrdered.push_back(procTrackNode);
        }

#ifdef VALIDATE_GRAPH_CORRECTNESS
        /* assert: dependency must lay before parent */
        for (auto itStageIdx = graph.nodesFlatOrdered.begin(); itStageIdx < graph.nodesFlatOrdered.end(); ++itStageIdx) {
            const effect_node_t& trackNode = *(*itStageIdx);
            for (auto depNodeIdx : trackNode.dependencies) {
                auto itDependency = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [depNodeIdx](const effect_node_t* ptr) {
                    return ptr->stageId == depNodeIdx;
                });

                if (itDependency >= itStageIdx) {
                    log_lf(Log::L_ERROR, "unexpected: dependecy index >= this index!!\n");
                }
                auto itDependencyChildren = std::find_if(trackNode.children.begin(), trackNode.children.end(), [depNodeIdx](const effect_node_t* ptr) {
                    return ptr->stageId == depNodeIdx;
                });
                dbgassert(itDependencyChildren != trackNode.children.end());
            }
        }
#endif

        /* Determine nodes accumulated inputLatency (own internalLatency + max_latency(all_children)) */
        /* This has to be done in bottom up/child first */
        samplecount_t maxLatency = 0;
        for (auto const ptrNode : graph.nodesFlatOrdered) {
            ptrNode->inputLatency = 0;
#ifdef VALIDATE_GRAPH_CORRECTNESS
            dbgassert(ptrNode->children.size() == ptrNode->dependencies.size());
            for (const auto trNodeChild : ptrNode->children) {
                dbgassert(ptrNode->stageId != trNodeChild->stageId);
                dbgassert(map.at(trNodeChild->stageId) == trNodeChild);
            }
#endif
            for (const auto nodeIdx : ptrNode->dependencies) {
                const auto ptrChNode = map.at(nodeIdx);
#ifdef VALIDATE_GRAPH_CORRECTNESS
                dbgassert(ptrChNode && ptrChNode->inputLatency != INVALID_SAMPLE_OFFSET_U32);
#endif
                ptrNode->inputLatency = std::max(ptrNode->inputLatency, ptrChNode->inputLatency + ptrChNode->internalLatency);
            }
            maxLatency = std::max(maxLatency, ptrNode->inputLatency + ptrNode->internalLatency);
#ifdef VALIDATE_GRAPH_CORRECTNESS
            for (auto const trNodeChild : ptrNode->children) {
                dbgassert(STL_CONTAINS(ptrNode->dependencies, trNodeChild->stageId));
                dbgassert(ptrNode->inputLatency >= trNodeChild->inputLatency + trNodeChild->internalLatency);
            }
#endif
        }
        graph.trackGraph->maxLatencySamples = maxLatency;
        /* Assign the resolved latencies to the previously populated push/pull inputs of each node */
        for (auto const ptrNode : graph.nodesFlatOrdered) {
            for (auto& pulls : ptrNode->pulls) {
                if (isChannelConnected(pulls.channel) && pulls.channel.getType() == stage_type::INPUT_AUDIOSTAGE_EFFECT) {
                    const auto effId = static_cast<audiostageid_i32>(pulls.channel.projectGlobalId);
                    const auto ptrChNode = map[effId];
                    dbgassert(ptrChNode);
                    pulls.latency  = ptrChNode->inputLatency + ptrChNode->internalLatency;
                }
            }
        }
        out_procgraph = shrdPtrProcGraph;
        return true;
    }

    bool buildEffectRoutingGraph(const Host::PluginManager* const host, const project_t* const project, const audio_stage_t* stage, std::shared_ptr<effect_graph_t>& out_graph) {
        auto shrdGraph = std::make_shared<effect_graph_t>();
        auto* const graph = shrdGraph.get();
        uint32_t trackEdgeId = 0;
        std::map<audiostageid_i32, effect_node_ptr> map;
        std::map<audiostageid_i32, effect_node_ptr> audioStageInputs;
        std::map<audiostageid_i32, effect_node_ptr> audioStageOutputs;
        auto makeOrGetTrackNode = [&](std::map<audiostageid_i32, effect_node_ptr>& map, track_node_type_t trackNodeType, audiostageid_i32 stageId, samplecount_t latency) -> effect_node_t& {
            if (map.count(stageId)) {
                return *map[stageId];
            }
            effect_node_t& trackCfg = graph->memPoolTrackNodes.push_back(effect_node_t{trackNodeType, stageId, latency});
            map[trackCfg.stageId] = &trackCfg;
            return trackCfg;
        };
        makeOrGetTrackNode(audioStageInputs, track_node_type_t::AUDIOSTAGE, stage->stageId.inputStageId, 0);
        makeOrGetTrackNode(audioStageOutputs, track_node_type_t::AUDIOSTAGE, stage->stageId.outputStageId, 0);
        for (effectbase* effect : stage->effects) {
            const auto effectIdI32 = static_cast<audiostageid_i32>(effect->projectGlobalId);
            effect_node_t& trackCfg = makeOrGetTrackNode(map, track_node_type_t::EFFECT, effectIdI32, effect->getPluginLatency());
            for (channel_ref_t inputChannel : effect->inputChannels) {
                if (inputChannel.type == stage_type::INPUT_DEFAULT) {
                    channel_ref_t tmp;
                    if (resolveEffectDefaultConnection(host, project, stage, effect, tmp)) {
                        inputChannel = tmp;
                    }
                }
                if (isChannelConnected(inputChannel)) {
                    if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE) {
                        audio_stage_t* src = host->getAudioStage(inputChannel.stage.stageRef);
                        dbgassert(src);
                        auto outputPostStageId = src->stageId.inputStageId;
                        effect_node_t& trackSrcCfg = makeOrGetTrackNode(audioStageInputs, track_node_type_t::AUDIOSTAGE, outputPostStageId, 0);
                        trackCfg.dependencies.push_back(outputPostStageId);
                        trackCfg.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationNone(), AutomationNone(), 0, src->flags });
                        trackCfg.children.push_back(&trackSrcCfg);
                        trackSrcCfg.parents.push_back(&trackCfg);

                    } else if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE_EFFECT) {
                        effectbase* effSrc = host->getPluginById(inputChannel.projectGlobalId);
                        if (effSrc) {
                            auto effSrcId_I32 = static_cast<audiostageid_i32>(effSrc->projectGlobalId);
                            effect_node_t& trackSrcCfg = makeOrGetTrackNode(map, track_node_type_t::EFFECT, effSrcId_I32, effSrc->getPluginLatency());
                            trackCfg.dependencies.push_back(effSrcId_I32);
                            trackCfg.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationNone(), AutomationNone(), 0, audiostageflags_t::NONE });
                            trackCfg.children.push_back(&trackSrcCfg);
                            trackSrcCfg.parents.push_back(&trackCfg);
                        }
                    } else if (inputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO) {
                        trackCfg.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationNone(), AutomationNone(), 0, audiostageflags_t::NONE });
                    } else {
                        log_lf(Log::L_ERROR, "missing track input routing\n");
                    }
                }
            }
        }

        effect_node_t& nodeOutput = makeOrGetTrackNode(audioStageOutputs, track_node_type_t::AUDIOSTAGE, stage->stageId.outputStageId, 0);
        for (channel_ref_t inputChannel : stage->postEffectRouting) {
            if (inputChannel.type == stage_type::INPUT_DEFAULT) {
                channel_ref_t tmp;
                if (resolveEffectDefaultConnection(host, project, stage, nullptr, tmp)) {
                    inputChannel = tmp;
                }
            }
            if (isChannelConnected(inputChannel)) {
                if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE) {
                    audio_stage_t* src = host->getAudioStage(inputChannel.stage.stageRef);
                    dbgassert(src);
                    auto outputPostStageId = src->stageId.inputStageId;
                    effect_node_t& trackSrcCfg = makeOrGetTrackNode(audioStageInputs, track_node_type_t::AUDIOSTAGE, outputPostStageId, 0);
                    nodeOutput.dependencies.push_back(outputPostStageId);
                    nodeOutput.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationNone(), AutomationNone(), 0, src->flags });
                    nodeOutput.children.push_back(&trackSrcCfg);
                    trackSrcCfg.parents.push_back(&nodeOutput);

                } else if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE_EFFECT) {
                    effectbase* effSrc = host->getPluginById(inputChannel.projectGlobalId);
                    if (effSrc) {
                        auto effSrcId_I32 = static_cast<audiostageid_i32>(effSrc->projectGlobalId);
                        effect_node_t& trackSrcCfg = makeOrGetTrackNode(map, track_node_type_t::EFFECT, effSrcId_I32, effSrc->getPluginLatency());
                        nodeOutput.dependencies.push_back(effSrcId_I32);
                        nodeOutput.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationNone(), AutomationNone(), 0, audiostageflags_t::NONE });
                        nodeOutput.children.push_back(&trackSrcCfg);
                        trackSrcCfg.parents.push_back(&nodeOutput);
                    }
                } else if (inputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO) {
                    nodeOutput.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationNone(), AutomationNone(), 0, audiostageflags_t::NONE });
                } else {
                    log_lf(Log::L_ERROR, "missing track input routing\n");
                }
            }
        }

        graph->nodes.reserve(map.size() + audioStageOutputs.size() + audioStageInputs.size());
        auto maps = {&map, &audioStageOutputs, &audioStageInputs};
        for (const auto* nodeMap : maps) {
            for (const auto& mappedNode : *nodeMap) {
                auto node = mappedNode.second;
                if (node->parents.empty()) {
                    graph->roots.push_back(node);
                }
                stl_remove_duplicates(node->children);
                stl_remove_duplicates(node->parents);
                stl_remove_duplicates(node->dependencies);
                graph->nodes.push_back(node);
            }
        }
        //if (gEnableLog) {
        //    for (effect_node_ptr& ptr : trackGraph->nodes) {
        //        for (int32_t src : ptr->dependencies) {
        //            log_lf(Log::L_DEBUG, "%d => %d\n", src, ptr->projectGlobalId);
        //        }
        //    }
        //}
        out_graph = shrdGraph;

        return true;
    }
}
