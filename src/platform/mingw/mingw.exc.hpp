#pragma once

#ifdef __MINGW32__
#include <excpt.h>

#define __mingw_try(suffix, pHandler) \
    __asm__ __volatile__ ( \
    ".l_trybegin_" suffix ":\n" \
    ".seh_handler __C_specific_handler, @except\n" \
    ".seh_handlerdata\n" \
    ".long 1\n" \
    ".rva .l_trybegin_" suffix ", .l_tryend_" suffix ", " __MINGW64_STRINGIFY(__MINGW_USYMBOL(pHandler)) " , .l_catch_" suffix "\n" \
    ".text\n" \
    );
#define __mingw_except_begin(suffix) \
    asm __volatile__ ("nop\n" \
    ".l_tryend_" suffix ":\n" \
    "jmp .l_finally_" suffix "\n" \
    ".l_catch_" suffix ":\n");

#define __mingw_except_end(suffix) \
    asm __volatile__ (".l_finally_" suffix ": nop\n");

#define seh_try(label) __mingw_try(label, SEH_EXC_HANDLER)
#define seh_catch(label) __mingw_except_begin(label)
#define seh_finally(label) __mingw_except_end(label)
#elif defined(_MSC_VER)
#define seh_try(label) __try
#define seh_catch(label) __except (isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
#define seh_finally(label) 
#else
#define seh_try(label)
#define seh_catch(label)
#define seh_finally(label)
#endif