// Copyright (c) 2015 midnightBITS
// Changes to the original code are licensed under MIT license (see LICENSE for
// details), original code is licensed as follows:

/*
 * Copyright 2001-2004 Unicode, Inc.
 *
 * Disclaimer
 *
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 *
 * Limitations on Rights to Redistribute This Code
 *
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

    Conversions between UTF32, UTF-16, and UTF-8. Source code file.
    Author: Mark E. Davis, 1994.
    Rev History: Rick McGowan, fixes & updates May 2001.
    Sept 2001: fixed const & error conditions per
        mods suggested by S. Parent & A. Lillich.
    June 2002: Tim Dodd added detection and handling of incomplete
        source sequences, enhanced error detection, added casts
        to eliminate compiler warnings.
    July 2003: slight mods to back out aggressive FFFE detection.
    Jan 2004: updated switches in from-UTF8 conversions.
    Oct 2004: updated to use UNI_MAX_LEGAL_UTF32 in UTF-32 conversions.

    See the header file "ConvertUTF.h" for complete documentation.

------------------------------------------------------------------------ */

#include <cstdint>
#include <iterator>
#include <utfconv/utf.hpp>

namespace utf {
	using std::uint8_t;
	/*
	 * Index into the table below with the first byte of a UTF-8 sequence to
	 * get the number of trailing bytes that are supposed to follow it.
	 * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is
	 * left as-is for anyone who may want to do such conversion, which was
	 * allowed in earlier algorithms.
	 */
	static constexpr const uint8_t trailingBytesForUTF8[256] = {
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 00
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 10
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 20
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 30
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 40
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 50
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 60
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 70
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 80
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 90
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // A0
	    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // B0
	    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // C0
	    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // D0
	    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // E0
	    3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5   // F0
	};

	/*
	 * Magic values subtracted from a buffer value during UTF8 conversion.
	 * This table contains as many values as there might be trailing bytes
	 * in a UTF-8 sequence.
	 */
	static constexpr const char32_t offsetsFromUTF8[6] = {
	    0x00000000UL, 0x00003080UL, 0x000E2080UL,
	    0x03C82080UL, 0xFA082080UL, 0x82082080UL};

	/*
	 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
	 * into the first byte, depending on how many bytes follow.  There are
	 * as many entries in this table as there are UTF-8 sequence types.
	 * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
	 * for *legal* UTF-8 will be 4 or fewer bytes total.
	 */
	static constexpr const uint8_t firstByteMark[7] = {0x00, 0x00, 0xC0, 0xE0,
	                                                   0xF0, 0xF8, 0xFC};

	enum : char16_t {
		UNI_SUR_HIGH_START = 0xD800,
		UNI_SUR_HIGH_END = 0xDBFF,
		UNI_SUR_LOW_START = 0xDC00,
		UNI_SUR_LOW_END = 0xDFFF,
	};

	enum : char32_t {
		UNI_REPLACEMENT_CHAR = 0x0000FFFD,
		UNI_MAX_BMP = 0x0000FFFF,
		UNI_MAX_UTF16 = 0x0010FFFF,
		UNI_MAX_LEGAL_UTF32 = 0x0010FFFF
	};

	// used for shifting by 10 bits
	static constexpr const unsigned halfShift = 10;

	static constexpr const char32_t halfBase = 0x0001'0000UL;
	static constexpr const char32_t halfMask = 0x3FFUL;
	static constexpr const char32_t byteMask = 0xBF;
	static constexpr const char32_t byteMark = 0x80;

