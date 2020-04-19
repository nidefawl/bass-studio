#pragma once


#ifdef __cplusplus
extern "C" {
#endif

void failedAssert(const char* expr, const char *file, int line);

#ifdef NDEBUG
#define assert_expr(_Expression) (!!(_Expression))
#define dbgassert (void)
#else
#define dbgassert(_Expression) \
 (void) \
 ((!!(_Expression)) || \
  (failedAssert(#_Expression,__FILE__,__LINE__),0))
#define assert_expr(_Expression) \
 ((!!(_Expression)) || \
  (failedAssert(#_Expression,__FILE__,__LINE__),0))

#endif
#ifdef __cplusplus
}
#endif

#ifdef NDEBUG
#define always_assert(_Expression) ((void)_Expression)
#else /* !defined (NDEBUG) */
#define always_assert(_Expression) dbgassert(_Expression)
#endif
