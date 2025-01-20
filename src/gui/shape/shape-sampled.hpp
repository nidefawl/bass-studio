#pragma once
#include "host/shape/shape.hpp"
#include <vector>

namespace DAW::Shape {
struct sampled_pt_t {
    vec2 pos;
};
struct sampled_curved_t {
    std::vector<sampled_pt_t> pts;
    int32_t flags = DAW::Shape::SHAPE_FLAGS_NONE;
    inline float shapeSegmentPt(float t, const sampled_pt_t& pt) const {
        return t;
    }
};
};
