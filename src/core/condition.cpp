#include <stepwise/core/condition.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <iterator>

#include <stepwise/base/number_parsing.hpp>

namespace stepwise::core {
namespace {

Error bad_condition(std::string_view text) {
    return Error{ErrorCode::InvalidArgument,
                 std::format("invalid condition '{}', expected <register><op><number> such as rax==5; "
                             "operators: == != < <= > >=",
                             text)};
}

}  // namespace

bool Condition::holds(const backend::Registers& registers) const noexcept {
    const std::uint64_t actual = registers.*reg;
    switch (op) {
        case CompareOp::Equal:
            return actual == value;
        case CompareOp::NotEqual:
            return actual != value;
        case CompareOp::Less:
            return actual < value;
        case CompareOp::LessOrEqual:
            return actual <= value;
        case CompareOp::Greater:
            return actual > value;
        case CompareOp::GreaterOrEqual:
            return actual >= value;
    }
    return false;
}

Result<Condition> parse_condition(std::string_view text) {
    std::string compact;
    std::ranges::copy_if(text, std::back_inserter(compact),
                         [](char c) { return std::isspace(static_cast<unsigned char>(c)) == 0; });

    const std::size_t op_begin = compact.find_first_of("=!<>");
    if (op_begin == std::string::npos || op_begin == 0) {
        return bad_condition(text);
    }
    const bool two_chars = op_begin + 1 < compact.size() && compact[op_begin + 1] == '=';
    const std::string_view op_text{compact.data() + op_begin, two_chars ? 2U : 1U};

    CompareOp op{};
    if (op_text == "==") {
        op = CompareOp::Equal;
    } else if (op_text == "!=") {
        op = CompareOp::NotEqual;
    } else if (op_text == "<=") {
        op = CompareOp::LessOrEqual;
    } else if (op_text == ">=") {
        op = CompareOp::GreaterOrEqual;
    } else if (op_text == "<") {
        op = CompareOp::Less;
    } else if (op_text == ">") {
        op = CompareOp::Greater;
    } else {
        return bad_condition(text);
    }

    const std::string_view name{compact.data(), op_begin};
    const auto field = std::ranges::find_if(backend::register_fields,
                                            [name](const backend::RegisterField& f) { return f.name == name; });
    if (field == backend::register_fields.end()) {
        return Error{ErrorCode::InvalidArgument, std::format("unknown register '{}'", name)};
    }

    const std::string_view operand = std::string_view{compact}.substr(op_begin + op_text.size());
    if (operand.empty()) {
        return bad_condition(text);
    }
    auto value = parse_unsigned(operand);
    if (!value) {
        return value.error();
    }
    return Condition{compact, field->member, op, value.value()};
}

}  // namespace stepwise::core
