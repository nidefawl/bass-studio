#pragma once
#include "gui/button.h"
#include "gui/guicontainer.h"

struct guictr_layout_entry;

using ContainerBuilder = std::function<std::shared_ptr<guictr_base> ()>;
std::map<container_type, ContainerBuilder>& getContainerFactory();
std::shared_ptr<guictr_layout_entry> createGuiCtrLayoutEntry(std::shared_ptr<guictr_base> ctr);
