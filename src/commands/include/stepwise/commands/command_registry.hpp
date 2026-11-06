#pragma once

#include <concepts>
#include <span>
#include <string_view>
#include <vector>

#include <stepwise/commands/command.hpp>

namespace stepwise::commands {

template <class C>
concept RegistrableCommand = std::derived_from<C, Command> && requires(CommandArgs args) {
    { C::info() } -> std::same_as<CommandInfo>;
    { C::parse(args) } -> std::same_as<Result<CommandPtr>>;
};

[[nodiscard]] std::vector<std::string_view> tokenize(std::string_view line);

class CommandRegistry {
public:
    using ParseFn = Result<CommandPtr> (*)(CommandArgs);

    struct Entry {
        CommandInfo info;
        ParseFn parse;
    };

    template <RegistrableCommand C>
    Result<void> add() {
        return add_entry(Entry{C::info(), &C::parse});
    }

    [[nodiscard]] const Entry* find(std::string_view name) const noexcept;
    [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }

    [[nodiscard]] Result<CommandPtr> parse_line(std::string_view line) const;

private:
    Result<void> add_entry(Entry entry);

    std::vector<Entry> entries_;
};

}  // namespace stepwise::commands
