#pragma once
#include "types.h"

struct DrawVBO {
    uint32_t vaoId      = 0;
    uint32_t vboVertId  = 0;
    uint32_t vboIdxId   = 0;
    uint32_t nIndices   = 0;
    size_t vboVertSize = 0;
    size_t vboIdxSize  = 0;
    ~DrawVBO();
    void destroy();
    void genBuffers();

    void uploadBuffer(uint32_t bufferType, void* ptr, size_t len);
};
