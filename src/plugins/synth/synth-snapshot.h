#pragma once
#include "file/shapefile.h"
#include "str_util.h"
#include "types.h"
#include <vector>
#include <memory>

namespace PluginSynth {
#define SYNTH_SNAPSHOT_VERSION 10
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
    uint8_t range;
};
struct modulation_dest_snapshot {
    int32_t paramIdx;
    double range;
};
struct setting_snapshot_t {
    int32_t paramIdx;
    float range;
};
struct shape_snapshot_t {
    int32_t type = -1;
    DAW::Shape::shape_preset_t shape;
};
struct modulation_snapshot_t {
    int32_t slotIdx = -1;
    std::vector<modulation_src_snapshot_t> inputs;
    std::vector<modulation_dest_snapshot> destinations;
};
struct ui_layout_t {
    int32_t uiId = 0;
    float splitPos = 0.8;
};
struct snapshot_t {
    int32_t version = 0;
    std::vector<param_float_snapshot_t> params;
    std::vector<modulation_snapshot_t> modulations;
    std::vector<setting_snapshot_t> settings;
    std::vector<shape_snapshot_t> shapes;
    std::vector<ui_layout_t> uiLayout;
};

std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot);
bool deserializeSnapshot(const std::shared_ptr<std::vector<std::byte>>& data, snapshot_t& snapshotOut);

} // namespace PluginSynth