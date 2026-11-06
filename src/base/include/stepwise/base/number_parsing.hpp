#pragma once

#include <cstdint>
#include <string_view>

#include <stepwise/base/result.hpp>

namespace stepwise {

[[nodiscard]] bool has_hex_prefix(std::string_view text) noexcept;

Result<std::uint64_t> parse_digits(std::string_view digits, std::string_view token, int base);

Result<std::uint64_t> parse_unsigned(std::string_view text);

}  // namespace stepwise
