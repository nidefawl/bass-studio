#pragma once
#include <windows.h>
#include <vector>
#include "str_util.hpp"

#ifndef USE_WSTRING
uint32_t wcharToSring(uint32_t codepage, const wchar_t *utf16, size_t utf16_len, std::vector<char>& converted);
uint32_t stringToWchar(uint32_t codepage, const char* mbsz, size_t mbsz_len, std::vector<wchar_t>& converted);
std::basic_string<wchar_t> StringU8ToW(String const& s);
String StringWToU8(std::basic_string<wchar_t> const& s);
#endif
