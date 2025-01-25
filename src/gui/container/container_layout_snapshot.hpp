#pragma once
#include "gui/gui.hpp"
#include "gui/container/container.hpp"
#include <vector>
#include <memory>
#include <optional>
#include <variant>

struct guictrlayout_entry_snapshot_t {
    gui_type type;
    String label;
    container_layout ctrLayout = container_layout::SOLE;
    int32_t activePosition     = -1;
    std::vector<std::shared_ptr<guictrlayout_entry_snapshot_t>> entries;
    std::vector<float> splitterPositions;
    int32_t entryTag = -1;
    String data;
};


struct dawview_layout_t {
    int32_t version = -1;
    std::shared_ptr<guictrlayout_entry_snapshot_t> left;
    std::shared_ptr<guictrlayout_entry_snapshot_t> right;
    std::shared_ptr<guictrlayout_entry_snapshot_t> center;
    std::vector<float> splitterPositions;
};

namespace DAW::ProjectFileV1 {
std::optional<String> saveDawViewLayoutSnapshot(dawview_layout_t& snapshot, const String& path);
std::variant<std::shared_ptr<dawview_layout_t>, String> loadDawViewLayoutSnapshot(const String& path);
} // namespace DAW::ProjectFileV1

namespace DAW::ProjectFileV2 {
std::optional<String> saveDawViewLayoutSnapshot(dawview_layout_t& snapshot, const String& path);
std::variant<std::shared_ptr<dawview_layout_t>, String> loadDawViewLayoutSnapshot(const String& path);
} // namespace DAW::ProjectFileV2
