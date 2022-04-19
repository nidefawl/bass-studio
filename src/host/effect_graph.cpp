#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "dsp_util.h"

#include "project.h"
#include "vst_host.h"
#include "track.h"
#include "audio_host.h"
#include "assert_dbg.h"
#include "track_impl.h"
#include "track_graph.h"
#include "effect_graph.h"
#include "daw_channel.h"
#include "plugin/base_plugin.h"
#include "daw_channel.h"
#include <vector>


namespace DAW {
    size_t validateEffectRouting(const vsthost* const host, const DAW::channel_desc& dstDesc, channel_ref_t& inputChannel) {
        size_t numRemoved = 0;
        if (inputChannel.getType() == stage_type::INPUT_DEFAULT) {
            inputChannel = ChannelDefaultNone();
        } else if (inputChannel.getType() == stage_type::INPUT_EMPTY) {
            inputChannel = ChannelDefaultNone();
        } else if (inputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO) {
            String name  = "External " + AudioIO::getTrackNameShort(inputChannel.externalInputType, inputChannel.externalInputIdx, stage_bufferpoint::INPUT);
            inputChannel = ChannelAudioInput(inputChannel.externalInputIdx, inputChannel.srcChannelOffset, name, inputChannel.externalInputType);
        } else if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE) {
            auto* srcstage = host->getAudioStage(inputChannel.stage.stageRef);
            if (!srcstage) {
                log_lf(Log::L_WARN, "Input audiostage with id %d not found\n", inputChannel.stage.stageRef.stageId);
                inputChannel = ChannelNone();
                numRemoved++;
            } else {
                inputChannel = ChannelStage(srcstage, inputChannel.stage.buffer);
            }
        } else if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE_EFFECT) {
            auto* eff = host->getPluginById(inputChannel.projectGlobalId);
            if (!eff) {
                log_lf(Log::L_WARN, "Input effect with id %d not found\n", inputChannel.projectGlobalId);
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
    bool validateEffectRoutings(const vsthost* const host, audio_stage_t* stage) {
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
                numRemoved += validateEffectRouting(host, *itOwnInput, inputChannel);
            }
            auto it = std::remove_if(effect->inputChannels.begin(), effect->inputChannels.end(), [](auto& ch) {
                return ch.type == stage_type::INPUT_EMPTY;
            });
            effect->inputChannels.erase(it, effect->inputChannels.end());
        }
        DAW::channel_desc defaultDesc{};
        for (auto& inputChannel : stage->postEffectRouting) {
            numRemoved += validateEffectRouting(host, defaultDesc, inputChannel);
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
    struct dependency_graph_flattened_t {
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
    bool dep_resolve_graph(dependency_graph_flattened_t& ctxt, effect_node_t* node) {
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
    bool buildEffectProcessingGraph(const vsthost* const host, const project_t* const project, const audio_stage_t* stage, std::shared_ptr<effect_processing_graph_t>& out_procgraph) {
        std::shared_ptr<effect_graph_t> dependencyGraph;
        if (!buildEffectRoutingGraph(host, project, stage, dependencyGraph)) {
            log_lf(Log::L_ERROR, "Failed building track graph\n");
            return false;
        }
        effect_node_t root;
        root.children.insert(root.children.begin(), dependencyGraph->roots.begin(), dependencyGraph->roots.end());
        dependency_graph_flattened_t graphFlattened;
        if (!dep_resolve_graph(graphFlattened, &root)) {
            log_lf(Log::L_ERROR, "Failed flattening track graph\n");
            return false;
        }

        std::vector<const effect_node_t*> effsVisited;
        std::shared_ptr<effect_processing_graph_t> shrdPtrProcGraph = std::make_shared<effect_processing_graph_t>();
        shrdPtrProcGraph->nodes.reserve(dependencyGraph->nodes.size());
        shrdPtrProcGraph->trackGraph = dependencyGraph;
        for (effect_node_ptr trackNode : dependencyGraph->nodes) {
            auto const procTrackNode = new processing_effect_node_t();

            procTrackNode->type            = trackNode->type;
            procTrackNode->pulls           = trackNode->pulls;
            procTrackNode->dependencies    = trackNode->dependencies;
            procTrackNode->stageId         = trackNode->stageId;
            procTrackNode->internalLatency = trackNode->internalLatency;
            procTrackNode->inputLatency    = trackNode->inputLatency;
#ifndef NDEBUG
            procTrackNode->inputLatency = INVALID_SAMPLE_OFFSET_U32;
#endif
            switch (trackNode->type) {
                case track_node_type_t::TRACK:
                    procTrackNode->trackOptional = nullptr;
                    dbgassert(0);
                    break;
                case track_node_type_t::AUDIOSTAGE:
                    procTrackNode->stage = host->getAudioStage(audio_stage_ref_t{ procTrackNode->stageId });
                    dbgassert(procTrackNode->stage);
                    dbgassert(audioStageIdMatches(stage->stageId, procTrackNode->stageId));// for now
                    break;
                case track_node_type_t::EFFECT:
                    procTrackNode->effectOptional = host->getPluginById(static_cast<int32_t>(trackNode->stageId));
                    dbgassert(procTrackNode->effectOptional);
                    break;
            }
            shrdPtrProcGraph->nodes.push_back(procTrackNode);
        }

        effect_processing_graph_t& graph = *shrdPtrProcGraph;
        effsVisited.reserve(graph.nodes.size());
        for (const auto ptrTrackNode : graphFlattened.resolved) {
            if (STL_CONTAINS(effsVisited, ptrTrackNode)) {
                // expected
                continue;
            }
            effsVisited.push_back(ptrTrackNode);
            const auto& trackNode = *ptrTrackNode;
            const auto nodeIdx = trackNode.stageId;
            auto procTrackNode = getNode(graph.nodes, nodeIdx);
            dbgassert(procTrackNode);
            for (auto tnChild : trackNode.children) {
                auto procTrackNodeChild = getNode(graph.nodes, tnChild->stageId);
                dbgassert(procTrackNodeChild);
                dbgassert(procTrackNodeChild->stageId != trackNode.stageId);
                procTrackNode->children.push_back(procTrackNodeChild);
            }

            for (auto tnParent : trackNode.parents) {
                auto procTrackNodeParent = getNode(graph.nodes, tnParent->stageId);
                dbgassert(procTrackNodeParent);
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
        for (auto const ptrNode : graph.nodesFlatOrdered) {
            ptrNode->inputLatency = 0;
#ifndef NDEBUG
            dbgassert(ptrNode->children.size() == ptrNode->dependencies.size());
            for (const auto trNodeChild : ptrNode->children) {
                dbgassert(ptrNode->stageId != trNodeChild->stageId);
                dbgassert(getNodeConst(graph.nodesFlatOrdered, trNodeChild->stageId) == trNodeChild);
            }
#endif
            for (const auto nodeIdx : ptrNode->dependencies) {
                const auto ptrChNode = getNodeConst(graph.nodesFlatOrdered, nodeIdx);
#ifndef NDEBUG
                dbgassert(ptrChNode && ptrChNode->inputLatency != INVALID_SAMPLE_OFFSET_U32);
#endif
                ptrNode->inputLatency = std::max(ptrNode->inputLatency, ptrChNode->inputLatency + ptrChNode->internalLatency);
            }
#ifndef NDEBUG
            for (auto const trNodeChild : ptrNode->children) {
                dbgassert(STL_CONTAINS(ptrNode->dependencies, trNodeChild->stageId));
                dbgassert(ptrNode->inputLatency >= trNodeChild->inputLatency + trNodeChild->internalLatency);
            }
#endif
        }
        /* Assign the resolved latencies to the previously populated push/pull inputs of each node */
        for (auto const ptrNode : graph.nodesFlatOrdered) {
            for (auto& pulls : ptrNode->pulls) {
                if (isChannelConnected(pulls.channel) && pulls.channel.getType() == stage_type::INPUT_AUDIOSTAGE_EFFECT) {
                    const auto effId = static_cast<audiostageid_i32>(pulls.channel.projectGlobalId);
                    const auto ptrChNode = getNodeConst(graph.nodesFlatOrdered, effId);
                    dbgassert(ptrChNode);
                    pulls.latency  = ptrChNode->inputLatency + ptrChNode->internalLatency;
                }
            }
        }
        out_procgraph = shrdPtrProcGraph;
        return true;
    }
    effect_node_ptr makeEffectNode(int32_t effectId, samplecount_t latency) {
        return new effect_node_t(track_node_type_t::EFFECT, static_cast<audiostageid_i32>(effectId), latency);// std::make_unique<effect_node_t>
    }
    effect_node_ptr makeAudioStageNode(audiostageid_i32 stageId, samplecount_t latency) {
        return new effect_node_t(track_node_type_t::AUDIOSTAGE, stageId, latency);// std::make_unique<effect_node_t>
    }
    template<typename M, typename I>
    inline effect_node_t& getEffNode(M map, I idx) {
        return *map[idx];
    }
    bool buildEffectRoutingGraph(const vsthost* const host, const project_t* const project, const audio_stage_t* stage, std::shared_ptr<effect_graph_t>& out_graph) {
        uint32_t trackEdgeId = 0;
        std::map<audiostageid_i32, effect_node_ptr> map;
        std::map<audiostageid_i32, effect_node_ptr> audioStageInputs;
        std::map<audiostageid_i32, effect_node_ptr> audioStageOutputs;

        audioStageInputs[stage->stageId.inputStageId]   = makeAudioStageNode(stage->stageId.inputStageId, 0);
        audioStageOutputs[stage->stageId.outputStageId] = makeAudioStageNode(stage->stageId.outputStageId, 0);
        effect_node_t& nodeOutput                       = getEffNode(audioStageOutputs, stage->stageId.outputStageId);
        for (effectbase* effect : stage->effects) {
            const int32_t effectId = effect->projectGlobalId;
            const auto effectIdI32 = static_cast<audiostageid_i32>(effect->projectGlobalId);
            if (!map.count(effectIdI32)) {
                map[effectIdI32] = makeEffectNode(effectId, effect->getPluginLatency());
            }
            effect_node_t& trackCfg = getEffNode(map, effectIdI32);
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

                        // the correct dependency here would be the inputs routed to this track (the node of this track in the track graph)

                        if (!audioStageInputs.count(outputPostStageId)) {
                            audioStageInputs[outputPostStageId] = makeAudioStageNode(outputPostStageId, 0);
                        }
                        effect_node_t& trackSrcCfg = getEffNode(audioStageInputs, outputPostStageId);
                        trackCfg.dependencies.push_back(outputPostStageId);
                        trackCfg.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationConstant(dsp_util::gainToLinScale(1.0f)), 0, src->flags });
                        trackCfg.children.push_back(&trackSrcCfg);
                        trackSrcCfg.parents.push_back(&trackCfg);

                    } else if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE_EFFECT) {
                        effectbase* effSrc = host->getPluginById(inputChannel.projectGlobalId);
                        if (effSrc) {
                            auto effSrcId_I32 = static_cast<audiostageid_i32>(effSrc->projectGlobalId);
                            if (!map.count(effSrcId_I32)) {
                                map[effSrcId_I32] = makeEffectNode(effSrc->projectGlobalId, effSrc->getPluginLatency());
                            }
                            effect_node_t& trackSrcCfg = getEffNode(map, effSrcId_I32);
                            trackCfg.dependencies.push_back(effSrcId_I32);
                            trackCfg.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationConstant(dsp_util::gainToLinScale(1.0f)), 0, audiostageflags_t::NONE });
                            trackCfg.children.push_back(&trackSrcCfg);
                            trackSrcCfg.parents.push_back(&trackCfg);
                        }
                    } else if (inputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO) {
                        trackCfg.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationConstant(dsp_util::gainToLinScale(1.0f)), 0, audiostageflags_t::NONE });
                    } else {
                        log_lf(Log::L_ERROR, "missing track input routing\n");
                    }
                }
            }
        }

        //effect_node_t& trackCfg = getEffNode(audioStageOutputs, audioStageOutputs[TRACKID_DEFAULT_I32]);
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
                    // the correct dependency here would be the inputs routed to this track (the node of this track in the track graph)

                    if (!audioStageInputs.count(outputPostStageId)) {
                        audioStageInputs[outputPostStageId] = makeAudioStageNode(outputPostStageId, 0);
                    }
                    effect_node_t& trackSrcCfg = getEffNode(audioStageInputs, outputPostStageId);
                    nodeOutput.dependencies.push_back(outputPostStageId);
                    nodeOutput.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationConstant(dsp_util::gainToLinScale(1.0f)), 0, src->flags });
                    nodeOutput.children.push_back(&trackSrcCfg);
                    trackSrcCfg.parents.push_back(&nodeOutput);

                    //trackCfg.children.push_back(&trackSrcCfg);
                    //trackSrcCfg.parents.push_back(&trackCfg);

                } else if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE_EFFECT) {
                    effectbase* effSrc = host->getPluginById(inputChannel.projectGlobalId);
                    if (effSrc) {
                        auto effSrcId_I32 = static_cast<audiostageid_i32>(effSrc->projectGlobalId);
                        if (!map.count(effSrcId_I32)) {
                            map[effSrcId_I32] = makeEffectNode(effSrc->projectGlobalId, effSrc->getPluginLatency());
                        }
                        effect_node_t& trackSrcCfg = getEffNode(map, effSrcId_I32);
                        nodeOutput.dependencies.push_back(effSrcId_I32);
                        nodeOutput.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationConstant(dsp_util::gainToLinScale(1.0f)), 0, audiostageflags_t::NONE });
                        nodeOutput.children.push_back(&trackSrcCfg);
                        trackSrcCfg.parents.push_back(&nodeOutput);
                    }
                } else if (inputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO) {
                    nodeOutput.pulls.push_back(effect_source_t{ trackEdgeId++, inputChannel, AutomationConstant(dsp_util::gainToLinScale(1.0f)), 0, audiostageflags_t::NONE });
                } else {
                    log_lf(Log::L_ERROR, "missing track input routing\n");
                }
            }
        }

        auto trackGraph = std::make_shared<effect_graph_t>();
        trackGraph->nodes.reserve(map.size() + audioStageOutputs.size() + audioStageInputs.size());
        for (const auto& mappedNode : map) {
            if (mappedNode.second->parents.empty()) {
                trackGraph->roots.push_back(mappedNode.second);
            }
            trackGraph->nodes.push_back(mappedNode.second);
        }
        for (const auto& mappedNode : audioStageOutputs) {
            if (mappedNode.second->parents.empty()) {
                trackGraph->roots.push_back(mappedNode.second);
            }
            trackGraph->nodes.push_back(mappedNode.second);
        }
        for (const auto& mappedNode : audioStageInputs) {
            if (mappedNode.second->parents.empty()) {
                trackGraph->roots.push_back(mappedNode.second);
            }
            trackGraph->nodes.push_back(mappedNode.second);
        }
        //if (gEnableLog) {
        //    for (effect_node_ptr& ptr : trackGraph->nodes) {
        //        for (int32_t src : ptr->dependencies) {
        //            log_lf(Log::L_DEBUG, "%d => %d\n", src, ptr->projectGlobalId);
        //        }
        //    }
        //}
        out_graph = trackGraph;

        return true;
    }
}
