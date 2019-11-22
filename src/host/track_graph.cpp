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
#include "daw_channel.h"
#include <vector>
#include <deque>


namespace DAW {
	bool gEnableLog = 0;
	bool validateTrackRoutings(const vsthost* const host, const track_vector& tracksFlat) {
		size_t numRemoved = 0;
		for (track_t* track : tracksFlat) {
			track_impl_t* trackImpl = track->getStage();
			const auto inputChannel = trackImpl->inputChannel;
			const auto outputChannel = trackImpl->outputChannel;
			if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
				if (!host->getAudioStage(inputChannel.stage.stageRef)) {
					trackImpl->inputChannel = ChannelNone();
					numRemoved++;
				}
			} else {
				dbgassert(inputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
//				inputChannel.stage.stageRef.stageId = TRACKID_INVALID_I32; //FIX: old project files have stageId == 0
			}
			if (outputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
				if (!host->getAudioStage(outputChannel.stage.stageRef)) {
					trackImpl->outputChannel = ChannelNone();
					numRemoved++;
				}
			} else {
				dbgassert(outputChannel.stage.stageRef.stageId == TRACKID_INVALID_I32);
//				outputChannel.stage.stageRef.stageId = TRACKID_INVALID_I32; //FIX: old project files have stageId == 0
			}
		}
		return numRemoved == 0;
	}
	bool removeTrackRoutings(const track_vector& tracksFlat, const audiostageid_i32 stageId) {
		size_t numRemoved = 0;
		for (track_t* track : tracksFlat) {

			track_impl_t* trackImpl = track->getStage();
			const auto inputChannel = trackImpl->inputChannel;
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

	bool buildProcessingGraph(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat, track_graph_t& out_dependencyGraph, processing_list_t& out_processingList) {
		DAW::track_graph_t dependencyGraph ;
		if (!DAW::buildTrackRoutingGraph(host, project, tracksFlat, dependencyGraph )) {
			log_printf("Failed building track graph\n", 0);
			return false;
		}

		track_vector tracksVisited;
		std::vector<processing_track_node_t> dependencyList;
		std::deque<audiostageid_i32> stack;
		stack.insert(stack.begin(), dependencyGraph.roots.cbegin(), dependencyGraph .roots.cend());
		while (!stack.empty()) {
			audiostageid_i32 nodeIdx = stack.front();
			stack.pop_front();
			audio_stage_t* audioStage = host->getAudioStage(audio_stage_ref_t{nodeIdx});
			dbgassert(audioStage);
			track_t* const track = audioStage->getTrack();
			dbgassert(track);
			if (STL_CONTAINS(tracksVisited, track)) {
				log_printf("loop in track graph\n", 0);
				continue;
			}
			tracksVisited.push_back(track);
			auto itGraphNode = std::find_if(dependencyGraph.nodes.begin(), dependencyGraph.nodes.end(), [nodeIdx] (const DAW::track_node_t& ptr) {
				return ptr.stageId == nodeIdx;
			});
			dbgassert(itGraphNode != dependencyGraph.nodes.end());
			const DAW::track_node_t trackNode = *itGraphNode;
			if (trackNode.dependencies.size()) {
				stack.insert(stack.begin(), trackNode.dependencies.cbegin(), trackNode.dependencies.cend());
			}
			dependencyList.push_back(processing_track_node_t{nodeIdx, trackNode, track});
		}
		for (processing_track_node_t processingNode : dependencyList) {
			const audiostageid_i32 nodeIdx = processingNode.nodeIdx;
			const DAW::track_node_t& trackNode = processingNode.trackNode;
			auto itStageIdx = std::find_if(dependencyList.begin(), dependencyList.end(), [nodeIdx] (const processing_track_node_t& n) {
				return n.nodeIdx == nodeIdx;
			});
			for (auto depNodeIdx : trackNode.dependencies) {

				auto itDependency = std::find_if(dependencyList.begin(), dependencyList.end(), [depNodeIdx] (const processing_track_node_t& n) {
					return n.nodeIdx == depNodeIdx;
				});
				//we process in reverse, so dependency must lay after parent
				if (itDependency <= itStageIdx) {
					log_printf("unexpected: dependecy index <= this index!!\n", 0);
				}

				//TODO: look thru whole dependency chain, compare all node iterators against itStageIdx
			}
		}
		out_dependencyGraph = std::move(dependencyGraph);
		out_processingList = processing_list_t{std::move(dependencyList)};
		return true;
	}

	bool buildTrackRoutingGraph(const vsthost* const host, const project_t* const project, const track_vector& tracksFlat, track_graph_t& out) {

		std::map<audiostageid_i32, track_node_t> map;
		for (track_t* track : tracksFlat) {
			track_impl_t* trackImpl = track->getStage();
			if (!map.count(trackImpl->stageId)) {
				map[trackImpl->stageId] = track_node_t{trackImpl->stageId, {}};
			}
			track_node_t& trackCfg = map[trackImpl->stageId];

			auto inputChannel = trackImpl->inputChannel;
			auto outputChannel = trackImpl->outputChannel;

			if (inputChannel.type == channel_input_type::INPUT_DEFAULT) {
				channel_ref_t tmp;
				if (DAW::resolveDefaultConnection(host, project, trackImpl, true, tmp)) {
					inputChannel = tmp;
				}
			}
			if (outputChannel.type == channel_input_type::INPUT_DEFAULT) {
				channel_ref_t tmp;
				if (DAW::resolveDefaultConnection(host, project, trackImpl, false, tmp)) {
					outputChannel = tmp;
				}
			}
			if (isChannelConnected(inputChannel)) {
				if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
					audio_stage_t* src = host->getAudioStage(inputChannel.stage.stageRef);
					dbgassert(src);
					if (!map.count(src->stageId)) {
						map[src->stageId] = track_node_t{src->stageId, {}};
					}
					track_node_t& trackSrcCfg = map[trackImpl->stageId];
					trackSrcCfg.numDependants++;
					trackCfg.dependencies.push_back(src->stageId);
					trackCfg.pulls.push_back(DAW::track_source_t{inputChannel, 1.0f});
				}
			}
			if (isChannelConnected(outputChannel)) {
				if (outputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE && trackImpl->mixer.isEnabled()) {
					audio_stage_t* dst = host->getAudioStage(outputChannel.stage.stageRef);
					dbgassert(dst);
					if (!map.count(dst->stageId)) {
						map[dst->stageId] = track_node_t{dst->stageId, {}};
					}
					track_node_t& trackDstCfg = map[dst->stageId];
					trackCfg.numDependants++;
					trackDstCfg.dependencies.push_back(trackImpl->stageId);
					trackDstCfg.pushs.push_back(DAW::track_source_t{ChannelStage(trackImpl, false), 1.0f});
				}
			}
			if (TRACKTYPE_TO_CTR(track->type)  == TRACK_CTR_MIDIAUDIO && trackImpl->mixer.isEnabled()) {
				/* Feed audio/midi tracks output into returns input */
				for (track_t* trackReturn : project->trackReturnCtr) {
					/* Calculate send gain level */
					float fGainReturn;
					if (!getGainLvl(trackImpl->mixer.getParamValue(PARAM_OFFSET_SEND+trackReturn->localIdxFlat), fGainReturn)) {
						continue;
					}

					track_impl_t* audioReturn = trackReturn->audio;
					dbgassert(audioReturn);

					if (!map.count(audioReturn->stageId)) {
						map[audioReturn->stageId] = track_node_t{audioReturn->stageId, {}};
					}
					track_node_t& trackDstCfg = map[audioReturn->stageId];
					trackCfg.numDependants++;
					trackDstCfg.dependencies.push_back(audioReturn->stageId);
					trackDstCfg.pushs.push_back(DAW::track_source_t{ChannelStage(audioReturn, false), fGainReturn});

				}

			}

		}
		track_graph_t tmp;
		for (std::map<audiostageid_i32, track_node_t>::iterator mapIt = map.begin(); mapIt != map.end(); ++mapIt) {
			if (mapIt->second.numDependants == 0) {
				tmp.roots.push_back(mapIt->second.stageId);
			}
			tmp.nodes.push_back(mapIt->second);
		}
		if (gEnableLog) {
			for (std::map<audiostageid_i32, track_node_t>::iterator mapIt = map.begin(); mapIt != map.end(); ++mapIt) {
				for (audiostageid_i32 src : mapIt->second.dependencies) {
					log_printf("%d => %d\n", src, mapIt->second.stageId);
				}
			}
		}
		out = std::move(tmp);

		return true;
	}
}
