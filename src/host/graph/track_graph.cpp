#include "config.h"
#include "host/audio_config.h"
#include "host/midihost/midi_host.h"
#include "logging.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "dsp_util.h"

#include "host/project/project.h"
#include "tls.h"
#include "host/host_pluginmanager.h"
#include "host/track/track.h"
#include "host/audiohost/audio_host.h"
#include "assert_dbg.h"
#include "host/track/track_impl.h"
#include "track_graph.h"
#include "host/daw_channel.h"
#include "host/host.h"
#include <numeric>
#include <vector>
#include <deque>


namespace DAW {
    bool gEnableLog = 0;

    bool validateTrackRoutings(const Host::Host* const host, const track_vector& tracksFlat) {
        size_t numRemoved = 0;
        for (track_t* track : tracksFlat) {
            track_impl_t* trackImpl  = track->getStage();
            for (auto& midiInputChannel : trackImpl->midiInputChannels) {
                if (isMidiChannelConnected(midiInputChannel)) {
                    if (midiInputChannel.getType() == midistage_type::INPUT_AUDIOSTAGE) {
                        auto* stage = host->getAudioStage(midiInputChannel.stage.stageRef);
                        if (!stage) {
                            log_lf(Log::L_WARN, "Input midistage with id %d not found\n", static_cast<int32_t>(midiInputChannel.stage.stageRef.stageId));
                            midiInputChannel = MidiChannelNone();
                            numRemoved++;
                        } else {
                            midiInputChannel = MidiChannelStage(stage, midiInputChannel.stage.buffer, midiInputChannel.srcChannel, midiInputChannel.dstChannel);
                        }
                    } else if (midiInputChannel.getType() == midistage_type::INPUT_EXTERNAL_MIDI) {
                        // auto& settings = daw_tls::getSettings();
                        // auto& midiSettings = settings.iosettings.getIOConfigMidi("stdmidi");
                        // String labelAll = "All Inputs";
                        // String name = labelAll;
                        // if (midiInputChannel.externalInputIdx != 255) {
                        //     if (CtrSize(midiSettings.inputs) > midiInputChannel.externalInputIdx) {
                        //         auto& device = midiSettings.inputs[midiInputChannel.externalInputIdx];
                        //         name = device.deviceName;
                        //     } else {
                        //         name = StringFormat("Missing Device %d", midiInputChannel.externalInputIdx+1);
                        //     }
                        // }
                        // midiInputChannel = MidiChannelExternal(midiInputChannel.externalInputIdx, name, midiInputChannel.srcChannel, midiInputChannel.dstChannel);
                    } else if (midiInputChannel.getType() == midistage_type::INPUT_DEFAULT) {
                        midiInputChannel = MidiChannelDefault();
                    } else if (midiInputChannel.getType() == midistage_type::INPUT_EMPTY) {
                        midiInputChannel = MidiChannelNone();
                    } else {
                        log_lf(Log::L_WARN, "Unknown midi input channel type %d\n", static_cast<int32_t>(midiInputChannel.getType()));
                        midiInputChannel = MidiChannelNone();
                        numRemoved++;
                    }
                } else {
                    midiInputChannel = MidiChannelNone();
                }
                const auto inputChannel  = trackImpl->inputChannel;
                const auto outputChannel = trackImpl->outputChannel;
                if (inputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO) {
                    String name = "External " + AudioIO::getExternalIOName(inputChannel.externalInputType, inputChannel.externalInputIdx, stage_bufferpoint::INPUT);
                    trackImpl->inputChannel = ChannelAudioInput(inputChannel.externalInputIdx, inputChannel.srcChannelOffset, name, inputChannel.externalInputType);
                } else if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE) {
                    auto* stage = host->getAudioStage(inputChannel.stage.stageRef);
                    if (!stage) {
                        log_lf(Log::L_WARN, "Input audiostage with id %d not found\n", static_cast<int32_t>(inputChannel.stage.stageRef.stageId));
                        trackImpl->inputChannel = ChannelNone();
                        numRemoved++;
                    } else {
                        trackImpl->inputChannel = ChannelStage(stage, stage_bufferpoint::OUTPUT_POST);
                    }
                } else {
                    dbgassert(inputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
                    //inputChannel.stage.stageRef.stageId = TRACKID_INVALID_I32; //FIX: old project files have stageId == 0
                }
                if (outputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO) {
                    int32_t idx = outputChannel.externalInputIdx;
                    String name = "External " + AudioIO::getExternalIOName(outputChannel.externalInputType, idx, stage_bufferpoint::OUTPUT_POST);

                    trackImpl->outputChannel = ChannelAudioInput(idx, outputChannel.srcChannelOffset, name, outputChannel.externalInputType);
                } else if (outputChannel.getType() == stage_type::INPUT_AUDIOSTAGE) {
                    auto* stage = host->getAudioStage(outputChannel.stage.stageRef);
                    if (!stage) {
                        log_lf(Log::L_WARN, "Output audiostage with id %d not found\n", static_cast<int32_t>(outputChannel.stage.stageRef.stageId));
                        trackImpl->outputChannel = ChannelNone();
                        numRemoved++;
                    } else {
                        //TODO: validate dstChannelOffset
                        trackImpl->outputChannel = ChannelStage(stage, stage_bufferpoint::INPUT, outputChannel.srcChannelOffset, outputChannel.dstChannelOffset); 
                    }
                } else {
                    dbgassert(outputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
                    //outputChannel.stage.stageRef.stageId = TRACKID_INVALID_I32; //FIX: old project files have stageId == 0
                }
            }
        }
        if (numRemoved) {
          log_lf(Log::L_WARN, "Removed %zu track routings\n", numRemoved);
        }
        return numRemoved == 0;
    }

    std::vector<removed_track_routings> removeTrackRoutings(const track_vector& tracksFlat, const audiostageid_i32 stageId) {
        std::vector<removed_track_routings> removedRoutings;
        for (track_t* track : tracksFlat) {
            size_t numRemoved = 0;
            track_impl_t* trackImpl  = track->getStage();
            removed_track_routings removed;
            removed.stageRef= trackImpl->toRef();
            const auto inputChannel  = trackImpl->inputChannel;
            const auto outputChannel = trackImpl->outputChannel;
            if (inputChannel.getType() != stage_type::INPUT_DEFAULT && isChannelConnected(inputChannel)) {
                if (inputChannel.stage.stageRef.stageId == stageId) {
                    removed.inputChannel = inputChannel;
                    trackImpl->inputChannel = ChannelNone();
                    numRemoved++;
                }
            } else {
                dbgassert(inputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
            }
            if (outputChannel.getType() != stage_type::INPUT_DEFAULT && isChannelConnected(outputChannel)) {
                if (inputChannel.stage.stageRef.stageId == stageId) {
                    removed.outputChannel = trackImpl->outputChannel;
                    trackImpl->outputChannel = ChannelNone();
                    numRemoved++;
                }
            } else {
                dbgassert(outputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
            }
            auto& midiInputChannels = trackImpl->midiInputChannels;
            for (auto it = midiInputChannels.begin(); it != midiInputChannels.end();) {
                auto& midiInputChannel = *it;
                if (isMidiChannelConnected(midiInputChannel)) {
                    if (midiInputChannel.getType() == midistage_type::INPUT_AUDIOSTAGE) {
                        if (midiInputChannel.stage.stageRef.stageId == stageId) {
                            removed.midiInputChannels.push_back(midiInputChannel);
                            it = midiInputChannels.erase(it);
                            numRemoved++;
                            continue;
                        }
                    }
                }
                ++it;
            }
            if (numRemoved) {
                removedRoutings.push_back(removed);
            }
        }
        return removedRoutings;
    }

    struct dependency_trackgraph_flattened_t {
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
    bool dep_resolve(dependency_trackgraph_flattened_t& ctxt, track_node_t* node) {
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

    /* Helper function to detect if node is part of a loop */
    bool hasFeedbackLoop(const track_node_ptr node, std::vector<track_node_ptr>& unresolved) {
        unresolved.push_back(node);
        for (auto child : node->children) {
            if (STL_CONTAINS(unresolved, child)) {
                return true;
            }
            if (hasFeedbackLoop(child, unresolved)) {
                return true;
            }
        }
        removeEntry(unresolved, node);
        return false;
    }

    bool buildProcessingGraphFromRoutingGraph(const Host::Host* const host, const std::shared_ptr<track_graph_t>& dependencyGraph, const dependency_trackgraph_flattened_t& graphFlattened, std::shared_ptr<processing_graph_t>& out_procgraph) {
        std::vector<const track_node_t*> tracksVisited;
        std::shared_ptr<processing_graph_t> shrdPtrProcGraph = std::make_shared<processing_graph_t>();

        shrdPtrProcGraph->nodes.reserve(dependencyGraph->nodes.size());
        shrdPtrProcGraph->trackGraph = dependencyGraph;
        for (auto& trackNode : dependencyGraph->nodes) {
            audio_stage_t* audioStage = host->getAudioStage(audio_stage_ref_t{ trackNode->stageId });
            dbgassert(audioStage);
            track_t* const track = audioStage->getTrack();
            dbgassert(track);
            auto const procTrackNode = new processing_track_node_t();
            procTrackNode->type            = trackNode->type;
            procTrackNode->pushs           = trackNode->pushs;
            procTrackNode->pulls           = trackNode->pulls;
            procTrackNode->dependencies    = trackNode->dependencies;
            procTrackNode->stageId         = trackNode->stageId;
            procTrackNode->internalLatency = trackNode->internalLatency;
            procTrackNode->inputLatency    = trackNode->inputLatency;
            procTrackNode->trackOptional   = track;
#ifndef NDEBUG
            procTrackNode->inputLatency = INVALID_SAMPLE_OFFSET_U32;
#endif
            shrdPtrProcGraph->nodes.push_back(procTrackNode);
        }

        processing_graph_t& graph = *(shrdPtrProcGraph);
        tracksVisited.reserve(graph.nodes.size());
        for (const auto ptrTrackNode : graphFlattened.resolved) {
            if (STL_CONTAINS(tracksVisited, ptrTrackNode)) {
                // expected
                continue;
            }
            tracksVisited.push_back(ptrTrackNode);
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
            const track_node_t& trackNode = *(*itStageIdx);
            for (auto depNodeIdx : trackNode.dependencies) {
                auto itDependency = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [depNodeIdx](const track_node_t* ptr) {
                    return ptr->stageId == depNodeIdx;
                });

                if (itDependency >= itStageIdx) {
                    log_lf(Log::L_ERROR, "unexpected: dependecy index >= this index!!\n");
                }
                auto itDependencyChildren = std::find_if(trackNode.children.begin(), trackNode.children.end(), [depNodeIdx](const track_node_t* ptr) {
                    return ptr->stageId == depNodeIdx;
                });
                dbgassert(itDependencyChildren != trackNode.children.end());
            }
        }
#endif

        /**
         * Determine nodes accumulated inputLatency (max_output_latency(all_children))
         * Where child_output_latency = pChild->inputLatency + pChild->internalLatency
         * This has to be done in bottom up/child first order
         */
        samplerate_t maxLatency = 0;
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
            auto stage = host->getAudioStage(audio_stage_ref_t{ ptrNode->stageId });
            dbgassert(stage);
            stage->latencyInput = ptrNode->inputLatency;
            stage->latencyOuput = stage->latencyInput + stage->getInternalLatency();
            maxLatency = std::max(maxLatency, stage->latencyOuput);
#ifndef NDEBUG
            for (auto const trNodeChild : ptrNode->children) {
                dbgassert(STL_CONTAINS(ptrNode->dependencies, trNodeChild->stageId));
                dbgassert(ptrNode->inputLatency >= trNodeChild->inputLatency + trNodeChild->internalLatency);
            }
#endif
        }
        graph.trackGraph->maxLatencySamples = maxLatency;
        /* Assign the resolved latencies to the previously populated push/pull inputs of each node */
        for (auto const ptrNode : graph.nodesFlatOrdered) {
            for (auto& push : ptrNode->pushs) {
                if (isChannelConnected(push.channel) && push.channel.getType() == stage_type::INPUT_AUDIOSTAGE) {
                    const auto pushId = push.channel.stage.stageRef.stageId;
                    const auto ptrChNode = getNodeConst(graph.nodesFlatOrdered, pushId);
                    dbgassert(ptrChNode);
                    push.latency   = ptrChNode->inputLatency + ptrChNode->internalLatency;
                }
            }
            for (auto& pulls : ptrNode->pulls) {
                if (isChannelConnected(pulls.channel) && pulls.channel.getType() == stage_type::INPUT_AUDIOSTAGE) {
                    const auto pullId = pulls.channel.stage.stageRef.stageId;
                    const auto ptrChNode = getNodeConst(graph.nodesFlatOrdered, pullId);
                    dbgassert(ptrChNode);
                    pulls.latency  = ptrChNode->inputLatency + ptrChNode->internalLatency;
                }
            }
        }
        /* Assign correct latencies for external output pulls */
        for (track_source_t& pulls : graph.trackGraph->externalOutputRouting) {
            if (isChannelConnected(pulls.channel) && pulls.channel.getType() == stage_type::INPUT_AUDIOSTAGE) {
                const auto pullId = pulls.channel.stage.stageRef.stageId;
                const auto ptrChNode = getNodeConst(graph.nodesFlatOrdered, pullId);
                if (!ptrChNode) {
                    log_lf(Log::L_ERROR, "External output routing to non existing stage\n");
                    continue;
                }
                dbgassert(ptrChNode);
                pulls.latency  = ptrChNode->inputLatency + ptrChNode->internalLatency;
            }
        }
        out_procgraph = shrdPtrProcGraph;
        return true;
    }

    bool buildProcessingGraphSolo(const Host::Host* const host, const project_t* const project, const track_vector& tracksFlat, const track_vector& tracksSolod, std::shared_ptr<processing_graph_t>& out_procgraph) {
        std::shared_ptr<track_graph_t> dependencyGraph;
        if (!buildTrackRoutingGraph(host, project, tracksFlat, dependencyGraph)) {
            return false;
        }
        track_node_t roots;
        for (auto root : dependencyGraph->roots) {
            if (STL_CONTAINS(tracksSolod, host->getAudioStage(audio_stage_ref_t{ root->stageId })->getTrack()))
                roots.children.push_back(root);
        }
        dependency_trackgraph_flattened_t graphFlattened;
        if (!dep_resolve(graphFlattened, &roots)) {
            return false;
        }
        return buildProcessingGraphFromRoutingGraph(host, dependencyGraph, graphFlattened, out_procgraph);
    }

    bool buildProcessingGraph(const Host::Host* const host, const project_t* const project, const track_vector& tracksFlat, std::shared_ptr<processing_graph_t>& out_procgraph) {
        std::shared_ptr<track_graph_t> dependencyGraph;
        if (!buildTrackRoutingGraph(host, project, tracksFlat, dependencyGraph)) {
            return false;
        }
        track_node_t root;
        root.children.insert(root.children.begin(), dependencyGraph->roots.begin(), dependencyGraph->roots.end());
        dependency_trackgraph_flattened_t graphFlattened;
        if (!dep_resolve(graphFlattened, &root)) {
            return false;
        }
        return buildProcessingGraphFromRoutingGraph(host, dependencyGraph, graphFlattened, out_procgraph);
    }

    track_node_ptr makeTrackNode(audiostageid_i32 a, samplecount_t latency) {
        return new track_node_t(track_node_type_t::TRACK, a, latency);
    }

    processing_track_node_ptr makeProcTrackNode() {
        return new processing_track_node_t();
    }

    template<typename M, typename I>
    inline track_node_t& getNode(M map, I idx) {
        return *map[idx];
    }

    void updateSoloFlag(const Host::Host* const host, const project_t* const project, const track_vector& tracksFlat) {
        std::shared_ptr<track_graph_t> dependencyGraph;
        if (!buildTrackRoutingGraph(host, project, tracksFlat, dependencyGraph)) {
            log_lf(Log::L_ERROR, "Failed building track graph\n");
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

    void unsoloAll(const Host::Host* const host, const project_t* const project, const track_vector& tracksFlat) {
        for (track_t* track : tracksFlat) {
            audiostageflags_t& flags = track->getStage()->flags;
            flags &= ~audiostageflags_t::SOLO_PARENT;
            flags &= ~audiostageflags_t::SOLO;
        }
    }

    bool buildTrackRoutingGraph(const Host::Host* const host, const project_t* const project, const track_vector& tracksFlat, std::shared_ptr<track_graph_t>& out_graph) {
        uint32_t trackEdgeId = 0;
        auto trackGraph = std::make_shared<track_graph_t>();
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

            if (inputChannel.type == stage_type::INPUT_DEFAULT) {
                channel_ref_t tmp;
                if (resolveDefaultConnection(host, project, trackImpl, true, tmp)) {
                    inputChannel = tmp;
                } else {
                    //log_printf("Default input of stage #%d cannot be mapped\n", stageId);
                }
            }
            if (outputChannel.type == stage_type::INPUT_DEFAULT) {
                channel_ref_t tmp;
                if (resolveDefaultConnection(host, project, trackImpl, false, tmp)) {
                    outputChannel = tmp;
                } else {
                    //log_printf("Default output of stage #%d cannot be mapped\n", stageId);
                }
            }
            for (auto& midiInputChannel : trackImpl->midiInputChannels) {
                if (isMidiChannelConnected(midiInputChannel)) {
                    if (midiInputChannel.getType() == midistage_type::INPUT_AUDIOSTAGE) {
                        audio_stage_t* src = host->getAudioStage(midiInputChannel.stage.stageRef);
                        if (!src) {
                            log_lf(Log::L_ERROR, "Stage missing for midi input routing on track %s\n", StringAsCStr(track->name));
                            continue;
                        }
                        dbgassert(src);
                        auto srcStageId = src->stageId.stageId;
                        if (!map.count(srcStageId)) {
                            map[srcStageId] = makeTrackNode(srcStageId, src->getInternalLatency());
                        }
                        track_node_t& trackSrcCfg = getNode(map, srcStageId);
                        trackCfg.dependencies.push_back(srcStageId);
                        trackCfg.children.push_back(&trackSrcCfg);
                        trackSrcCfg.parents.push_back(&trackCfg);
                    }
                }
            }
            if (isChannelConnected(inputChannel)) {
                if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE) {
                    audio_stage_t* src = host->getAudioStage(inputChannel.stage.stageRef);
                    if (!src) {
                        log_lf(Log::L_ERROR, "stage missing for audio input routing on track %s\n", StringAsCStr(track->name));
                        continue;
                    }
                    dbgassert(src);
                    auto srcStageId = src->stageId.stageId;
                    if (!map.count(srcStageId)) {
                        map[srcStageId] = makeTrackNode(srcStageId, src->getInternalLatency());
                    }
                    track_node_t& trackSrcCfg = getNode(map, srcStageId);
                    trackCfg.dependencies.push_back(srcStageId);
                    trackCfg.pulls.push_back(track_source_t{ trackEdgeId++, inputChannel, AutomationNone(), AutomationNone(), 0, src->flags });
                    trackCfg.children.push_back(&trackSrcCfg);
                    trackSrcCfg.parents.push_back(&trackCfg);
                } else if (inputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO) {
                    trackCfg.pulls.push_back(track_source_t{ trackEdgeId++, inputChannel, AutomationNone(), AutomationNone(), 0, audiostageflags_t::NONE });
                } else if (inputChannel.type != stage_type::INPUT_DEFAULT) {
                    log_lf(Log::L_ERROR, "missing audio input routing on track %s\n", StringAsCStr(track->name));
                }
            }
            if (isChannelConnected(outputChannel)) {
                if (outputChannel.getType() == stage_type::INPUT_AUDIOSTAGE && trackImpl->mixer.isEnabled()) {
                    audio_stage_t* dst = host->getAudioStage(outputChannel.stage.stageRef);
                    if (!dst) {
                        log_lf(Log::L_ERROR, "missing audio output routing on track %s\n", StringAsCStr(track->name));
                        continue;
                    }
                    auto dstStageId = dst->stageId.stageId;
                    if (!map.count(dstStageId)) {
                        map[dstStageId] = makeTrackNode(dstStageId, dst->getInternalLatency());
                    }
                    track_node_t& trackDstCfg = getNode(map, dstStageId);
                    trackDstCfg.dependencies.push_back(stageId);
                    trackDstCfg.pushs.push_back(track_source_t{ trackEdgeId++, ChannelStage(trackImpl, stage_bufferpoint::OUTPUT_POST, outputChannel.srcChannelOffset, outputChannel.dstChannelOffset), AutomationNone(), AutomationNone(), 0, trackImpl->flags });
                    trackDstCfg.children.push_back(&trackCfg);
                    trackCfg.parents.push_back(&trackDstCfg);
                } else if (outputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO && trackImpl->mixer.isEnabled()) {
                    trackGraph->externalOutputRouting.push_back(track_source_t{ trackEdgeId++, ChannelStage(trackImpl, stage_bufferpoint::OUTPUT_POST, outputChannel.srcChannelOffset, outputChannel.dstChannelOffset), AutomationNone(), AutomationNone(), 0, trackImpl->flags });
                }
            }
            if (TRACKTYPE_TO_CTR(track->type) == TRACK_CTR_MIDIAUDIO && trackImpl->mixer.isEnabled()) {
                /* Feed audio/midi tracks output into returns input */
                for (track_t* trackReturn : project->trackReturnCtr) {
                    int32_t paramGainIdx = PARAM_OFFSET_SEND_GAIN + trackReturn->localIdxFlat;
                    auto automationRef   = GetRoutingFromDestinationParam(&trackImpl->mixer, paramGainIdx);
                    if (automationRef.type <= automation_routing_type::ROUTING_NONE) {
                        continue;
                    }
                    if (automationRef.type <= automation_routing_type::ROUTING_CONSTANT) {
                        auto sendGainVal = trackImpl->mixer.getParamValue(paramGainIdx);
                        /* Calculate send gain level */
                        if (!dsp_util::getGainLvl(sendGainVal, sendGainVal)) {
                            continue;
                        }
                    }

                    int32_t paramPanIdx   = PARAM_OFFSET_SEND_PAN + trackReturn->localIdxFlat;
                    auto automationRefPan   = GetRoutingFromDestinationParam(&trackImpl->mixer, paramPanIdx);

                    track_impl_t* audioReturn = trackReturn->audio;
                    dbgassert(audioReturn);
                    auto srcStageId = audioReturn->stageId.stageId;

                    if (!map.count(srcStageId)) {
                        map[srcStageId] = makeTrackNode(srcStageId, audioReturn->getInternalLatency());
                    }
                    track_node_t& trackReturnCfg = getNode(map, srcStageId);
                    trackReturnCfg.dependencies.push_back(trackImpl->stageId.stageId);
                    trackReturnCfg.pushs.push_back(track_source_t{ trackEdgeId++, ChannelStage(trackImpl, stage_bufferpoint::OUTPUT_POST), automationRef, automationRefPan, 0, trackImpl->flags });
                    trackReturnCfg.children.push_back(&trackCfg);
                    trackCfg.parents.push_back(&trackReturnCfg);
                }
            }
        }
        //std::shared_ptr<track_graph_t> trackGraph(new track_graph_t(), [](track_graph_t *gr) {
        //  log_lf(Log::L_DEBUG, "free track_graph %08X\n", reinterpret_cast<uint64_t>(gr));
        //});
        trackGraph->nodes.reserve(map.size());
        std::vector<track_node_ptr> unresolved;
        for (auto mapIt = map.begin(); mapIt != map.end(); ++mapIt) {
            track_node_ptr node = mapIt->second;
            if (hasFeedbackLoop(node, unresolved)) {
                return false;
            }
            if (node->parents.empty()) {
                trackGraph->roots.push_back(node);
            }
            stl_remove_duplicates(node->children);
            stl_remove_duplicates(node->parents);
            stl_remove_duplicates(node->dependencies);
            trackGraph->nodes.push_back(mapIt->second);
        }
        if (gEnableLog) {
            for (track_node_ptr& ptr : trackGraph->nodes) {
                for (audiostageid_i32 src : ptr->dependencies) {
                    log_lf(Log::L_DEBUG, "%d => %d\n", static_cast<int32_t>(src), static_cast<int32_t>(ptr->stageId));
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

    int32_t GetUnqiueProcessingNodeId(const DAW::processing_track_node_t& node) {
        switch(node.type) {
            case track_node_type_t::TRACK:
            case track_node_type_t::AUDIOSTAGE:
                return static_cast<int32_t>(node.stageId);
            case track_node_type_t::EFFECT:
                return static_cast<int32_t>(node.stageId) + (1<<30);
        }
        return -1;
    }
}
