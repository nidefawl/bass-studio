#pragma once

#include <assert.h>
#ifdef NDEBUG
#define always_assert(_Expression) ((void)_Expression)
#else /* !defined (NDEBUG) */
#define always_assert(_Expression) assert(_Expression)
#endif
