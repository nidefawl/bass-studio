#pragma once
#include "basectrl.h"
#include "gui/container/container_builder.h"
#include "gui/controls/button.h"
#include "container.h"
#include "gui/gui.h"
#include <memory>
#include <vector>
#include <map>

class DawInstance;
struct GuiCtrLayoutEntry;
struct ContainerInstanceContext {
    DawInstance* const daw = nullptr;
    DawCtrl* const dawCtrl = nullptr;
    std::map<int32_t, std::shared_ptr<GuiCtrLayoutEntry>> entriesPreconstructed{};
    std::map<gui_type, int32_t> stats{};
    std::map<gui_type, std::vector<std::shared_ptr<GuiCtrLayoutEntry>>> entriesConstructed{};
};

using ContainerBuilder = std::function<std::shared_ptr<guictr_base>(create_ctr_t& ctxt)>;
using ContainerFactory = std::map<gui_type, ContainerBuilder>;
using ContainerRegistry = std::vector<std::pair<gui_type, String>>;
ContainerFactory& getContainerFactory();
ContainerRegistry& getContainerRegistry();
std::shared_ptr<GuiCtrLayoutEntry> createGuiCtrLayoutEntry(const std::shared_ptr<guictr_layout>& ctr);
std::shared_ptr<GuiCtrLayoutEntry> createGuiCtrLayoutEntry(const std::shared_ptr<guictr_base>& ctr);
bool getContainerLabel(gui_type type, String& out);
bool makeContainer(ContainerInstanceContext& ctxt, gui_type type, std::shared_ptr<guictr_base>& out);
