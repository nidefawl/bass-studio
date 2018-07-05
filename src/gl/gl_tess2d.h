#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "gl_vbo.h"

using glm::mat4x4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
#define TESS_COLOR 1
class tess2d {
	std::vector<float> buf;
	int flags;
	int vertexcount = 0;
	vec4 rgba { 1.0f, 1.0f, 1.0f, 1.0f };
	vec2 offset { 0.0f, 0.0f};
	vec2 uv { 0.0f, 0.0f };
public:
	tess2d() : flags(0) {
	}
	tess2d(int _flags) : flags(_flags) {
	}
	int size() {
		return getBufIdx(vertexcount);
	}
	int count() {
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
        	assert(0&&"tesselator flag TESS_COLOR not set!");
        }
    	this->rgba = color;
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
		static_assert(sizeof(vec2) == sizeof(float)*2, "sizeof vec2 is not sizeof float * 2");

		int32_t index = getBufIdx(vertexcount);
        if ((int32_t)buf.size() < index+getVSize()) {
        	buf.resize(buf.size()+256);
        }
		vec2 pos = v + offset;
		float* bufPos = buf.data() + index;

        memcpy(bufPos, glm::value_ptr(pos), sizeof(vec2));
        bufPos += 2;
    	memcpy(bufPos, glm::value_ptr(uv), sizeof(vec2));
        bufPos += 2;
        if (flags & TESS_COLOR) {
        	memcpy(buf.data()+index, glm::value_ptr(rgba), sizeof(vec4));
//            bufPos += 4;
        }
        vertexcount++;
    }
    int32_t getBufIdx(int v) {
        return getVSize() * v;
    }
    int32_t getVSize() {
        if (flags & TESS_COLOR) {
        	return 2+2+4;
        }
    	return 2+2;
    }
    void reset() {
    	buf.clear();
    	vertexcount = 0;
    }
    void store(std::vector<float>& _outFloat, std::vector<int>& _outInt) {
    	_outFloat.resize(vertexcount*getVSize());
    	memcpy(_outFloat.data(), buf.data(), vertexcount*getVSize()*sizeof(float));
    	_outInt.reserve((vertexcount/4) * 6);
    	buildQuadIndices(vertexcount, _outInt, 0);
    }
    static void buildQuadIndices(int vertexCount, std::vector<int>& _out, int offset=0) {
    	static int quadIdx[] = {0,1,2,0,2,3};
    	const int nQuads = vertexCount/4;
    	for (int i = 0; i < nQuads; i++) {
    		for (int j = 0; j < 6; j++)
    			_out.push_back(offset + quadIdx[j] + i*4);
    	}
    }
    static void uploadVBO(tess2d& tess, DrawVBO& vbo);
};
