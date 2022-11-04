#pragma once

#ifndef DBG_ASSERT_DISABLE
#ifdef NDEBUG
#define DBG_ASSERT_DISABLE 1
#else
#define DBG_ASSERT_DISABLE 0
#endif
#endif

#if DBG_ASSERT_DISABLE

#define assert_expr(_Expression) (!!(_Expression))


//#define dbgassert (void)
#define dbgassert(_Expression)
#define always_assert(_Expression) ((void)(_Expression))

#else // DBG_ASSERT_DISABLE

#ifdef __cplusplus
#include "compiler.h"

void CPP_failedAssert(const char* expr, const char *file, int line);

#define dbgassert(_Expression) \
 (void) \
 (hint_unlikely(!!(_Expression)) || \
  (CPP_failedAssert(#_Expression,__FILE__,__LINE__),0))

#define assert_expr(_Expression) \
 ((!!(_Expression)) || \
  (CPP_failedAssert(#_Expression,__FILE__,__LINE__),0))

#define always_assert(_Expression) dbgassert(_Expression)

#else/* !defined (__cplusplus) */

void C_failedAssert(const char* expr, const char *file, int line);

#define dbgassert(_Expression) \
 (void) \
 ((!!(_Expression)) || \
  (C_failedAssert(#_Expression,__FILE__,__LINE__),0))

//No need for other macros in C


#endif


#endif // DBG_ASSERT_DISABLE
