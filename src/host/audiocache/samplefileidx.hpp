#pragma once
#include "types.hpp"
#include <vector>
#include "str_util.hpp"

struct samplefile_entry_t {
    int32_t id = -1;
    String name;
};

struct samplefile_index_t {
    std::vector<samplefile_entry_t> list;
};
