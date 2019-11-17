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
#include <vector>
#include <deque>

//bool resolveAudioChannel(vsthost* host, int32_t numChannelsTrack, channel_ref_t& inputChannel, const AudioBlock* const ptrExternalInputs, track_audio_src& out);

namespace DAW {

	/**
	 * track_node_t - represents a node in the audio chain dependency graph
	 *
	 */
	struct track_node_t {
		audiostageid_i32 stageId;
		std::vector<audiostageid_i32> dependencies;
		int32_t numDependants;
		uint64_t latencyBefore;
		uint64_t latencyToMaster;
	};
	/**
	 * track_graph_t - represents the audio chain dependency graph build from I/O configuration of all loaded tracks
	 *
	 */
	struct track_graph_t {
		std::vector<audiostageid_i32> roots; // outbut nodes (Master, )
		std::vector<track_node_t> nodes;
		uint64_t maxLatency;
	};
	bool buildTrackGraph(vsthost* host, const track_vector& tracksFlat, track_graph_t& out) {

		std::map<audiostageid_i32, track_node_t> map;
		for (track_t* track : tracksFlat) {
			track_impl_t* trackImpl = track->getStage();
			if (!map.count(trackImpl->stageId)) {
				map[trackImpl->stageId] = track_node_t{trackImpl->stageId, {}};
			}
			track_node_t& trackCfg = map[trackImpl->stageId];
			if (isChannelConnected(trackImpl->inputChannel)) {
				if (trackImpl->inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
					audio_stage_t* src = host->getAudioStage(trackImpl->inputChannel.stage.stageRef);
					dbgassert(src);
					if (!map.count(src->stageId)) {
						map[src->stageId] = track_node_t{src->stageId, {}};
					}
					track_node_t& trackSrcCfg = map[trackImpl->stageId];
					trackSrcCfg.numDependants++;
					trackCfg.dependencies.push_back(src->stageId);
				}
			}
			if (isChannelConnected(trackImpl->outputChannel)) {
				if (trackImpl->outputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
					audio_stage_t* dst = host->getAudioStage(trackImpl->outputChannel.stage.stageRef);
					dbgassert(dst);
					if (!map.count(dst->stageId)) {
						map[dst->stageId] = track_node_t{dst->stageId, {}};
					}
					track_node_t& trackDstCfg = map[trackImpl->stageId];
					trackCfg.numDependants++;
					trackDstCfg.dependencies.push_back(dst->stageId);
				}
			}
		}
		track_graph_t tmp;
		for (auto mapIt = map.begin(); mapIt != map.end(); ++mapIt) {
			if (mapIt->second.numDependants == 0) {
				tmp.roots.push_back(mapIt->second.stageId);
			}
		}

		return true;
	}
}
