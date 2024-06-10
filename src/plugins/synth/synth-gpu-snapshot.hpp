#pragma once
#include "file/shapefile.h"
#include "str_util.h"
#include "synth-plugin.h"
#include "synth-snapshot.h"
#include "byte-buffer.h"
#include <array>
#include <cstdint>
#include <utility>

namespace PluginSynth::GPU {
struct ui_layout_t {
    int32_t uiId = 0;
};
struct lfo_snapshot_t {
    DAW::Shape::shape_snapshot_t shape{};
    bool modeIsShape = true;
    int32_t randomModeId = 0;
    int32_t syncFlags = 0;
};
struct adsr_snapshot_t {
    int32_t shapingMode = 0;
};
struct snapshot_t {
    int32_t version = 0;
    std::vector<PluginSynth::param_float_snapshot_t> params;
    std::vector<modulation_snapshot_t> modulations;
    std::vector<adsr_snapshot_t> adsrs;
    std::vector<lfo_snapshot_t> lfos;
    std::vector<ui_layout_t> uiLayout;
};

static constexpr int32_t SYNTH_GPU_SNAPSHOT_VERSION = 0;

std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot);
bool deserializeSnapshot(const std::shared_ptr<std::vector<std::byte>>& data, snapshot_t& snapshotOut);

} // namespace PluginSynth::GPU
