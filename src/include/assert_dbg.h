#pragma once


#ifdef NDEBUG

#define assert_expr(_Expression) (!!(_Expression))
#define dbgassert (void)
#define always_assert(_Expression) ((void)(_Expression))

#else/* !defined (NDEBUG) */

#ifdef __cplusplus

void CPP_failedAssert(const char* expr, const char *file, int line);

#define dbgassert(_Expression) \
 (void) \
 ((!!(_Expression)) || \
  (CPP_failedAssert(#_Expression,__FILE__,__LINE__),0))

#define assert_expr(_Expression) \
 ((!!(_Expression)) || \
  (CPP_failedAssert(#_Expression,__FILE__,__LINE__),0))

#define always_assert(_Expression) dbgassert(_Expression)

#else/* !defined (__cplusplus) */
#pragma message("C ASSERT")
void C_failedAssert(const char* expr, const char *file, int line);

#define dbgassert(_Expression) \
 (void) \
 ((!!(_Expression)) || \
  (C_failedAssert(#_Expression,__FILE__,__LINE__),0))

//No need for other macros in C


#endif


#endif /* NDEBUG */
