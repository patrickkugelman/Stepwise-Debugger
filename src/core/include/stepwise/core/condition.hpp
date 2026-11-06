#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <stepwise/backend/registers.hpp>
#include <stepwise/base/result.hpp>

namespace stepwise::core {

enum class CompareOp { Equal, NotEqual, Less, LessOrEqual, Greater, GreaterOrEqual };

struct Condition {
    std::string text;
    std::uint64_t backend::Registers::*reg;
    CompareOp op;
    std::uint64_t value;

    [[nodiscard]] bool holds(const backend::Registers& registers) const noexcept;
};

Result<Condition> parse_condition(std::string_view text);

}  // namespace stepwise::core
