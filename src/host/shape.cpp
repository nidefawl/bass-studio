#include "shape.h"
#include "assert_dbg.h"
#include <cstdint>
#include <glm/geometric.hpp>
#include <vector>
#include "logging.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "seq_util.h"

namespace DAW::Shape {

    int shape_t::getMinDistEdge(vec2 pos, vec2 scale, float* fDist) const {
        if (pts.empty()) {
            return -1;
        }
        if (pts.size() < 2) {
            return 0;
        }
        float xMinSampledDist = -1;
        float minDist = -1.0f;
        float sampleRes = scale.x * 0.5f;
        auto xPx = math::ceilfS32(sampleRes);
        for (int32_t i = 0; i < xPx; ++i) {
            float x = i / sampleRes;
            float sampleX = pos.x + (x * 2.0f - 1.0f)*0.15f;
            float y = sampleCurve(sampleX, false);
            vec2 pt = vec2(sampleX, y) * scale;
            float dist = glm::distance(pos * scale, pt);
            if (minDist < 0.0f || dist < minDist) {
                minDist = dist;
                xMinSampledDist = sampleX;
            }
        }
        int32_t idx = -1;
        {
            auto numPtsMin1 = CtrSize(pts) - 1;
            if (flags & SHAPE_CYCLIC && xMinSampledDist >= 0.0f && xMinSampledDist < 0.5f) {
                auto lastPt = pts.back();
                lastPt.pos.x -= 1.0f;
                if (lastPt.pos.x <= xMinSampledDist && pts.front().pos.x >= xMinSampledDist) {
                    idx = numPtsMin1;
                }
            }
            for (int32_t i = 0; i < numPtsMin1; ++i) {
                float x0 = pts[i].pos.x;
                float x1 = pts[i + 1].pos.x;
                float dx = x1 - x0;
                if (dx * scale.x < 3.0f) {
                    float dist = math::distancePointLine(pos * scale, pts[i+1].pos * scale, pts[i].pos * scale);
                    if (dist < minDist) {
                        minDist = dist;
                        idx = i;
                    }
                } 
                if (idx < 0 && xMinSampledDist >= 0.0f && x0 <= xMinSampledDist && x1 >= xMinSampledDist) {
                    idx = i;
                }
            }
            if ((flags & SHAPE_CYCLIC) && xMinSampledDist >= 0.0f && idx < 0 && xMinSampledDist > 0.5f) {
                auto firstPt = pts.front();
                firstPt.pos.x += 1.0f;
                if (pts.back().pos.x <= xMinSampledDist && firstPt.pos.x >= xMinSampledDist) {
                    idx = numPtsMin1;
                }
            }
        }
        if (idx > -1) {
            if (fDist) {
                *fDist = minDist;
            }
            if (minDist > GetMinDistEdgeMouseHit()) {
                return -1;
            }
        }
        return idx;
    }

    float shape_t::sampleCurve(float posX, bool sampleLeftRight) const {
        dbgassert(!pts.empty());
        if (pts.empty())
            return 0.0f;
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
        if (pt0.pos.x >= pt1.pos.x) {
            pt1.pos.x += 1;
        }
        if (pt0.pos.x > pX) {
            pX += 1;
        }
        float diffX = math::abs(pt1.pos.x - pt0.pos.x);
        if (diffX < 0.00001f) {
            return pt1.pos.y;
        }
        float t = (pX - pt0.pos.x) / (pt1.pos.x - pt0.pos.x);
        t = shapeSegment(t, pt0.shape);
        float y = pt0.pos.y + (pt1.pos.y - pt0.pos.y) * t;

        return y;
    }

    float shape_t::sampleCurveOneShot(float posX) const {
        dbgassert(!pts.empty());
        if (pts.empty())
            return 0.0f;
        if (pts.size() == 1) {
            return pts[0].pos.y;
        }
        float pX = posX;
        if (pX < 0.0f) {
            return pts[0].pos.y;
        }
        if (pX > 1.0f) {
            return pts[pts.size() - 1].pos.y;
        }
        size_t idx = 0;
        for (size_t i = 0; i < pts.size(); i++) {
            float px = pts[i].pos.x;
            if (px >= pX) {
                idx = i;
                break;
            }
        }
        auto pt0 = idx == 0 ? pts.back() : pts[idx - 1];
        auto pt1 = pts[idx];
        if (pt0.pos.x >= pt1.pos.x) {
            pt1.pos.x += 1;
        }
        float diffX = math::abs(pt1.pos.x - pt0.pos.x);
        if (diffX < 0.00001f) {
            return pt1.pos.y;
        }
        float t = (pX - pt0.pos.x) / (pt1.pos.x - pt0.pos.x);
        t       = shapeSegment(t, pt0.shape);
        float y = pt0.pos.y + (pt1.pos.y - pt0.pos.y) * t;

        return y;
    }

