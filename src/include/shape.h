#pragma once
#include "assert_dbg.h"
#include <glm/geometric.hpp>
#include <vector>
#include "logging.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "seq_util.h"

namespace DAW::Shape {

struct shape_pt_t {
    vec2 pos;
    float shape = 0.5f;
};
inline bool operator==(const shape_pt_t& lhs, const shape_pt_t& rhs) {
    return lhs.pos == rhs.pos && lhs.shape == rhs.shape;
}
enum ShapeFlags {
    SHAPE_FLAGS_NONE = 0,
    SHAPE_CYCLIC = 1 << 0,
    SHAPE_SHAPED = 1 << 1,
    SHAPE_EASEINOUT = 1 << 2,
    SHAPE_INVERT = 1 << 4,
    SHAPE_LOCK_POINTS = 1 << 8,
};

/* struct shape_base_t {
    std::vector<shape_pt_t> pts;
    String name;
    float renderPhase = -1.0f;
    int32_t flags = SHAPE_FLAGS_NONE;
}; */
inline float GetMinDistPointMouseHit() { return 10.0f; }
inline float GetMinDistEdgeMouseHit() { return 5.0f; }

struct shape_t {
    std::vector<shape_pt_t> pts;
    String name;
    float renderPhase = -1.0f;
    int32_t flags = SHAPE_FLAGS_NONE;
    shape_t() = default;
    shape_t(int32_t shapeFlags, std::vector<shape_pt_t> pts, String name, float renderPhase = 1.0f) {
        this->flags = shapeFlags;
        this->pts = std::move(pts);
        this->name = std::move(name);
        this->renderPhase = renderPhase;
    }
    float shapeSegment(float t, float shape) const;
    float sampleCurveOneShot(float posX) const;
    float sampleCurve(float posX, bool sampleLeftRight) const;
    int getMinDistEdge(vec2 pos, vec2 scale, float* fDist) const;

    int getMinPt(vec2 local, vec2 scale, float* distance = nullptr) const;
    void eraseDuplicates();
    void sort();
    shape_pt_t& getPointAfterIdx(int idx);

public:
    
    enum class hittype {
        HIT_NONE,
        HIT_NODE,
        HIT_EDGE,
    };
    struct hit_result {
        hittype type = hittype::HIT_NONE;
        int32_t idx = -1;
        float dist = 0.0f;
    };
    hit_result getMouseHit(vec2 localPos, vec2 scale) const;
};
shape_t GetShapeSaw(int32_t flags = SHAPE_CYCLIC);
shape_t GetShapeSawInverse(int32_t flags = SHAPE_CYCLIC);
shape_t GetShapeTriangle(int32_t flags = SHAPE_CYCLIC);
} // namespace DAW::Shape


