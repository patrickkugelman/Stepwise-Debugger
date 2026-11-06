#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <stepwise/base/number_parsing.hpp>
#include <stepwise/base/result.hpp>
#include <stepwise/base/types.hpp>
#include <stepwise/core/location.hpp>

namespace stepwise::commands {

Result<Address> parse_address(std::string_view text);
Result<BreakpointId> parse_breakpoint_id(std::string_view text);

Result<std::byte> parse_hex_byte(std::string_view text);

Result<core::LocationSpec> parse_location(std::string_view text);

Error usage_error(const std::string& usage);

}  // namespace stepwise::commands
