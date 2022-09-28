#pragma once
#include <cstddef>
#include "types.h"
#include "math/seq_math.h"
#include "str_util.h"

namespace DAW::ByteBuffer {
    using size_t = std::size_t;
    template<typename T>
    struct stream_write {
        T& stream;
        size_t pos;
        template<typename D>
        void write(const D& input) {
            if (stream.size() < pos + sizeof(D)) {
                stream.resize(stream.size() + math::max<size_t>(sizeof(D), 128));
            }
            const auto* pInput = reinterpret_cast<const std::byte*>(&input);
            std::memcpy(stream.data() + pos, pInput, sizeof(D));
            pos += sizeof(D);
        }
        void writeString(const String& str) {
            auto sizeStr = static_cast<int32_t>(str.length());
            write<int32_t>(sizeStr);
            if (stream.size() < pos + sizeStr) {
                stream.resize(stream.size() + math::max<size_t>(sizeStr, 128));
            }
            const auto* pInput = reinterpret_cast<const std::byte*>(str.data());
            std::memcpy(stream.data() + pos, pInput, sizeStr);
            pos += sizeStr;
        }
        void setPos(size_t p) {
            pos = p;
        }
    };

    struct stream_read {
        const std::byte* const dataBegin;
        const size_t size;
        const std::byte* data;
        size_t pos = 0;
        template<typename T>
        explicit stream_read(const T& vec) 
            : dataBegin(vec.data()),
            size(vec.size()),
            data(vec.data())
        { }
        template<typename T>
        bool read(T& out) {
            if (pos + sizeof(T) > size) return false;
            T tmp{};
            std::memcpy(&tmp, &data[pos], sizeof(T));
            pos += sizeof(T);
            out = tmp;
            return true;
        }
        bool readString(String& out) {
            int32_t stringSize = 0;
            if (!read(stringSize)) return false;
            if (pos + stringSize > size) return false;
            String str;
            str.resize(stringSize);
            std::memcpy(str.data(), &data[pos], stringSize);
            out = std::move(str);
            pos += stringSize;
            return true;
        }
    };
}