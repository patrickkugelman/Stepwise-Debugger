#include <stepwise/base/number_parsing.hpp>

#include <charconv>
#include <format>

namespace stepwise {

bool has_hex_prefix(std::string_view text) noexcept {
    return text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
}

Result<std::uint64_t> parse_digits(std::string_view digits, std::string_view token, int base) {
    std::uint64_t value = 0;
    const char* const end = digits.data() + digits.size();
    const auto [stop, error] = std::from_chars(digits.data(), end, value, base);
    if (digits.empty() || error != std::errc{} || stop != end) {
        return Error{ErrorCode::InvalidArgument, std::format("invalid number '{}'", token)};
    }
    return value;
}

Result<std::uint64_t> parse_unsigned(std::string_view text) {
    if (has_hex_prefix(text)) {
        return parse_digits(text.substr(2), text, 16);
    }
    return parse_digits(text, text, 10);
}

}  // namespace stepwise
