#pragma once
#include "str_util.hpp"
#include "types.hpp"
#include <vector>
#include <memory>
#include "file/shapefile.hpp"

namespace PluginLFO {

struct impl_channel_snapshot_t;

struct ui_layout_t {
    int32_t uiId = 0;
    int32_t numActive = 0;
};

struct snapshot_t {
    int32_t version = 0;
    std::vector<ui_layout_t> uiLayout;
    std::vector<impl_channel_snapshot_t> channels;
};

struct impl_channel_snapshot_t {
    DAW::Shape::shape_snapshot_t shape;
    int32_t syncFlags = false;
    bool modeIsShape = true;
    int32_t modeRandom = -1;
};

} // namespace PluginLFO
