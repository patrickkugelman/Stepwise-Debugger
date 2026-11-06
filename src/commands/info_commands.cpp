#include <optional>
#include <utility>

#include "argument_parsing.hpp"
#include "command_groups.hpp"

namespace stepwise::commands {
namespace {

class InfoCommand final : public Command {
public:
    enum class Topic { Breakpoints, Line };

    InfoCommand(Topic topic, std::optional<core::LocationSpec> spec)
        : topic_(topic), spec_(std::move(spec)) {}

    static CommandInfo info() {
        return {"info", {}, "info breakpoints | info line [address | function | file:line]",
                "Show breakpoints, or the address range of a source line (default: the current one).",
                {{ArgumentKind::Word, {"breakpoints", "line"}}, {ArgumentKind::Location, {}}}};
    }

    static Result<CommandPtr> parse(CommandArgs args) {
        if (args.size() == 1 && (args[0] == "breakpoints" || args[0] == "b")) {
            return std::make_unique<InfoCommand>(Topic::Breakpoints, std::nullopt);
        }
        if (!args.empty() && args[0] == "line" && args.size() <= 2) {
            std::optional<core::LocationSpec> spec;
            if (args.size() == 2) {
                auto parsed = parse_location(args[1]);
                if (!parsed) {
                    return parsed.error();
                }
                spec = std::move(parsed.value());
            }
            return std::make_unique<InfoCommand>(Topic::Line, std::move(spec));
        }
        return usage_error(info().usage);
    }

    Result<CommandOutput> execute(CommandContext& context) override {
        return topic_ == Topic::Breakpoints ? list_breakpoints(context.session)
                                            : describe_line(context.session);
    }

private:
    static Result<CommandOutput> list_breakpoints(core::Session& session) {
        BreakpointListOutput output;
        for (const core::Breakpoint& breakpoint : session.breakpoints().list()) {
            output.rows.push_back(BreakpointRow{breakpoint, session.describe(breakpoint.address)});
        }
        return output;
    }

    Result<CommandOutput> describe_line(core::Session& session) const {
        auto where = spec_ ? session.resolve_location(*spec_, false) : session.current_location();
        if (!where) {
            return where.error();
        }
        InfoLineOutput output{where.value(), session.symbols().line_at(where.value().address),
                              std::nullopt, std::nullopt, std::nullopt};
        if (output.line) {
            output.start = session.describe(output.line->start);
            output.end = session.describe(output.line->end);
            const auto* requested = spec_ ? std::get_if<core::FileLineSpec>(&*spec_) : nullptr;
            if (requested != nullptr && requested->line != output.line->location.line) {
                output.requested_line = requested->line;
            }
        }
        return output;
    }

    Topic topic_;
    std::optional<core::LocationSpec> spec_;
};

}  // namespace

Result<void> register_info_commands(CommandRegistry& registry) {
    return registry.add<InfoCommand>();
}

}  // namespace stepwise::commands
