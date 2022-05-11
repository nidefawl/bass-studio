#pragma once

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#endif

//#define _MM_EXCEPT_INVALID    (0x0001)
//#define _MM_EXCEPT_DENORM     (0x0002)
//#define _MM_EXCEPT_DIV_ZERO   (0x0004)
//#define _MM_EXCEPT_OVERFLOW   (0x0008)
//#define _MM_EXCEPT_UNDERFLOW  (0x0010)
//#define _MM_EXCEPT_INEXACT    (0x0020)
//#define _MM_EXCEPT_MASK       (0x003f)
//
//#define _MM_MASK_INVALID      (0x0080)
//#define _MM_MASK_DENORM       (0x0100)
//#define _MM_MASK_DIV_ZERO     (0x0200)
//#define _MM_MASK_OVERFLOW     (0x0400)
//#define _MM_MASK_UNDERFLOW    (0x0800)
//#define _MM_MASK_INEXACT      (0x1000)
//#define _MM_MASK_MASK         (0x1f80)
//
//#define _MM_ROUND_NEAREST     (0x0000)
//#define _MM_ROUND_DOWN        (0x2000)
//#define _MM_ROUND_UP          (0x4000)
//#define _MM_ROUND_TOWARD_ZERO (0x6000)
//#define _MM_ROUND_MASK        (0x6000)
//
//#define _MM_FLUSH_ZERO_MASK   (0x8000)
//#define _MM_FLUSH_ZERO_ON     (0x8000)
//#define _MM_FLUSH_ZERO_OFF    (0x0000)
//
//#define _MM_GET_EXCEPTION_MASK() (_mm_getcsr() & _MM_MASK_MASK)
//#define _MM_GET_EXCEPTION_STATE() (_mm_getcsr() & _MM_EXCEPT_MASK)
//#define _MM_GET_FLUSH_ZERO_MODE() (_mm_getcsr() & _MM_FLUSH_ZERO_MASK)
//#define _MM_GET_ROUNDING_MODE() (_mm_getcsr() & _MM_ROUND_MASK)
/*
 * MACRO functions for setting and reading the DAZ bit in the MXCSR
 */
//#define _MM_DENORMALS_ZERO_MASK   0x0040
//#define _MM_DENORMALS_ZERO_ON     0x0040
//#define _MM_DENORMALS_ZERO_OFF    0x0000
//
//#define _MM_SET_DENORMALS_ZERO_MODE(mode) _mm_setcsr((_mm_getcsr() & ~_MM_DENORMALS_ZERO_MASK) | (mode))
//#define _MM_GET_DENORMALS_ZERO_MODE() (_mm_getcsr() & _MM_DENORMALS_ZERO_MASK)

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
#ifdef _WIN32
    status.registerBits        = _mm_getcsr();
    status.regExceptionMask    = (int)(status.registerBits & _MM_MASK_MASK);
    status.regExceptionState   = (int)(status.registerBits & _MM_EXCEPT_MASK);
    status.regFlushZeroMode    = (int)(status.registerBits & _MM_FLUSH_ZERO_MASK);
    status.regDenormalsAreZero = (int)(status.registerBits & _MM_DENORMALS_ZERO_MASK);
    status.regRoundingMode     = (int)(status.registerBits & _MM_ROUND_MASK);
#endif
    return status;
}
inline void setSSEFlushDenormals() {
#ifdef _WIN32
    /* Set FTZ and DAZ flags in the MXCSR control and status register */
    _mm_setcsr(_mm_getcsr() | _MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON);
#endif
}
inline void setSSENoFlushDenormals() {
#ifdef _WIN32
    /* Clear FTZ and DAZ flags in the MXCSR control and status register */
    _mm_setcsr(_mm_getcsr() | _MM_FLUSH_ZERO_OFF | _MM_DENORMALS_ZERO_OFF);
#endif
}
