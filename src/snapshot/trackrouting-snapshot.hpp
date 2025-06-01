#pragma once
#include <vector>
#include <map>
#include "str_util.hpp"
#include "types.hpp"

struct track_id_snapshot_t {
    int32_t stageId           = -1;
    int32_t inputStageId      = -1;
    int32_t outputStageId     = -1;
    int32_t outputPostStageId = -1;
};

struct io_midi_snapshot_t {
    int32_t type                = 1;
    int32_t stageId             = -1;
    int32_t stageEndPointType   = 2;
    int32_t srcChannel          = -1;
    int32_t dstChannel          = -1;
    String inputName            = "None";
};
struct io_configuration_snapshot_t {
    int32_t type                = 0;
    int32_t stageId             = -1;
    int32_t stageEndPointType   = 0;
    int32_t externalInputType   = 0;
    int32_t projectGlobalId     = 0;
    int32_t externalInputIdx    = 0;
    int32_t srcChannelOffset    = 0;
    int32_t dstChannelOffset    = 0;
};
struct track_io_configuration_snapshot_t {
    io_configuration_snapshot_t input;
    io_configuration_snapshot_t output;
    std::vector<io_midi_snapshot_t> midiInputs;
   io_midi_snapshot_t midiOutput; 
};
struct track_effect_routing_snapshot_t {
    int32_t routingState = 0;
    std::vector<io_configuration_snapshot_t> inputRoutingOutputStage;
    std::map<int32_t, std::vector<io_configuration_snapshot_t>> inputRoutingEffects;
};