#include "command_groups.hpp"

namespace stepwise::commands {
namespace {

class RegsCommand final : public Command {
public:
    static CommandInfo info() {
        return {"regs", {"registers"}, "regs", "Show the general purpose and segment registers."};
    }
    static Result<CommandPtr> parse(CommandArgs args) { return parse_no_args<RegsCommand>(args); }

    Result<CommandOutput> execute(CommandContext& context) override {
        auto registers = context.session.registers();
        if (!registers) {
            return registers.error();
        }
        return RegistersOutput{registers.value()};
    }
};

class BacktraceCommand final : public Command {
public:
    static CommandInfo info() {
        return {"backtrace", {"bt", "where"}, "backtrace",
                "Show the call stack, innermost first, by following the frame pointers."};
    }
    static Result<CommandPtr> parse(CommandArgs args) {
        return parse_no_args<BacktraceCommand>(args);
    }

    Result<CommandOutput> execute(CommandContext& context) override {
        auto frames = context.session.backtrace();
        if (!frames) {
            return frames.error();
        }
        return BacktraceOutput{std::move(frames.value())};
    }
};

}  // namespace

Result<void> register_inspection_commands(CommandRegistry& registry) {
    if (auto added = registry.add<RegsCommand>(); !added) {
        return added;
    }
    return registry.add<BacktraceCommand>();
}

}  // namespace stepwise::commands
