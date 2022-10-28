#pragma once
#include "str_util.h"

class PluginInfo {
public:
   PluginInfo() = default;

private:
   String _name;
   String _file;
   String _index; // in case of shell plugin
};
