#pragma once
#include "types.h"
#include "math/vec.h"
#include "gl_vbo.h"
#include "assert_dbg.h"
#include <vector>
#include <array>
#include <glm/gtc/type_ptr.hpp>

#define TESS_COLOR 1
#define TESS_ATTR 2
#define TESS_ATTR2 4
class tess2d {
    std::vector<float> buf;
    uint32_t flags;
    uint32_t vertexcount = 0;
    vec4 rgba{1.0f, 1.0f, 1.0f, 1.0f};
    vec2 offset{0.0f, 0.0f};
    vec2 uv{0.0f, 0.0f};
    vec4 attr{0.0f, 0.0f, 0.0f, 0.0f};
    vec4 attr2{0.0f, 0.0f, 0.0f, 0.0f};

public:
    explicit tess2d(uint32_t _flags = 0) : flags(_flags) {
    }
    uint32_t size() {
        return getBufIdx(vertexcount);
    }
    uint32_t count() {
        return vertexcount;
    }
    float* data() {
        return buf.data();
    }

    void setOffset(vec2 offset) {
        this->offset = offset;
    }
    void setColor(vec4 color) {
        if (!(flags & TESS_COLOR)) {
            dbgassert(0 && "tesselator flag TESS_COLOR not set!");
        }
        this->rgba = color;
    }
    void setAttrs(vec4 i) {
        if (!(flags & TESS_ATTR)) {
            dbgassert(0 && "tesselator flag TESS_ATTR not set!");
        }
        this->attr = i;
    }
    void setAttrIdx(int32_t idx, float f) {
        if (!(flags & TESS_ATTR)) {
            dbgassert(0 && "tesselator flag TESS_ATTR not set!");
        }
        this->attr[idx] = f;
    }
    void setAttrs2(vec4 i) {
        if (!(flags & TESS_ATTR2)) {
            dbgassert(0 && "tesselator flag TESS_ATTR2 not set!");
        }
        this->attr2 = i;
    }
    void setAttr2Idx(int32_t idx, float f) {
        if (!(flags & TESS_ATTR2)) {
            dbgassert(0 && "tesselator flag TESS_ATTR2 not set!");
        }
        this->attr2[idx] = f;
    }
    void add(float x, float y) {
        add({x, y});
    }
    void add(float x, float y, float u, float v) {
        uv.x = u;
        uv.y = v;
        add({x, y});
    }
    void add(vec2 v) {
        static_assert(sizeof(vec2) == sizeof(float) * 2, "sizeof vec2 is not sizeof float * 2");

        auto index = getBufIdx(vertexcount);
        if (buf.size() < index + getVSize()) {
            buf.resize(buf.size() + 256);
        }
        vec2 pos      = v + offset;
        float* bufPos = buf.data() + index;

        memcpy(bufPos, value_ptr(pos), sizeof(vec2));
        bufPos += 2;
        memcpy(bufPos, value_ptr(uv), sizeof(vec2));
        bufPos += 2;
        if (flags & TESS_COLOR) {
            memcpy(bufPos, value_ptr(rgba), sizeof(vec4));
            bufPos += 4;
        }
        if (flags & TESS_ATTR) {
            memcpy(bufPos, value_ptr(attr), sizeof(vec4));
            bufPos += 4;
        }
        if (flags & TESS_ATTR2) {
            memcpy(bufPos, value_ptr(attr2), sizeof(vec4));
            bufPos += 4;
        }
        vertexcount++;
    }
    size_t getBufIdx(uint32_t v) {
        return getVSize() * v;
    }
    size_t getVSize() {
        size_t size = 2 + 2;

        if (flags & TESS_COLOR) {
            size += 4;
        }

        if (flags & TESS_ATTR) {
            size += 4;
        }
        if (flags & TESS_ATTR2) {
            size += 4;
        }

        return size;
    }
    void reset() {
        buf.clear();
        vertexcount = 0;
    }
    void store(std::vector<float>& _outFloat, std::vector<uint32_t>& _outInt) {
        _outFloat.resize(vertexcount * getVSize());
        memcpy(_outFloat.data(), buf.data(), vertexcount * getVSize() * sizeof(float));
        _outInt.reserve(vertexcount * 6ULL / 4ULL);
        buildQuadIndices(vertexcount, 0, _outInt);
    }

    static void buildQuadIndices(uint32_t numVertices, uint32_t offset, std::vector<uint32_t>& _out) {
        static std::array<uint32_t, 6> quadIndices { 0, 1, 2, 0, 2, 3 };
        for (uint32_t i = 0; i < numVertices; i++) {
            for (uint32_t idx : quadIndices)
                _out.push_back(offset + idx + i * 4);
        }
    }

    static void uploadVBO(tess2d& tess, DrawVBO& vbo);
    static inline void fullscreenQuad(tess2d& tess, float w, float h) {
        float x  = 0;
        float y  = 0;
        float tw = w;
        float th = h;
        tess.add(x + tw,   y,      1.0f, 1.0f);
        tess.add(x,        y,      0.0f, 1.0f);
        tess.add(x,        y + th, 0.0f, 0.0f);
        tess.add(x + tw,   y + th, 1.0f, 0.0f);
    }
};
