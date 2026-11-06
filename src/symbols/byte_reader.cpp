#include <stepwise/symbols/byte_reader.hpp>

#include <algorithm>

namespace stepwise::symbols {
namespace {

Error truncated() {
    return Error{ErrorCode::Protocol, "unexpected end of data"};
}

}  // namespace

Result<void> ByteReader::seek(std::uint64_t position) {
    if (position > data_.size()) {
        return truncated();
    }
    position_ = static_cast<std::size_t>(position);
    return {};
}

Result<void> ByteReader::skip(std::uint64_t count) {
    if (count > remaining()) {
        return truncated();
    }
    position_ += static_cast<std::size_t>(count);
    return {};
}

Result<std::span<const std::byte>> ByteReader::bytes(std::uint64_t count) {
    if (count > remaining()) {
        return truncated();
    }
    const auto view = data_.subspan(position_, static_cast<std::size_t>(count));
    position_ += static_cast<std::size_t>(count);
    return view;
}

Result<ByteReader> ByteReader::sub(std::uint64_t count) {
    auto view = bytes(count);
    if (!view) {
        return view.error();
    }
    return ByteReader{view.value()};
}

template <class T>
Result<T> ByteReader::read_integer() {
    auto raw = bytes(sizeof(T));
    if (!raw) {
        return raw.error();
    }
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value = static_cast<T>(value | static_cast<T>(static_cast<T>(std::to_integer<unsigned>(raw.value()[i]))
                                                      << (8 * i)));
    }
    return value;
}

Result<std::uint8_t> ByteReader::u8() {
    return read_integer<std::uint8_t>();
}

Result<std::int8_t> ByteReader::i8() {
    auto value = read_integer<std::uint8_t>();
    if (!value) {
        return value.error();
    }
    return static_cast<std::int8_t>(value.value());
}

Result<std::uint16_t> ByteReader::u16() {
    return read_integer<std::uint16_t>();
}

Result<std::uint32_t> ByteReader::u32() {
    return read_integer<std::uint32_t>();
}

Result<std::uint64_t> ByteReader::u64() {
    return read_integer<std::uint64_t>();
}

Result<std::uint64_t> ByteReader::unsigned_of_size(unsigned size) {
    switch (size) {
        case 1: {
            auto value = u8();
            if (!value) {
                return value.error();
            }
            return static_cast<std::uint64_t>(value.value());
        }
        case 2: {
            auto value = u16();
            if (!value) {
                return value.error();
            }
            return static_cast<std::uint64_t>(value.value());
        }
        case 4: {
            auto value = u32();
            if (!value) {
                return value.error();
            }
            return static_cast<std::uint64_t>(value.value());
        }
        case 8:
            return u64();
        default:
            return Error{ErrorCode::InvalidArgument, "unsupported integer size"};
    }
}

Result<std::uint64_t> ByteReader::uleb128() {
    std::uint64_t result = 0;
    for (unsigned shift = 0;; shift += 7) {
        auto byte = u8();
        if (!byte) {
            return byte.error();
        }
        const std::uint64_t payload = byte.value() & 0x7fU;
        if (shift >= 64 || (shift > 57 && (payload >> (64 - shift)) != 0)) {
            return Error{ErrorCode::Protocol, "LEB128 value does not fit in 64 bits"};
        }
        result |= payload << shift;
        if ((byte.value() & 0x80U) == 0) {
            return result;
        }
    }
}

Result<std::int64_t> ByteReader::sleb128() {
    std::uint64_t result = 0;
    for (unsigned shift = 0;; shift += 7) {
        auto byte = u8();
        if (!byte) {
            return byte.error();
        }
        if (shift >= 64) {
            return Error{ErrorCode::Protocol, "LEB128 value does not fit in 64 bits"};
        }
        result |= static_cast<std::uint64_t>(byte.value() & 0x7fU) << shift;
        if ((byte.value() & 0x80U) == 0) {
            const unsigned used = shift + 7;
            if (used < 64 && (byte.value() & 0x40U) != 0) {
                result |= ~std::uint64_t{0} << used;
            }
            return static_cast<std::int64_t>(result);
        }
    }
}

Result<std::string_view> ByteReader::cstring() {
    const auto rest = data_.subspan(position_);
    const auto end = std::ranges::find(rest, std::byte{0});
    if (end == rest.end()) {
        return truncated();
    }
    const auto length = static_cast<std::size_t>(end - rest.begin());
    const std::string_view text{reinterpret_cast<const char*>(rest.data()), length};
    position_ += length + 1;
    return text;
}

}  // namespace stepwise::symbols
