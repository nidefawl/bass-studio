#pragma once
//mostly for debugging purposes

#define __mingw_try(suffix, pHandler) \
    __asm__ __volatile__ (".l_startw_" suffix ":\n" \
    ".seh_handler __C_specific_handler, @except\n" \
    ".seh_handlerdata\n" \
    ".long 1\n" \
    ".rva .l_startw_" suffix ", .l_endw_" suffix ", " __MINGW64_STRINGIFY(__MINGW_USYMBOL(pHandler)) " , .l_exchandler_" suffix "\n" \
    ".text\n" \
    );
#define __mingw_except_begin(suffix) \
    asm ("nop\n" \
    ".l_endw_" suffix ":\n" \
    "jmp .l_excend_" suffix "\n" \
    ".l_exchandler_" suffix ":\n");

#define __mingw_except_end(suffix) \
    asm (".l_excend_" suffix ": nop\n");
