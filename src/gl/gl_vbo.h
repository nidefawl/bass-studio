#pragma once
#include "types.h"

struct DrawVBO {
    uint32_t vaoId      = 0;
    uint32_t vboVertId  = 0;
    uint32_t vboIdxId   = 0;
    int64_t nIndices   = 0;
    int64_t vboVertSize = 0;
    int64_t vboIdxSize  = 0;
    ~DrawVBO();
    void destroy();
    void genBuffers();

    void uploadBuffer(uint32_t bufferType, void* ptr, int64_t len);
};
