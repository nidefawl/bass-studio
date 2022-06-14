#pragma once
#include "gui/controls/button.h"
#include "container.h"

class DawInstance;
struct guictr_layout_entry;
struct ContainerInstanceContext {
    DawInstance* const daw;
};

using ContainerBuilder = std::function<std::shared_ptr<guictr_base>(ContainerInstanceContext& ctxt)>;
using ContainerFactory = std::map<gui_type, ContainerBuilder>;
ContainerFactory& getContainerFactory();
std::shared_ptr<guictr_layout_entry> createGuiCtrLayoutEntry(std::shared_ptr<guictr_base> ctr);
bool getContainerLabel(gui_type type, String& out);
bool makeContainer(ContainerInstanceContext& ctxt, gui_type type, std::shared_ptr<guictr_base>& out);