	template <typename T>
	static bool isLegalUTF8(T source, int length) {
		uint8_t a;
		auto srcptr = source + length;
		switch (length) {
			default:
				return false;
				/* Everything else falls through when "true"... */
			case 4:
				if ((a = static_cast<uint8_t>(*--srcptr)) < 0x80 || a > 0xBF)
					return false;
				[[fallthrough]];
			case 3:
				if ((a = static_cast<uint8_t>(*--srcptr)) < 0x80 || a > 0xBF)
					return false;
				[[fallthrough]];
			case 2:
				if ((a = static_cast<uint8_t>(*--srcptr)) < 0x80 || a > 0xBF)
					return false;

				switch (static_cast<uint8_t>(*source)) {
						/* no fall-through in this inner switch */
					case 0xE0:
						if (a < 0xA0) return false;
						break;
					case 0xED:
						if (a > 0x9F) return false;
						break;
					case 0xF0:
						if (a < 0x90) return false;
						break;
					case 0xF4:
						if (a > 0x8F) return false;
						break;
					default:
						if (a < 0x80) return false;
				}
				[[fallthrough]];

			case 1:
				if (static_cast<uint8_t>(*source) >= 0x80 &&
				    static_cast<uint8_t>(*source) < 0xC2)
					return false;
		}
		if (static_cast<uint8_t>(*source) > 0xF4) return false;
		return true;
	}

	using str8_it = std::string_view::const_iterator;
	static inline char32_t decode(str8_it& source,
	                              str8_it sourceEnd,
	                              bool& ok) {
		ok = false;
		auto const uchar = static_cast<uint8_t>(*source);
		auto extraBytesToRead = trailingBytesForUTF8[uchar];
		if (extraBytesToRead >= sourceEnd - source) return 0;

		if (!isLegalUTF8(source, extraBytesToRead + 1)) return 0;

		char32_t ch = 0;
		/*
		 * The cases all fall through.
		 */
		switch (extraBytesToRead) {
			// 4 and 5 extra bytes are not passed through by isLegalUTF8
			case 3:
				ch += static_cast<uint8_t>(*source++);
				ch <<= 6;
				[[fallthrough]];
			case 2:
				ch += static_cast<uint8_t>(*source++);
				ch <<= 6;
				[[fallthrough]];
			case 1:
				ch += static_cast<uint8_t>(*source++);
				ch <<= 6;
				[[fallthrough]];
			case 0:
				ch += static_cast<uint8_t>(*source++);
		}
		ch -= offsetsFromUTF8[extraBytesToRead];
		ok = true;
		return ch;
	}

#ifdef __cpp_lib_char8_t
	using utf8_it = std::u8string_view::const_iterator;
	static inline char32_t decode(utf8_it& source,
	                              utf8_it sourceEnd,
	                              bool& ok) {
		ok = false;
		auto const uchar = static_cast<uint8_t>(*source);
		auto extraBytesToRead = trailingBytesForUTF8[uchar];
		if (extraBytesToRead >= sourceEnd - source) return 0;

		if (!isLegalUTF8(source, extraBytesToRead + 1)) return 0;

		char32_t ch = 0;
		/*
		 * The cases all fall through.
		 */
		switch (extraBytesToRead) {
			// 4 and 5 extra bytes are not passed through by isLegalUTF8
			case 3:
				ch += static_cast<uint8_t>(*source++);
				ch <<= 6;
				[[fallthrough]];
			case 2:
				ch += static_cast<uint8_t>(*source++);
				ch <<= 6;
				[[fallthrough]];
			case 1:
				ch += static_cast<uint8_t>(*source++);
				ch <<= 6;
				[[fallthrough]];
			case 0:
				ch += static_cast<uint8_t>(*source++);
		}
		ch -= offsetsFromUTF8[extraBytesToRead];
		ok = true;
		return ch;
	}
#endif  // __cpp_lib_char8_t

	using utf16_it = std::u16string_view::const_iterator;
	static inline char32_t decode(utf16_it& source,
	                              utf16_it sourceEnd,
	                              bool& ok) {
		ok = true;
		char32_t ch = *source++;
		/* If we have a surrogate pair, convert to char32_t first. */
		if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
			ok = false;
			/* If the 16 bits following the high surrogate are in the source
			 * buffer... */
			if (source < sourceEnd) {
				char32_t trail = *source;
				/* If it's a low surrogate, convert to char32_t. */
				if (trail >= UNI_SUR_LOW_START && trail <= UNI_SUR_LOW_END) {
					ch = ((ch - UNI_SUR_HIGH_START) << halfShift) + trail -
					     UNI_SUR_LOW_START + halfBase;
					++source;
					ok = true;
				}
			}
		}

