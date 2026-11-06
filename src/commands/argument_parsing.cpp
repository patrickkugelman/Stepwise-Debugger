#include "argument_parsing.hpp"

#include <format>
#include <limits>

namespace stepwise::commands {

Result<Address> parse_address(std::string_view text) {
    auto value = parse_unsigned(text);
    if (!value) {
        return value.error();
    }
    return Address{value.value()};
}

Result<BreakpointId> parse_breakpoint_id(std::string_view text) {
    auto value = parse_unsigned(text);
    if (!value) {
        return value.error();
    }
    if (value.value() > std::numeric_limits<std::uint32_t>::max()) {
        return Error{ErrorCode::InvalidArgument, std::format("invalid breakpoint number '{}'", text)};
    }
    return BreakpointId{static_cast<std::uint32_t>(value.value())};
}

Result<std::byte> parse_hex_byte(std::string_view text) {
    const std::string_view digits = has_hex_prefix(text) ? text.substr(2) : text;
    auto value = parse_digits(digits, text, 16);
    if (!value) {
        return value.error();
    }
    if (value.value() > 0xff) {
        return Error{ErrorCode::InvalidArgument, std::format("'{}' does not fit in one byte", text)};
    }
    return static_cast<std::byte>(value.value());
}

Result<core::LocationSpec> parse_location(std::string_view text) {
    if (text.empty()) {
        return Error{ErrorCode::InvalidArgument, "empty location"};
    }
    if (text.front() >= '0' && text.front() <= '9') {
        auto address = parse_address(text);
        if (!address) {
            return address.error();
        }
        return core::LocationSpec{address.value()};
    }
    if (const std::size_t colon = text.rfind(':'); colon != std::string_view::npos) {
        const std::string_view file = text.substr(0, colon);
        auto line = parse_unsigned(text.substr(colon + 1));
        if (file.empty() || !line || line.value() == 0 ||
            line.value() > std::numeric_limits<std::uint32_t>::max()) {
            return Error{ErrorCode::InvalidArgument,
                         std::format("invalid location '{}', expected file:line", text)};
        }
        return core::LocationSpec{
            core::FileLineSpec{std::string{file}, static_cast<std::uint32_t>(line.value())}};
    }
    return core::LocationSpec{core::FunctionSpec{std::string{text}}};
}

Error usage_error(const std::string& usage) {
    return Error{ErrorCode::InvalidArgument, std::format("usage: {}", usage)};
}

}  // namespace stepwise::commands
