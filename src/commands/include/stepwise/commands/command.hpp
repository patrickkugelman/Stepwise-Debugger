#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <stepwise/backend/registers.hpp>
#include <stepwise/base/result.hpp>
#include <stepwise/core/session.hpp>

namespace stepwise::commands {

class CommandRegistry;

enum class ArgumentKind {
    Word,
    Location,
    CommandName,
};

struct ArgumentSlot {
    ArgumentKind kind;
    std::vector<std::string> words;
};

struct CommandInfo {
    std::string name;
    std::vector<std::string> aliases;
    std::string usage;
    std::string help;
    std::vector<ArgumentSlot> arguments{};
};

struct NoOutput {};
struct QuitOutput {};

struct RegistersOutput {
    backend::Registers registers;
};

struct HelpOutput {
    std::vector<CommandInfo> commands;
    bool detailed;
};

struct MemoryOutput {
    Address address;
    std::vector<std::byte> bytes;
};

struct BreakpointSetOutput {
    core::Breakpoint breakpoint;
    core::CodeLocation where;
};

struct BreakpointRow {
    core::Breakpoint breakpoint;
    core::CodeLocation where;
};

struct BreakpointListOutput {
    std::vector<BreakpointRow> rows;
};

struct BacktraceOutput {
    std::vector<core::Frame> frames;
};

struct ListOutput {
    std::string file;
    std::uint32_t first_line;
    std::vector<std::string> lines;
};

struct InfoLineOutput {
    core::CodeLocation where;
    std::optional<symbols::LineInfo> line;
    std::optional<core::CodeLocation> start;
    std::optional<core::CodeLocation> end;
    std::optional<std::uint32_t> requested_line;
};

using CommandOutput = std::variant<NoOutput, QuitOutput, RegistersOutput, HelpOutput, MemoryOutput,
                                   BreakpointSetOutput, BreakpointListOutput, BacktraceOutput,
                                   ListOutput, InfoLineOutput>;

struct CommandContext {
    core::Session& session;
    const CommandRegistry& registry;
};

using CommandArgs = std::span<const std::string_view>;

class Command {
public:
    virtual ~Command() = default;

    virtual Result<CommandOutput> execute(CommandContext& context) = 0;
};

using CommandPtr = std::unique_ptr<Command>;

template <class C>
Result<CommandPtr> parse_no_args(CommandArgs args) {
    if (!args.empty()) {
        return Error{ErrorCode::InvalidArgument, std::format("usage: {}", C::info().usage)};
    }
    return std::make_unique<C>();
}

inline Result<CommandOutput> no_output(Result<void> outcome) {
    if (!outcome) {
        return outcome.error();
    }
    return NoOutput{};
}

}  // namespace stepwise::commands
