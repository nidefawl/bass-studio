#pragma once
#include "gl/gl_util.hpp"
#include "types.hpp"
#include "logging.hpp"

struct DrawVBO : public OpenGLResource {
    static int32_t instanceCount;
    uint32_t vaoId      = 0;
    uint32_t vboVertId  = 0;
    uint32_t vboIdxId   = 0;
    int32_t  nIndices   = 0;
    size_t vboVertSize = 0;
    size_t vboIdxSize  = 0;
    ~DrawVBO() override;
    void destroy() override;
    void genBuffers();

    void uploadBuffer(uint32_t bufferType, void* ptr, size_t len);

    static void printLeaked() {
        if (instanceCount != 0) {
            log_lf(Log::L_WARN, "DrawVBO::instanceCount: %d\n", instanceCount);
        }
    }
};
