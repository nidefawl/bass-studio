#pragma once
#include "gui/container/container.hpp"
#include "gui/table/table.hpp"

class gui_dragged_files final : public guictr_base {
    const int HEIGHT_ENTRY = 20;
    bool bIsExternal = false;
    std::vector<String> fileList;
    Table::tbl table;
public:
    gui_dragged_files() : guictr_base(gui_type::CTR_TYPE_DRAGGED_FILE) {
        pos = { 0, 0 };
        setDragRendered(true);
    }
    ~gui_dragged_files() override = default;
    void layout() override {
    }
    void setExternal(bool b) {
        bIsExternal = b;
    }
    void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override;
    void setFiles(const std::vector<String>& list);
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
};