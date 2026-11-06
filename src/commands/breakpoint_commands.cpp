#include <optional>
#include <string>
#include <utility>

#include "argument_parsing.hpp"
#include "command_groups.hpp"

namespace stepwise::commands {
namespace {

class BreakCommand final : public Command {
public:
    BreakCommand(core::LocationSpec spec, std::optional<core::Condition> condition)
        : spec_(std::move(spec)), condition_(std::move(condition)) {}

    static CommandInfo info() {
        return {"break", {"b"}, "break <address | function | file:line> [if <register><op><number>]",
                "Set a breakpoint. A function stops after its prologue; a line without code "
                "moves to the next line that has some. With a condition such as rax==5 it "
                "stops only when that holds (operators: == != < <= > >=, unsigned).",
                {{ArgumentKind::Location, {}}, {ArgumentKind::Word, {"if"}}}};
    }

    static Result<CommandPtr> parse(CommandArgs args) {
        if (args.empty() || args.size() == 2 || (args.size() > 2 && args[1] != "if")) {
            return usage_error(info().usage);
        }
        auto spec = parse_location(args[0]);
        if (!spec) {
            return spec.error();
        }
        std::optional<core::Condition> condition;
        if (args.size() > 2) {
            std::string expression;
            for (std::string_view token : args.subspan(2)) {
                expression += token;
            }
            auto parsed = core::parse_condition(expression);
            if (!parsed) {
                return parsed.error();
            }
            condition = std::move(parsed.value());
        }
        return std::make_unique<BreakCommand>(std::move(spec.value()), std::move(condition));
    }

    Result<CommandOutput> execute(CommandContext& context) override {
        auto where = context.session.resolve_location(spec_, true);
        if (!where) {
            return where.error();
        }
        auto& breakpoints = context.session.breakpoints();
        auto id = breakpoints.add(where.value().address, condition_);
        if (!id) {
            return id.error();
        }
        return BreakpointSetOutput{*breakpoints.find(id.value()), std::move(where.value())};
    }

private:
    core::LocationSpec spec_;
    std::optional<core::Condition> condition_;
};

enum class Edit { Delete, Enable, Disable };

template <Edit E>
class EditBreakpointCommand final : public Command {
public:
    explicit EditBreakpointCommand(BreakpointId id) : id_(id) {}

    static CommandInfo info() {
        if constexpr (E == Edit::Delete) {
            return {"delete", {"d"}, "delete <number>", "Remove a breakpoint."};
        } else if constexpr (E == Edit::Enable) {
            return {"enable", {}, "enable <number>", "Enable a disabled breakpoint."};
        } else {
            return {"disable", {}, "disable <number>", "Keep a breakpoint but do not stop at it."};
        }
    }

    static Result<CommandPtr> parse(CommandArgs args) {
        if (args.size() != 1) {
            return usage_error(info().usage);
        }
        auto id = parse_breakpoint_id(args[0]);
        if (!id) {
            return id.error();
        }
        return std::make_unique<EditBreakpointCommand>(id.value());
    }

    Result<CommandOutput> execute(CommandContext& context) override {
        auto& breakpoints = context.session.breakpoints();
        if constexpr (E == Edit::Delete) {
            return no_output(breakpoints.remove(id_));
        } else {
            return no_output(breakpoints.set_enabled(id_, E == Edit::Enable));
        }
    }

private:
    BreakpointId id_;
};

}  // namespace

Result<void> register_breakpoint_commands(CommandRegistry& registry) {
    if (auto added = registry.add<BreakCommand>(); !added) {
        return added;
    }
    if (auto added = registry.add<EditBreakpointCommand<Edit::Delete>>(); !added) {
        return added;
    }
    if (auto added = registry.add<EditBreakpointCommand<Edit::Enable>>(); !added) {
        return added;
    }
    return registry.add<EditBreakpointCommand<Edit::Disable>>();
}

}  // namespace stepwise::commands
