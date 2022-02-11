#pragma once
#include <Windows.h>
#include <vector>
#include "str_util.h"

#ifndef USE_WSTRING
uint32_t wcharToSring(uint32_t codepage, const wchar_t *utf16, size_t utf16_len, std::vector<char>& converted);
uint32_t stringToWchar(uint32_t codepage, const char* mbsz, size_t mbsz_len, std::vector<wchar_t>& converted);
#endif