		return ch;
	}

	using utf32_it = std::u32string_view::const_iterator;
	static inline char32_t decode(utf32_it& source,
	                              utf32_it /* sourceEnd */,
	                              bool& ok) {
		ok = true;
		return *source++;
	}

	static inline void encode(char32_t ch,
	                          std::back_insert_iterator<std::string>& target) {
		unsigned short bytesToWrite = 0;

		/* Figure out how many bytes the result will require */
		if (ch < 0x80u)
			bytesToWrite = 1;
		else if (ch < 0x800u)
			bytesToWrite = 2;
		else if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
			bytesToWrite = 3;
			ch = UNI_REPLACEMENT_CHAR;
		} else if (ch < 0x10000u)
			bytesToWrite = 3;
		else if (ch <= UNI_MAX_LEGAL_UTF32)
			bytesToWrite = 4;
		else {
			bytesToWrite = 3;
			ch = UNI_REPLACEMENT_CHAR;
		}

		uint8_t mid[4];
		uint8_t* midp = mid + sizeof(mid);
		switch (bytesToWrite) { /* note: everything falls through. */
			case 4:
				*--midp = static_cast<uint8_t>((ch | byteMark) & byteMask);
				ch >>= 6;
				[[fallthrough]];
			case 3:
				*--midp = static_cast<uint8_t>((ch | byteMark) & byteMask);
				ch >>= 6;
				[[fallthrough]];
			case 2:
				*--midp = static_cast<uint8_t>((ch | byteMark) & byteMask);
				ch >>= 6;
				[[fallthrough]];
			case 1:
				*--midp =
				    static_cast<uint8_t>(ch | firstByteMark[bytesToWrite]);
		}
		for (int i = 0; i < bytesToWrite; ++i)
			*target++ = static_cast<char>(*midp++);
	}

#ifdef __cpp_lib_char8_t
	static inline void encode(
	    char32_t ch,
	    std::back_insert_iterator<std::u8string>& target) {
		unsigned short bytesToWrite = 0;

		/* Figure out how many bytes the result will require */
		if (ch < 0x80u)
			bytesToWrite = 1;
		else if (ch < 0x800u)
			bytesToWrite = 2;
		else if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
			bytesToWrite = 3;
			ch = UNI_REPLACEMENT_CHAR;
		} else if (ch < 0x10000u)
			bytesToWrite = 3;
		else if (ch <= UNI_MAX_LEGAL_UTF32)
			bytesToWrite = 4;
		else {
			bytesToWrite = 3;
			ch = UNI_REPLACEMENT_CHAR;
		}

		uint8_t mid[4];
		uint8_t* midp = mid + sizeof(mid);
		switch (bytesToWrite) { /* note: everything falls through. */
			case 4:
				*--midp = static_cast<uint8_t>((ch | byteMark) & byteMask);
				ch >>= 6;
				[[fallthrough]];
			case 3:
				*--midp = static_cast<uint8_t>((ch | byteMark) & byteMask);
				ch >>= 6;
				[[fallthrough]];
			case 2:
				*--midp = static_cast<uint8_t>((ch | byteMark) & byteMask);
				ch >>= 6;
				[[fallthrough]];
			case 1:
				*--midp =
				    static_cast<uint8_t>(ch | firstByteMark[bytesToWrite]);
		}
		for (int i = 0; i < bytesToWrite; ++i)
			*target++ = static_cast<char8_t>(*midp++);
	}
