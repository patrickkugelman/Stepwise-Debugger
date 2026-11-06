#include <algorithm>
#include <format>
#include <optional>
#include <utility>

#include "argument_parsing.hpp"
#include "command_groups.hpp"

namespace stepwise::commands {
namespace {

constexpr std::uint32_t lines_before = 5;
constexpr std::uint32_t lines_shown = 10;

class ListCommand final : public Command {
public:
    explicit ListCommand(std::optional<core::LocationSpec> spec) : spec_(std::move(spec)) {}

    static CommandInfo info() {
        return {"list", {"l"}, "list [address | function | file:line]",
                "Show ten source lines around a location (default: the current one).",
                {{ArgumentKind::Location, {}}}};
    }

    static Result<CommandPtr> parse(CommandArgs args) {
        if (args.size() > 1) {
            return usage_error(info().usage);
        }
        std::optional<core::LocationSpec> spec;
        if (args.size() == 1) {
            auto parsed = parse_location(args[0]);
            if (!parsed) {
                return parsed.error();
            }
            spec = std::move(parsed.value());
        }
        return std::make_unique<ListCommand>(std::move(spec));
    }

    Result<CommandOutput> execute(CommandContext& context) override {
        auto& session = context.session;
        auto where = spec_ ? session.resolve_location(*spec_, false) : session.current_location();
        if (!where) {
            return where.error();
        }
        if (!where.value().source) {
            return Error{ErrorCode::NotFound,
                         std::format("no source line for address {:#x}", where.value().address.value())};
        }
        const symbols::SourceLocation& source = *where.value().source;

        auto lines = session.source_lines(source.file);
        if (!lines) {
            return lines.error();
        }
        const std::uint32_t first = source.line > lines_before ? source.line - lines_before : 1;
        if (first > lines.value().size()) {
            return Error{ErrorCode::InvalidArgument,
                         std::format("line {} is past the end of '{}'", source.line, source.file)};
        }
        const std::size_t last = std::min<std::size_t>(first - 1 + lines_shown, lines.value().size());
        return ListOutput{source.file, first,
                          std::vector<std::string>(lines.value().begin() + (first - 1),
                                                   lines.value().begin() + static_cast<std::ptrdiff_t>(last))};
    }

private:
    std::optional<core::LocationSpec> spec_;
};

}  // namespace

Result<void> register_source_commands(CommandRegistry& registry) {
    return registry.add<ListCommand>();
}

}  // namespace stepwise::commands
