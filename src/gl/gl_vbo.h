#pragma once
#include <stdint.h>

struct DrawVBO {
	uint32_t vaoId = 0;
	uint32_t vboVertId = 0;
	uint32_t vboIdxId = 0;
	uint32_t nIndices = 0;
	~DrawVBO();
	void genBuffers();
};