#endif  // __cpp_lib_char8_t

	static inline void encode(
	    char32_t ch,
	    std::back_insert_iterator<std::u16string>& target) {
		if (ch <= UNI_MAX_BMP) {
			/* UTF-16 surrogate values are illegal in UTF-32 */
			if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
				*target++ = UNI_REPLACEMENT_CHAR;
				return;
			}
			*target++ = static_cast<char16_t>(ch); /* normal case */
			return;
		}

		if (ch > UNI_MAX_UTF16) {
			*target++ = UNI_REPLACEMENT_CHAR;
			return;
		}

		ch -= halfBase;
		*target++ =
		    static_cast<char16_t>((ch >> halfShift) + UNI_SUR_HIGH_START);
		*target++ = static_cast<char16_t>((ch & halfMask) + UNI_SUR_LOW_START);
	}

	static inline void encode(
	    char32_t ch,
	    std::back_insert_iterator<std::u32string>& target) {
		if constexpr (false) {
			// This code will only have sense with some sort of UTF32-to-UTF32
			// validation
			if (ch <= UNI_MAX_LEGAL_UTF32) {
				/*
				 * UTF-16 surrogate values are illegal in UTF-32, and anything
				 * over Plane 17 (> 0x10FFFF) is illegal.
				 */
				if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
					*target++ = UNI_REPLACEMENT_CHAR;
					return;
				}
				*target++ = ch; /* normal case */
				return;
			}
			*target++ = UNI_REPLACEMENT_CHAR;
		} else
			*target++ = ch; /* normal case */
	}

	template <class StringView>
	static inline bool is_valid_impl(StringView src) {
		auto source = src.begin();
		auto sourceEnd = src.end();

		while (source < sourceEnd) {
			bool ok = false;
			[[maybe_unused]] char32_t ch = decode(source, sourceEnd, ok);
			if (!ok) return false;
		}

		return true;
	}

	template <class String, class StringView>
	static inline String convert(StringView src) {
		String out;
		auto source = src.begin();
		auto sourceEnd = src.end();
		auto target = std::back_inserter(out);

		while (source < sourceEnd) {
			bool ok = false;
			char32_t ch = decode(source, sourceEnd, ok);
			if (!ok) return {};

			encode(ch, target);
		}

		return out;
	}

	bool is_valid(std::string_view src) { return is_valid_impl(src); }
	bool is_valid(std::u16string_view src) { return is_valid_impl(src); }
	bool is_valid(std::u32string_view) { return true; }

	std::u16string as_u16(std::string_view src) {
		return convert<std::u16string>(src);
	}

	std::u32string as_u32(std::string_view src) {
		return convert<std::u32string>(src);
	}

	std::string as_str8(std::u16string_view src) {
		return convert<std::string>(src);
	}

	std::u32string as_u32(std::u16string_view src) {
		return convert<std::u32string>(src);
	}

	std::string as_str8(std::u32string_view src) {
		return convert<std::string>(src);
	}

	std::u16string as_u16(std::u32string_view src) {
		return convert<std::u16string>(src);
	}

#ifdef __cpp_lib_char8_t
	bool is_valid(std::u8string_view src) { return is_valid_impl(src); }

	template <typename CharOut, typename CharIn>
	std::basic_string<CharOut> char_conv(std::basic_string_view<CharIn> src) {
		static_assert(
		    sizeof(CharOut) == sizeof(CharIn),
		    "This function only works for strings of same-sized characters.");
		return {reinterpret_cast<CharOut const*>(src.data()), src.size()};
	}

	std::string as_str8(std::u8string_view src) { return char_conv<char>(src); }

	std::u16string as_u16(std::u8string_view src) {
		return convert<std::u16string>(src);
	}

	std::u32string as_u32(std::u8string_view src) {
		return convert<std::u32string>(src);
	}

	std::u8string as_u8(std::u16string_view src) {
		return convert<std::u8string>(src);
	}

	std::u8string as_u8(std::u32string_view src) {
		return convert<std::u8string>(src);
	}

	std::u8string as_u8(std::string_view src) {
		return char_conv<char8_t>(src);
	}
#endif  // __cpp_lib_char8_t
}  // namespace utf
