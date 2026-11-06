#include <format>
#include <optional>
#include <string>
#include <utility>

#include "command_groups.hpp"

namespace stepwise::commands {
namespace {

class QuitCommand final : public Command {
public:
    static CommandInfo info() {
        return {"quit", {"q", "exit"}, "quit", "Kill any running process and leave the debugger."};
    }
    static Result<CommandPtr> parse(CommandArgs args) { return parse_no_args<QuitCommand>(args); }

    Result<CommandOutput> execute(CommandContext&) override { return QuitOutput{}; }
};

class HelpCommand final : public Command {
public:
    explicit HelpCommand(std::optional<std::string> topic) : topic_(std::move(topic)) {}

    static CommandInfo info() {
        return {"help", {"h"}, "help [command]", "List commands or describe one command.",
                {{ArgumentKind::CommandName, {}}}};
    }

    static Result<CommandPtr> parse(CommandArgs args) {
        if (args.size() > 1) {
            return Error{ErrorCode::InvalidArgument, std::format("usage: {}", info().usage)};
        }
        std::optional<std::string> topic;
        if (!args.empty()) {
            topic.emplace(args.front());
        }
        return std::make_unique<HelpCommand>(std::move(topic));
    }

    Result<CommandOutput> execute(CommandContext& context) override {
        if (topic_) {
            const auto* entry = context.registry.find(*topic_);
            if (entry == nullptr) {
                return Error{ErrorCode::NotFound, std::format("unknown command '{}'", *topic_)};
            }
            return HelpOutput{{entry->info}, true};
        }
        HelpOutput output{{}, false};
        for (const auto& entry : context.registry.entries()) {
            output.commands.push_back(entry.info);
        }
        return output;
    }

private:
    std::optional<std::string> topic_;
};

}  // namespace

Result<void> register_meta_commands(CommandRegistry& registry) {
    if (auto added = registry.add<QuitCommand>(); !added) {
        return added;
    }
    return registry.add<HelpCommand>();
}

}  // namespace stepwise::commands
