#pragma once

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#endif
#ifdef __linux__
#include <xmmintrin.h>
#endif

struct RegisterStatus_SSE_CS {
    unsigned int registerBits = 0;
    int regExceptionMask      = 0;
    int regExceptionState     = 0;
    int regFlushZeroMode      = 0;
    int regDenormalsAreZero   = 0;
    int regRoundingMode       = 0;
};
inline RegisterStatus_SSE_CS getSSEControlStatusRegister() {
    RegisterStatus_SSE_CS status;
    status.registerBits        = _mm_getcsr();
    status.regExceptionMask    = (int)(status.registerBits & _MM_MASK_MASK);
    status.regExceptionState   = (int)(status.registerBits & _MM_EXCEPT_MASK);
    status.regFlushZeroMode    = (int)(status.registerBits & _MM_FLUSH_ZERO_MASK);
    status.regDenormalsAreZero = (int)(status.registerBits & _MM_DENORMALS_ZERO_MASK);
    status.regRoundingMode     = (int)(status.registerBits & _MM_ROUND_MASK);
    return status;
}
inline void setSSEFlushDenormals() {
    /* Set FTZ and DAZ flags in the MXCSR control and status register */
    _mm_setcsr(_mm_getcsr() | _MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON);
}
inline void setSSENoFlushDenormals() {
    /* Clear FTZ and DAZ flags in the MXCSR control and status register */
    _mm_setcsr(_mm_getcsr() & ~(_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON));
}
