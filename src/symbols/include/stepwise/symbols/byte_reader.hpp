#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <stepwise/base/result.hpp>

namespace stepwise::symbols {

class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> data) noexcept : data_(data) {}

    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - position_; }
    [[nodiscard]] bool at_end() const noexcept { return position_ == data_.size(); }

    Result<void> seek(std::uint64_t position);
    Result<void> skip(std::uint64_t count);
    Result<std::span<const std::byte>> bytes(std::uint64_t count);
    Result<ByteReader> sub(std::uint64_t count);

    Result<std::uint8_t> u8();
    Result<std::int8_t> i8();
    Result<std::uint16_t> u16();
    Result<std::uint32_t> u32();
    Result<std::uint64_t> u64();
    Result<std::uint64_t> unsigned_of_size(unsigned size);

    Result<std::uint64_t> uleb128();
    Result<std::int64_t> sleb128();

    Result<std::string_view> cstring();

private:
    template <class T>
    Result<T> read_integer();

    std::span<const std::byte> data_;
    std::size_t position_{0};
};

}  // namespace stepwise::symbols