    float shape_t::shapeSegment(float t, float shape) const {
        if (shape != 0.5f && flags & SHAPE_SHAPED) {
            float shapeBi  = 1.0f - shape * 2.0f;
            float shapeExp = 0.0f;
            float scale2   = 0.2f + t * 0.8f;
            if (shapeBi < 0.0f) {
                shapeExp = 1.0f + scale2 * std::fabs(shapeBi) * 16.f;
            } else {
                shapeExp = 1.0f / (1.0f + scale2 * std::fabs(shapeBi) * 16.f);
            }
            t = std::powf(t, shapeExp);
        }
        if (flags & SHAPE_EASEINOUT) {
            t = t * t * (3.0f - 2.0f * t);
        }
        if (flags & SHAPE_INVERT) {
            t = 1.0f - t;
        }
        return t;
    }

    int shape_t::getMinPt(vec2 local, vec2 scale, float* distance) const {
        int minIdx         = -1;
        float minDist      = 0;
        const auto ctrSize = CtrSize(pts);
        for (int i = 0; i < ctrSize; i++) {
            auto dist = glm::distance(local * scale, pts[i].pos * scale);
            if (minIdx < 0 || minDist > dist) {
                minIdx  = i;
                minDist = dist;
            }
        }
        if (!pts.empty() && pts.front().pos.x < 0.01f) {
            auto dist = glm::distance(local * scale, (pts.front().pos + vec2(1.0f, 0.0f)) * scale);
            if (minIdx < 0 || minDist > dist) {
                minIdx  = 0;
                minDist = dist;
            }
        }
        if (minIdx > -1) {
            if (distance) {
                *distance = minDist;
            }
            if (minDist > GetMinDistPointMouseHit()) {
                return -1;
            }
        }
        return minIdx;
    }

    void shape_t::eraseDuplicates() {
        if (pts.size() > 1) {
            for (auto it = pts.begin(); it != pts.end() - 1;) {
                auto& pt     = *it;
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

    void shape_t::sort() {
        std::sort(pts.begin(), pts.end(), [](const shape_pt_t& a, const shape_pt_t& b) {
            return a.pos.x < b.pos.x;
        });
    }

    shape_pt_t& shape_t::getPointAfterIdx(int idx) {
        idx++;
        while (idx < 0) {
            idx += CtrSize(pts);
        }
        while (idx > CtrSize(pts) - 1) {
            idx -= CtrSize(pts);
        }
        return pts[idx];
    }

    shape_t::hit_result shape_t::getMouseHit(vec2 localPos, vec2 scale) const {
        hit_result result;
        float minDist     = 0.0f;
        auto minPtIdx     = getMinPt(localPos, scale, &minDist);
        float minDistEdge = 0.0f;
        auto minEdgeIdx   = getMinDistEdge(localPos, scale, &minDistEdge);
        if (minPtIdx >= 0) {
            result.type = hittype::HIT_NODE;
            result.idx  = minPtIdx;
            result.dist = minDist;
        } else if (minEdgeIdx >= 0) {
            result.type = hittype::HIT_EDGE;
            result.idx  = minEdgeIdx;
            result.dist = minDistEdge;
        } else {
            result.type = hittype::HIT_NONE;
            result.dist = math::absMin(minDistEdge, minDist);
        }
        return result;
    }

    shape_base_t GetShapeSaw(int32_t flags) {
        auto pt1 = shape_pt_t{ { 0.0f, 1.0f }, 0.5f };
        auto pt2 = shape_pt_t{ { 1.0f, 0.0f }, 0.5f };
        return shape_base_t{ { { pt1, pt2 } }, "Saw", -1.0f, flags };
    }

    shape_base_t GetShapeSawInverse(int32_t flags) {
        auto pt1 = shape_pt_t{ { 0.0f, 0.0f }, 0.5f };
        auto pt2 = shape_pt_t{ { 1.0f, 1.0f }, 0.5f };
        return shape_base_t{ { { pt1, pt2 } }, "Inverse Saw", -1.0f, flags };
    }

    shape_base_t GetShapeTriangle(int32_t flags) {
        auto pt1 = shape_pt_t{ { 0.0f, 0.0f }, 0.5f };
        auto pt2 = shape_pt_t{ { 0.5f, 1.0f }, 0.5f };
        auto pt3 = shape_pt_t{ { 1.0f, 0.0f }, 0.5f };
        return shape_base_t{ { { pt1, pt2, pt3 } }, "Saw", -1.0f, flags };
    }

}// namespace DAW::Shape