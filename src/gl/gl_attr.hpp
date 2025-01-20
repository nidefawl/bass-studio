#pragma once
#include <vector>

struct VertexAttr {
    const char* name{};
    int32_t elements = -1;
    int32_t type = -1;
    int32_t bindingPt = -1;
};

/** This is only required to be called once per vao */
void bindVertexAttributes(std::vector<VertexAttr>& attr, int32_t fixedStride = 0);
