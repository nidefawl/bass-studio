#pragma once


#ifdef __cplusplus
extern "C" {
#endif

void failedAssert(const char* expr, const char *file, int line);

#define dbgassert(_Expression) \
 (void) \
 ((!!(_Expression)) || \
  (failedAssert(#_Expression,__FILE__,__LINE__),0))

#ifdef __cplusplus
}
#endif

#ifdef NDEBUG
#define always_assert(_Expression) ((void)_Expression)
#else /* !defined (NDEBUG) */
#define always_assert(_Expression) dbgassert(_Expression)
#endif
