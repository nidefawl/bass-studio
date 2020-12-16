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
#include <deque>


namespace DAW {
	size_t validateInput(const vsthost* const host, audio_stage_t* stage, DAW::channel_ref_t& inputChannel) {
		size_t numRemoved = 0;
		if (inputChannel.getType() == channel_input_type::INPUT_EXTERNAL_AUDIO) {
			int32_t idx = inputChannel.externalInputIdx;
			String name = "External "+AudioIO::getTrackNameShort(inputChannel.externalInputType, idx, stagebuffer_point::INPUT);
			inputChannel = ChannelAudioInput(idx, inputChannel.inputChannelOffset, name, inputChannel.externalInputType);
		} else if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
			auto* srcstage = host->getAudioStage(inputChannel.stage.stageRef);
			if (!srcstage) {
				log_printf("Input audiostage with id %d not found\n", inputChannel.stage.stageRef);
				inputChannel = ChannelNone();
				numRemoved++;
			} else {
				inputChannel = DAW::ChannelStage(srcstage, stagebuffer_point::OUTPUT_POST);
			}
		}  else if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE_EFFECT) {
			auto* eff = host->getPluginById(inputChannel.projectGlobalId);
			if (!eff) {
				log_printf("Input effect with id %d not found\n", inputChannel.projectGlobalId);
				inputChannel = ChannelNone();
				numRemoved++;
			} else {
				inputChannel = DAW::ChannelAudioEffect(eff, stagebuffer_point::OUTPUT_POST);
			}
		} else {
			dbgassert(inputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
//				inputChannel.stage.stageRef.stageId = TRACKID_INVALID_I32; //FIX: old project files have stageId == 0
		}
		return numRemoved;
	}
	bool validateEffectRoutings(const vsthost* const host, audio_stage_t* stage) {
		size_t numRemoved = 0;
		for (effectbase* effect : stage->effects) {
			for (auto& inputChannel : effect->inputChannels) {
				numRemoved += validateInput(host, stage, inputChannel);
			}
		}
		for (auto& inputChannel : stage->postEffectRouting) {
			numRemoved += validateInput(host, stage, inputChannel);
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
		std::shared_ptr<DAW::effect_graph_t> dependencyGraph;
		if (!DAW::buildEffectRoutingGraph(host, project, stage, dependencyGraph )) {
			log_printf("Failed building track graph\n", 0);
			return false;
		}
		effect_node_t root;
		root.children.insert(root.children.begin(), dependencyGraph->roots.begin(), dependencyGraph->roots.end());
		dependency_graph_flattened_t graphFlattened;
		if (!dep_resolve_graph(graphFlattened, &root)) {
			log_printf("Failed flattening track graph\n", 0);
			return false;
		}

		std::vector<const effect_node_t*> effsVisited;
		std::shared_ptr<effect_processing_graph_t> shrdPtrProcGraph = std::make_shared<effect_processing_graph_t>();
		shrdPtrProcGraph->nodes.reserve(dependencyGraph->nodes.size());
		shrdPtrProcGraph->trackGraph = dependencyGraph;
		for (effect_node_ptr trackNode : dependencyGraph->nodes) {
//			audio_stage_t* audioStage = host->getAudioStage(audio_stage_ref_t{trackNode->stageId});
//			dbgassert(audioStage);
//			track_t* const track = audioStage->getTrack();
//			dbgassert(track);
			processing_effect_node_t* procTrackNode = new processing_effect_node_t();//std::make_unique<processing_track_node_t>();

			procTrackNode->type = trackNode->type;
            procTrackNode->pulls = trackNode->pulls;
			procTrackNode->dependencies = trackNode->dependencies;
			procTrackNode->stageId = trackNode->stageId;
			procTrackNode->internalLatency = trackNode->internalLatency;
			procTrackNode->inputLatency = trackNode->inputLatency;
			switch (trackNode->type) {
				case track_node_type_t::TRACK:
					procTrackNode->trackOptional = nullptr;
					dbgassert(0);
					break;
				case track_node_type_t::AUDIOSTAGE:
                    if (procTrackNode->stageId == TRACKID_DEFAULT_I32) {
                        procTrackNode->stage = host->getAudioStage(audio_stage_ref_t{stage->stageId}); // resolve const to non const
                    } else {
                        procTrackNode->stage = host->getAudioStage(audio_stage_ref_t{procTrackNode->stageId});
                    }
					dbgassert(procTrackNode->stage);
                    dbgassert(procTrackNode->stageId == TRACKID_DEFAULT_I32 || procTrackNode->stageId == stage->stageId); // for now
					break;
				case track_node_type_t::EFFECT:
					procTrackNode->effectOptional = host->getPluginById(static_cast<int32_t>(trackNode->stageId));
					dbgassert(procTrackNode->effectOptional);
					break;
			}
			shrdPtrProcGraph->nodes.push_back(procTrackNode);
		}
		effect_processing_graph_t& graph = *(shrdPtrProcGraph.get());

		for (const effect_node_t* const ptrTrackNode : graphFlattened.resolved) {
			const DAW::effect_node_t& trackNode = *ptrTrackNode;
			auto nodeIdx = trackNode.stageId;
			//effectbase* eff = stage->getPluginById(static_cast<int32_t>(nodeIdx));
			//dbgassert(eff);
            if (STL_CONTAINS(effsVisited, ptrTrackNode)) {
				// expected
				continue;
			}
            effsVisited.push_back(ptrTrackNode);
			auto itProcGraphNode = std::find_if(graph.nodes.begin(), graph.nodes.end(), [nodeIdx] (const processing_effect_node_ptr& ptr) {
				return ptr->stageId == nodeIdx;
			});
			dbgassert(itProcGraphNode != graph.nodes.end());
			processing_effect_node_t* procTrackNode = *itProcGraphNode;

			for (effect_node_t* tnChild : trackNode.children) {
				auto itProcGraphNodeChild = std::find_if(graph.nodes.begin(), graph.nodes.end(), [nodeIdx=tnChild->stageId] (const processing_effect_node_ptr& ptr) {
					return ptr->stageId == nodeIdx;
				});
				dbgassert(itProcGraphNodeChild != graph.nodes.end());
				processing_effect_node_t* procTrackNodeChild = *itProcGraphNodeChild;
				dbgassert(procTrackNodeChild->stageId != trackNode.stageId);
				procTrackNode->children.push_back(procTrackNodeChild);
			}


			for (effect_node_t* tnParent : trackNode.parents) {
				auto itProcGraphNodeParent = std::find_if(graph.nodes.begin(), graph.nodes.end(), [nodeIdx=tnParent->stageId] (const processing_effect_node_ptr& ptr) {
					return ptr->stageId == nodeIdx;
				});
				dbgassert(itProcGraphNodeParent != graph.nodes.end());
				processing_effect_node_t* procTrackNodeParent = *itProcGraphNodeParent;
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
			const DAW::effect_node_t& trackNode = *(*itStageIdx);
			for (auto depNodeIdx : trackNode.dependencies) {
				auto itDependency = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [depNodeIdx] (const effect_node_t* ptr) {
					return ptr->stageId == depNodeIdx;
				});

				if (itDependency >= itStageIdx) {
					log_printf("unexpected: dependecy index >= this index!!\n", 0);
				}
				auto itDependencyChildren = std::find_if(trackNode.children.begin(), trackNode.children.end(), [depNodeIdx] (const effect_node_t* ptr) {
					return ptr->stageId == depNodeIdx;
				});
				dbgassert(itDependencyChildren != trackNode.children.end());
			}
		}
		for (auto itNodes = graph.nodes.begin(); itNodes != graph.nodes.end(); itNodes++) {
			auto ptrNode = *itNodes;
			ptrNode->inputLatency = INVALID_SAMPLE_OFFSET_U32;
		}
#endif

		/* Determine nodes accumulated inputLatency (own internalLatency + max_latency(all_children)) */
		/* This has to be done in bottom up/child first */
		for (auto itNodes = graph.nodesFlatOrdered.begin(); itNodes != graph.nodesFlatOrdered.end(); itNodes++) {
			auto ptrNode = *itNodes;
			ptrNode->inputLatency = 0;
			dbgassert(ptrNode->children.size() == ptrNode->dependencies.size());
			for (DAW::effect_node_t* trNodeChild : ptrNode->children) {
				dbgassert(ptrNode->stageId != trNodeChild->stageId);
				auto itProcNode = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [nodeIdx=trNodeChild->stageId] (processing_effect_node_t* ptr) {
					return ptr->stageId == nodeIdx;
				});
				dbgassert(itProcNode != graph.nodesFlatOrdered.end());
				auto ptrChNode = *itProcNode;
				dbgassert(ptrChNode == trNodeChild);
			}
			for (auto nodeIdx : ptrNode->dependencies) {
				auto itProcNode = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [nodeIdx] (processing_effect_node_t* ptr) {
					return ptr->stageId == nodeIdx;
				});
				dbgassert(itProcNode != graph.nodesFlatOrdered.end());
				auto ptrChNode = *itProcNode;
#ifndef NDEBUG
				dbgassert(ptrChNode->inputLatency != INVALID_SAMPLE_OFFSET_U32);
#endif
				ptrNode->inputLatency = std::max(ptrNode->inputLatency, ptrChNode->inputLatency + ptrChNode->internalLatency);

			}
			for (auto nodeIdx : ptrNode->dependencies) {
				auto itProcNode = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [nodeIdx] (processing_effect_node_t* ptr) {
					return ptr->stageId == nodeIdx;
				});
				dbgassert(itProcNode != graph.nodesFlatOrdered.end());
				auto ptrChNode = *itProcNode;
				dbgassert(ptrNode->inputLatency >= ptrChNode->inputLatency + ptrChNode->internalLatency);
			}
			for (DAW::effect_node_t* trNodeChild : ptrNode->children) {
				dbgassert(STL_CONTAINS(ptrNode->dependencies, trNodeChild->stageId));

				dbgassert(ptrNode->inputLatency >= trNodeChild->inputLatency + trNodeChild->internalLatency);
			}
		}
		/* Assign the resolved latencies to the previously populated push/pull inputs of each node */
		for (auto itNodes = graph.nodesFlatOrdered.begin(); itNodes != graph.nodesFlatOrdered.end(); itNodes++) {
			auto ptrNode = *itNodes;
			for (auto& pulls : ptrNode->pulls) {
				if (DAW::isChannelConnected(pulls.channel) && pulls.channel.getType() == DAW::channel_input_type::INPUT_AUDIOSTAGE_EFFECT) {
					const auto effId = static_cast<audiostageid_i32>(pulls.channel.projectGlobalId);
					auto itStageIdx = std::find_if(graph.nodesFlatOrdered.begin(), graph.nodesFlatOrdered.end(), [effId] (processing_effect_node_t* ptr) {
						return ptr->stageId == effId;
					});
					dbgassert(itStageIdx != graph.nodesFlatOrdered.end());
					auto ptrChNode = *itStageIdx;
					pulls.latency = ptrChNode->inputLatency + ptrChNode->internalLatency;
				}
			}
		}
