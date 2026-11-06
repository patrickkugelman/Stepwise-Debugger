#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <stepwise/base/result.hpp>

namespace stepwise::symbols {

struct LineRow {
    std::uint64_t address{};
    std::uint32_t file{};
    std::uint32_t line{};
    bool is_statement{};
    bool end_sequence{};
};

struct LineTable {
    std::vector<std::string> files;
    // sorted by address; end_sequence rows go first at equal addresses,
    // so the last row at an address is the one that counts
    std::vector<LineRow> rows;
};

struct DebugLineSections {
    std::span<const std::byte> line;
    std::span<const std::byte> line_str;
    std::span<const std::byte> str;
};

Result<LineTable> parse_debug_line(const DebugLineSections& sections);

}  // namespace stepwise::symbols
