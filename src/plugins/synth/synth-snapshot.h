#pragma once
#include "str_util.h"
#include "types.h"
#include <vector>
#include <memory>

namespace PluginSynth {

struct param_float_snapshot_t {
    int32_t paramIdx;
    double value;
};
struct param_int_snapshot_t {
    int32_t paramIdx;
    int32_t value;
};
struct modulation_src_snapshot_t {
    int32_t typeIdx;
    int32_t srcIdx;
    int32_t opIdx;
    double value;
    String function;
    bool isBiPolar;
};
struct modulation_dest_snapshot {
    int32_t paramIdx;
    double range;
};
struct modulation_snapshot_t {
    int32_t slotIdx = -1;
    std::vector<modulation_src_snapshot_t> inputs;
    std::vector<modulation_dest_snapshot> destinations;
};
struct ui_layout_t {
    float splitPos = 0.8;
};
struct snapshot_t {
    int32_t version = 0;
    std::vector<param_float_snapshot_t> params;
    std::vector<modulation_snapshot_t> modulations;
    std::vector<ui_layout_t> uiLayout;
};

std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot);
bool deserializeSnapshot(const std::shared_ptr<std::vector<std::byte>>& data, snapshot_t& snapshotOut);

} // namespace PluginSynth