//		out_dependencyGraph = std::move(dependencyGraph);
		out_procgraph = shrdPtrProcGraph;
		return true;
	}
	effect_node_ptr makeEffectNode(audiostageid_i32 stageId, int32_t effectId, samplerate_t b) {
		return new effect_node_t(track_node_type_t::EFFECT, static_cast<audiostageid_i32>(effectId), b);// std::make_unique<effect_node_t>
	}
	effect_node_ptr makeAudioStageNode(audiostageid_i32 stageId, samplerate_t b) {
		return new effect_node_t(track_node_type_t::AUDIOSTAGE, stageId, b);// std::make_unique<effect_node_t>
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
		int32_t effIdx = 0;
		int32_t lastEffIdx = stage->effects.size() - 1u;
		effectbase* effectPrev = nullptr;
		for (effectbase* effect : stage->effects) {
			const int32_t effectId = effect->projectGlobalId;
			const auto effectIdI32 = static_cast<audiostageid_i32>(effect->projectGlobalId);
			if (!map.count(effectIdI32)) {
				map[effectIdI32] = makeEffectNode(stage->stageId, effectId, effect->getDelay());
			}
			effect_node_t& trackCfg = getEffNode(map, effectIdI32);
			for (DAW::channel_ref_t inputChannel : effect->inputChannels) {
				if (inputChannel.type == channel_input_type::INPUT_DEFAULT) {
					channel_ref_t tmp;
					if (DAW::resolveEffectDefaultConnection(host, project, stage, effect, tmp)) {
						inputChannel = tmp;
					}
				}
				if (isChannelConnected(inputChannel)) {
					if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
						audio_stage_t* src = host->getAudioStage(inputChannel.stage.stageRef);
						dbgassert(src);
						// the correct dependency here would be the inputs routed to this track (the node of this track in the track graph)

						if (!audioStageInputs.count(src->stageId)) {
							audioStageInputs[src->stageId] = makeAudioStageNode(src->stageId, 0);
						}
						effect_node_t& trackSrcCfg = getEffNode(audioStageInputs, src->stageId);
						trackCfg.dependencies.push_back(src->stageId);
						trackCfg.pulls.push_back(DAW::effect_source_t{trackEdgeId++, inputChannel, 1.0f, 0, src->flags});
						trackCfg.children.push_back(&trackSrcCfg);
						trackSrcCfg.parents.push_back(&trackCfg);

						//trackCfg.children.push_back(&trackSrcCfg);
						//trackSrcCfg.parents.push_back(&trackCfg);

					} else if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE_EFFECT) {
						effectbase* effSrc = host->getPluginById(inputChannel.projectGlobalId);
						dbgassert(effSrc);
						auto effSrcId_I32 = static_cast<audiostageid_i32>(effSrc->projectGlobalId);
						if (!map.count(effSrcId_I32)) {
							map[effSrcId_I32] = makeEffectNode(effSrc->getTrackLink()->stageId, effSrc->projectGlobalId, effSrc->getDelay());
						}
						effect_node_t& trackSrcCfg = getEffNode(map, effSrcId_I32);
						trackCfg.dependencies.push_back(effSrcId_I32);
						trackCfg.pulls.push_back(DAW::effect_source_t{trackEdgeId++, inputChannel, 1.0f, 0, audiostageflags_t::NONE});
						trackCfg.children.push_back(&trackSrcCfg);
						trackSrcCfg.parents.push_back(&trackCfg);
					} else if (inputChannel.getType() == channel_input_type::INPUT_EXTERNAL_AUDIO) {
						trackCfg.pulls.push_back(DAW::effect_source_t{trackEdgeId++, inputChannel, 1.0f, 0, audiostageflags_t::NONE});
					} else {
						log_printf("missing track input routing\n", 0);
					}
				}
			}
