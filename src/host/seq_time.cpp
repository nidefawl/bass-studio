#include "logging.h"
#include "str_util.h"
#include "seq_time.h"
#include <array>
#include <charconv>
#include <system_error>

beatbar16th_t tickToBarBeat16th(tick_t tick, uint32_t signatureNum, uint32_t signatureDenomBits, bool isRelative) {
    beatbar16th_t t{};
    tick_t tick2 = tick;
    auto denom     = 4U - math::clamp(signatureDenomBits, 0U, 4U);
    auto ticksBar = static_cast<tick_t>((1<<denom)*signatureNum * TICKS_16TH);
    if (tick2 < 0) {
        if (isRelative) {
            tick2 = -tick2;
        } else {
            tick2 = (ticksBar + (tick2 % ticksBar)) % ticksBar;
        }
    }
    int32_t sixth = tick2 / TICKS_16TH;
    t.subticks = tick2 % TICKS_16TH;
    t.th = sixth & ((1 << denom) - 1);
    auto quarters = (sixth >> denom);
    t.beat = static_cast<int32_t>(quarters % signatureNum);
    t.bar = static_cast<int32_t>(quarters / signatureNum);
    if (tick < 0) {
        if (isRelative) {
            t.bar *= -1;
            t.bar--;
        } else {
            t.bar+= -(-tick+ticksBar-1) / ticksBar;
        }
    }
    return t;
}

tick_t beatBarNthToTick(const beatbar16th_t& beatBarNth, uint32_t signatureNum, uint32_t signatureDenomBits, bool isRelative) {
    auto denom     = 4U - math::clamp(signatureDenomBits, 0U, 4U);
    auto num          = static_cast<int32_t>(signatureNum);
    int32_t th        = beatBarNth.th;
    int32_t bar       = beatBarNth.bar;
    int32_t beat      = beatBarNth.beat;
    if (beatBarNth.bar < 0 && isRelative) {
        auto totalBeats = -(bar+1) * num + beat;
        auto beatAsTh = totalBeats * (1 << denom);
        return -1 * ((beatAsTh + th) * TICKS_16TH + beatBarNth.subticks);
    }
    auto totalBeats = bar * num + beat;
    auto beatAsTh = totalBeats * (1 << denom);
    return (beatAsTh + th) * TICKS_16TH + beatBarNth.subticks;
}

String tickAsBeatString(tick_t tick, bool isRelative) {
    return beatBarNthToString(tickToBarBeat16th(tick, 4, 2, isRelative), isRelative);
}
String beatBarNthToString(const beatbar16th_t& beatBarNth, bool isRelative) {
    log_lf(Log::L_DEBUG, "raw: %d.%d.%d.%d\n", beatBarNth.bar, beatBarNth.beat, beatBarNth.th, beatBarNth.subticks);
    std::array<char, 32> FormatBuffer{};
    constexpr const char format[]       = "%s%d.%d.%d.%d";
    beatbar16th_t cpy = beatBarNth;
    for (size_t i = 0; i <3; i++) {
        if (cpy[i] < 0 == isRelative) // increment negatives for relative, increment positives for absolute
            cpy[i]++;
    }
    const char* prefix = isRelative && beatBarNth.bar == -1 ? "-" : "";
#ifdef __APPLE__
    int ret = snprintf(FormatBuffer.data(), FormatBuffer.size(), format, prefix, cpy.bar, cpy.beat, cpy.th, cpy.subticks);
#else
    int ret = _snprintf_s(FormatBuffer.data(), FormatBuffer.size(), _TRUNCATE, format, prefix, cpy.bar, cpy.beat, cpy.th, cpy.subticks);
#endif
    if (ret < 0) return {};
    return String{FormatBuffer.data(), static_cast<String::size_type>(ret)};
}

beatbar16th_t stringToBeatBarNth(const String& str, bool isRelative, uint32_t signatureNum, uint32_t signatureDenomBits) {
    size_t strPosStart = 0;
    beatbar16th_t beatBarNth{};
    size_t idx = 0;
    bool isNegative = false;
    // consume the first minus sign
    size_t strPosMinus = str.find('-', strPosStart);
    if (strPosMinus != String::npos) {
        strPosStart = strPosMinus + 1;
        isNegative = true;
    }
    while (strPosStart < str.length() && idx < 4) {
        size_t strPosDot = str.find('.', strPosStart);
        if (strPosDot == String::npos) {
            strPosDot = str.length();
        }
        int32_t result{};
        auto [ptr, ec] { std::from_chars(&str[strPosStart], &str[strPosDot], result, 10) };
        strPosStart = strPosDot + 1;
        if (ec == std::errc::invalid_argument) {
            log_lf(Log::L_ERROR, "stringToBeatBarNth: invalid argument\n");
        } else if (ec == std::errc::result_out_of_range) {
            log_lf(Log::L_ERROR, "stringToBeatBarNth: result_out_of_range\n");
        } else {
            if (idx < 3) {
                beatBarNth[idx] = result;
            } else {
                beatBarNth.subticks = result;
            }
            idx++;
        }
    }
    bool allZeros = true;
    for (size_t i = 0; i < 4; i++) {
        if (beatBarNth[i] != 0) {
            allZeros = false;
            break;
        }
    }
    if (!(isRelative && isNegative && allZeros)) {
        for (size_t i = 0; i < 3; i++) {
            auto result = beatBarNth[i];
            if (isNegative) {
                result = -result;
            }
            if (isNegative && isRelative) {
                result--;
            }
            if ((result > 0 || (result==0&&isNegative)) && !isRelative) {
                result--;
            }
            isNegative = false;
            beatBarNth[i] = result;
        }
    }
    return beatBarNth;
}
