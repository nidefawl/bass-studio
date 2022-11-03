#pragma once
#include "gui/controls/button.h"
#include "container.h"
#include "gui/gui.h"
#include <memory>
#include <vector>
#include <map>

class DawInstance;
struct guictr_layout_entry;
struct ContainerInstanceContext {
    DawInstance* const daw;
    std::map<gui_type, std::vector<std::shared_ptr<guictr_layout_entry>>> entriesPreconstructed;
};

using ContainerBuilder = std::function<std::shared_ptr<guictr_base>(ContainerInstanceContext& ctxt)>;
using ContainerFactory = std::map<gui_type, ContainerBuilder>;
using ContainerRegistry = std::vector<std::pair<gui_type, String>>;
ContainerFactory& getContainerFactory();
ContainerRegistry& getContainerRegistry();
std::shared_ptr<guictr_layout_entry> createGuiCtrLayoutEntry(const std::shared_ptr<guictr_base>& ctr);
bool getContainerLabel(gui_type type, String& out);
bool makeContainer(ContainerInstanceContext& ctxt, gui_type type, std::shared_ptr<guictr_base>& out);
