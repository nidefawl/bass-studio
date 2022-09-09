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

struct shape_base_t {
    std::vector<shape_pt_t> pts;
};
struct shape_t : public shape_base_t {
    inline float shapeSegment(float t, float shape) const {
        if (shape != 0.5f) {
            float shapeBi = 1.0f-shape*2.0f;
            float shapeExp = 0.0f;
            float scale2 = 0.2f+t*0.8f;
            if (shapeBi < 0.0f) {
                shapeExp = 1.0f + scale2 * std::fabs(shapeBi) * 16.f;
            } else {
                shapeExp = 1.0f / (1.0f + scale2 * std::fabs(shapeBi) * 16.f);
            }
            t = std::powf(t, shapeExp);
        }
        return t;
    }
    float sampleCurve(float posX, bool sampleLeftRight) const {
        if (pts.size() == 1) {
            return pts[0].pos.y;
        }
        float pX = posX;
        while (pX > 1.0f) {
            pX -= 1.0f;
        }
        while (pX < 0.0f) {
            pX += 1.0f;
        }
        size_t idx = 0;
        for (size_t i = 0; i < pts.size(); i++) {
            float px = pts[i].pos.x;
            if (!sampleLeftRight && px >= pX) {
                idx = i;
                break;
            }
            if (sampleLeftRight && px > pX) {
                idx = i;
                break;
            }
        }
        auto pt0 = idx == 0 ? pts.back() : pts[idx - 1];
        auto pt1 = pts[idx];
        if (pt0.pos.x > pt1.pos.x) {
            pt1.pos.x += 1;
        }
        if (pt0.pos.x > pX) {
            pX += 1;
        }
        float diffX = math::abs(pt1.pos.x - pt0.pos.x);
        if (diffX < 0.00001f) {
            return pt0.pos.y;
        }
        float t = (pX - pt0.pos.x) / (pt1.pos.x - pt0.pos.x);
        t = shapeSegment(t, pt0.shape);
        float y = pt0.pos.y + (pt1.pos.y - pt0.pos.y) * t;

        return y;
    }
    int getMinDistEdge(vec2 pos, float* fDist) const {
        if (pts.empty()) {
            return -1;
        }
        if (pts.size() < 2) {
            return 0;
        }
        int32_t idx = -1;
        float minDist = -1.0f;
        for (size_t i = 0; i < pts.size()-1; ++i) {
            auto dist = math::distancePointLine(pos, pts[i].pos, pts[i+1].pos);
            if (minDist < 0 || dist < minDist) {
                minDist = dist;
                idx = i;
            }
        }
        auto firstPt = pts[0].pos;
        if (pos.x > firstPt.x) {
            firstPt.x += 1;
        }
        auto dist = minDist;
        if (pos.x > pts.back().pos.x) dist = math::distancePointLine(pos, pts.back().pos, firstPt + vec2(1.0, 0.0));
        if (pos.x < pts.front().pos.x) dist = math::distancePointLine(pos, pts.back().pos - vec2(1.0, 0.0), firstPt);
        if (minDist < 0 || dist < minDist) {
            minDist = dist;
            idx = static_cast<int32_t>(pts.size()) - 1;
        }
        if (fDist) {
            *fDist = minDist;
        }
        return idx;
    }
    int getMinPt(vec2 local, float* distance = nullptr) const {
        float minCollisionDistance = 0.05f;
        int minIdx     = -1;
        float minDist = 0;
        const auto ctrSize = CtrSize(pts);
        for (int i = 0; i < ctrSize; i++) {
            auto dist = glm::distance(local, pts[i].pos);
            if (minIdx < 0 || minDist > dist) {
                minIdx  = i;
                minDist = dist;
            }
        }
        if (!pts.empty() && pts.front().pos.x < 0.01f) {
            auto dist = glm::distance(local, pts.front().pos + vec2(1.0f, 0.0f));
            if (minIdx < 0 || minDist > dist) {
                minIdx  = 0;
                minDist = dist;
            }
        }
        if (minIdx > -1) {
            *distance = minDist;
            if (minDist > minCollisionDistance) {
                return -1;
            }
        }
        return minIdx;
    }
    void eraseDuplicates() {
        if (pts.size() > 1) {
            for (auto it = pts.begin(); it != pts.end() - 1;) {
                auto& pt = *it;
                auto& nextPt = *(it + 1);
                if (math::abs(pt.pos.x - nextPt.pos.x) < 0.0001f && math::abs(pt.pos.y - nextPt.pos.y) < 0.0001f) {
                    it = pts.erase(it);
                    log_lf(Log::L_DEBUG, "Erased close pt\n");
                } else {
                    ++it;
                }
            }
        }
    }
    void sort() {
        std::sort(pts.begin(), pts.end(), [](const shape_pt_t& a, const shape_pt_t& b) {
            return a.pos.x < b.pos.x;
        });
    }
    shape_pt_t& getPointAfterIdx(int idx) {
        idx++;
        while (idx < 0) {
            idx += CtrSize(pts);
        }
        while (idx > CtrSize(pts)-1) {
            idx -= CtrSize(pts);
        }
        return pts[idx];
    }
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
    hit_result getMouseHit(vec2 localPos) const {
        hit_result result;
        float minDist = 0.0f;
        auto minPtIdx = getMinPt(localPos, &minDist);
        float minDistEdge = 0.0f;
        auto minEdgeIdx = getMinDistEdge(localPos, &minDistEdge);
        minDistEdge += 4.0f;
        if (minEdgeIdx >= 0 && (minPtIdx < 0 || minDist > minDistEdge)) {
            result.type = hittype::HIT_EDGE;
            result.idx = minEdgeIdx;
            result.dist = minDistEdge;
        } else if (minPtIdx >= 0) {
            result.type = hittype::HIT_NODE;
            result.idx = minPtIdx;
            result.dist = minDist;
        } else {
            result.type = hittype::HIT_NONE;
            result.dist = math::absMin(minDistEdge, minDist);
        }
        return result;
    }
};

} // namespace DAW::Shape