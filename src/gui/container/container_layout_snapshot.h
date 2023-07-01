#pragma once
#include "gui/gui.h"
#include "gui/container/container.h"
#include <vector>
#include <memory>

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
    std::shared_ptr<guictrlayout_entry_snapshot_t> left;
    std::shared_ptr<guictrlayout_entry_snapshot_t> right;
    std::shared_ptr<guictrlayout_entry_snapshot_t> center;
    std::vector<float> splitterPositions;
};

bool saveDawViewLayoutSnapshot(dawview_layout_t& snapshot, const String& path);
std::shared_ptr<dawview_layout_t> loadDawViewLayoutSnapshot(const String& path);
