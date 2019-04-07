#pragma once
#define ERR_ACCESSVIOLATION 1
#define ERR_UNKNOWN 2
int handleFatalError(int type, int implSpecType);


#undef HAVE_BUILTIN_TRAP
#ifdef __GNUC__
#  define GCC_VERSION (__GNUC__ * 10000 \
    + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#  if GCC_VERSION > 40203
#    define HAVE_BUILTIN_TRAP
#  endif
#else
#  ifdef __has_builtin
#    if __has_builtin(__builtin_trap)
#      define HAVE_BUILTIN_TRAP
#    endif
#  endif
#endif

#ifdef HAVE_BUILTIN_TRAP
#  define debugRaiseSegFault() __builtin_trap()
#else
#  include <stdio.h>
#  define debugRaiseSegFault() do { \
    int *volatile iptr = 0; \
    int i = *iptr; \
    printf("%d", i); \
	} while (0)
#endif