//			if (inputChannel.type == channel_input_type::INPUT_DEFAULT) {
//				channel_ref_t tmp;
//				if (DAW::resolveDefaultConnection(host, project, trackImpl, true, tmp)) {
//					inputChannel = tmp;
//				}
//			}

			effectPrev = effect;
			effIdx++;
		}
		audioStageOutputs[TRACKID_DEFAULT_I32] = makeAudioStageNode(TRACKID_DEFAULT_I32, 0);
		effect_node_t& nodeOutput = getEffNode(audioStageOutputs, TRACKID_DEFAULT_I32);

//		effect_node_t& trackCfg = getEffNode(audioStageOutputs, audioStageOutputs[TRACKID_DEFAULT_I32]);
		for (DAW::channel_ref_t inputChannel : stage->postEffectRouting) {
			if (inputChannel.type == channel_input_type::INPUT_DEFAULT) {
				channel_ref_t tmp;
				if (DAW::resolveEffectDefaultConnection(host, project, stage, nullptr, tmp)) {
					inputChannel = tmp;
				}
			}
			if (isChannelConnected(inputChannel)) {
				if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
					audio_stage_t* src = host->getAudioStage(inputChannel.stage.stageRef);
					dbgassert(src);
					// the correct dependency here would be the inputs routed to this track (the node of this track in the track graph)

					if (!audioStageInputs.count(src->stageId)) {
						audioStageInputs[src->stageId] = makeAudioStageNode(src->stageId, 0);
					}
					effect_node_t& trackSrcCfg = getEffNode(audioStageInputs, src->stageId);
					nodeOutput.dependencies.push_back(src->stageId);
					nodeOutput.pulls.push_back(DAW::effect_source_t{trackEdgeId++, inputChannel, 1.0f, 0, src->flags});
					nodeOutput.children.push_back(&trackSrcCfg);
					trackSrcCfg.parents.push_back(&nodeOutput);

					//trackCfg.children.push_back(&trackSrcCfg);
					//trackSrcCfg.parents.push_back(&trackCfg);

				} else if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE_EFFECT) {
					effectbase* effSrc = host->getPluginById(inputChannel.projectGlobalId);
					dbgassert(effSrc);
					auto effSrcId_I32 = static_cast<audiostageid_i32>(effSrc->projectGlobalId);
					if (!map.count(effSrcId_I32)) {
						map[effSrcId_I32] = makeEffectNode(effSrc->getTrackLink()->stageId, effSrc->projectGlobalId, effSrc->getDelay());
					}
					effect_node_t& trackSrcCfg = getEffNode(map, effSrcId_I32);
					nodeOutput.dependencies.push_back(effSrcId_I32);
					nodeOutput.pulls.push_back(DAW::effect_source_t{trackEdgeId++, inputChannel, 1.0f, 0, audiostageflags_t::NONE});
					nodeOutput.children.push_back(&trackSrcCfg);
					trackSrcCfg.parents.push_back(&nodeOutput);
				} else if (inputChannel.getType() == channel_input_type::INPUT_EXTERNAL_AUDIO) {
					nodeOutput.pulls.push_back(DAW::effect_source_t{trackEdgeId++, inputChannel, 1.0f, 0, audiostageflags_t::NONE});
				} else {
					log_printf("missing track input routing\n", 0);
				}
			}
		}

		auto trackGraph = std::make_shared<effect_graph_t>();
		trackGraph->nodes.reserve(map.size());
		for (std::map<audiostageid_i32, effect_node_ptr>::iterator mapIt = map.begin(); mapIt != map.end(); ++mapIt) {
			effect_node_ptr node = mapIt->second;
			if (node->parents.empty()) {
				trackGraph->roots.push_back(node);
			}
			trackGraph->nodes.push_back(std::move(mapIt->second));
		}
		for (auto mapIt = audioStageOutputs.begin(); mapIt != audioStageOutputs.end(); ++mapIt) {
			effect_node_ptr node = mapIt->second;
			if (node->parents.empty()) {
				trackGraph->roots.push_back(node);
			}
			trackGraph->nodes.push_back(std::move(mapIt->second));
		}
		for (auto mapIt = audioStageInputs.begin(); mapIt != audioStageInputs.end(); ++mapIt) {
			effect_node_ptr node = mapIt->second;
			if (node->parents.empty()) {
				trackGraph->roots.push_back(node);
			}
			trackGraph->nodes.push_back(std::move(mapIt->second));
		}
//		if (gEnableLog) {
//			for (effect_node_ptr& ptr : trackGraph->nodes) {
//				for (int32_t src : ptr->dependencies) {
//					log_printf("%d => %d\n", src, ptr->projectGlobalId);
//				}
//			}
//		}
		out_graph = trackGraph;

		return true;
	}
}
