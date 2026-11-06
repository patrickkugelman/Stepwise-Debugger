#include "command_groups.hpp"

namespace stepwise::commands {
namespace {

class RunCommand final : public Command {
public:
    static CommandInfo info() {
        return {"run", {"r"}, "run", "Start the program and stop at its first instruction."};
    }
    static Result<CommandPtr> parse(CommandArgs args) { return parse_no_args<RunCommand>(args); }

    Result<CommandOutput> execute(CommandContext& context) override {
        return no_output(context.session.run());
    }
};

class ContinueCommand final : public Command {
public:
    static CommandInfo info() {
        return {"continue", {"c", "cont"}, "continue", "Resume until the next stop or exit."};
    }
    static Result<CommandPtr> parse(CommandArgs args) {
        return parse_no_args<ContinueCommand>(args);
    }

    Result<CommandOutput> execute(CommandContext& context) override {
        return no_output(context.session.cont());
    }
};

class StepiCommand final : public Command {
public:
    static CommandInfo info() {
        return {"stepi", {"si"}, "stepi", "Execute exactly one machine instruction."};
    }
    static Result<CommandPtr> parse(CommandArgs args) { return parse_no_args<StepiCommand>(args); }

    Result<CommandOutput> execute(CommandContext& context) override {
        return no_output(context.session.step_instruction());
    }
};

class StepCommand final : public Command {
public:
    static CommandInfo info() {
        return {"step", {"s"}, "step",
                "Run to the start of the next source line, entering calls that have line info."};
    }
    static Result<CommandPtr> parse(CommandArgs args) { return parse_no_args<StepCommand>(args); }

    Result<CommandOutput> execute(CommandContext& context) override {
        return no_output(context.session.step_line());
    }
};

}  // namespace

Result<void> register_execution_commands(CommandRegistry& registry) {
    if (auto added = registry.add<RunCommand>(); !added) {
        return added;
    }
    if (auto added = registry.add<ContinueCommand>(); !added) {
        return added;
    }
    if (auto added = registry.add<StepiCommand>(); !added) {
        return added;
    }
    return registry.add<StepCommand>();
}

}  // namespace stepwise::commands
