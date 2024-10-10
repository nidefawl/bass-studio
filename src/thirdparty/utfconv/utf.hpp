// Copyright (c) 2015 midnightBITS
// This code is licensed under MIT license (see LICENSE for details)

#pragma once
#include <string>
#include <string_view>

namespace utf {
	bool is_valid(std::string_view src);
	bool is_valid(std::u16string_view src);
	bool is_valid(std::u32string_view src);

	std::u16string as_u16(std::string_view src);
	std::u32string as_u32(std::string_view src);
	std::string as_str8(std::u16string_view src);
	std::u32string as_u32(std::u16string_view src);
	std::u16string as_u16(std::u32string_view src);
	std::string as_str8(std::u32string_view src);

#ifdef __cpp_lib_char8_t
	bool is_valid(std::u8string_view src);

	std::string as_str8(std::u8string_view src);
	std::u16string as_u16(std::u8string_view src);
	std::u32string as_u32(std::u8string_view src);
	std::u8string as_u8(std::u16string_view src);
	std::u8string as_u8(std::u32string_view src);
	std::u8string as_u8(std::string_view src);
#endif
}  // namespace utf
