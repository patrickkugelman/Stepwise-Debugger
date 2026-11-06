#include <stepwise/commands/command_registry.hpp>

#include <algorithm>
#include <format>
#include <utility>

namespace stepwise::commands {
namespace {

constexpr std::string_view whitespace = " \t\r\n";

}  // namespace

std::vector<std::string_view> tokenize(std::string_view line) {
    std::vector<std::string_view> tokens;
    std::size_t begin = line.find_first_not_of(whitespace);
    while (begin != std::string_view::npos) {
        const std::size_t end = line.find_first_of(whitespace, begin);
        tokens.push_back(line.substr(begin, end == std::string_view::npos ? end : end - begin));
        begin = line.find_first_not_of(whitespace, end);
    }
    return tokens;
}

const CommandRegistry::Entry* CommandRegistry::find(std::string_view name) const noexcept {
    const auto match = std::ranges::find_if(entries_, [name](const Entry& entry) {
        return entry.info.name == name || std::ranges::find(entry.info.aliases, name) !=
                                              entry.info.aliases.end();
    });
    return match == entries_.end() ? nullptr : &*match;
}

Result<CommandPtr> CommandRegistry::parse_line(std::string_view line) const {
    const auto tokens = tokenize(line);
    if (tokens.empty()) {
        return Error{ErrorCode::InvalidArgument, "empty command"};
    }
    const Entry* entry = find(tokens.front());
    if (entry == nullptr) {
        return Error{ErrorCode::NotFound, std::format("unknown command '{}'", tokens.front())};
    }
    return entry->parse(CommandArgs{tokens}.subspan(1));
}

Result<void> CommandRegistry::add_entry(Entry entry) {
    const auto reject_if_taken = [this](const std::string& name) -> Result<void> {
        if (find(name) != nullptr) {
            return Error{ErrorCode::AlreadyExists,
                         std::format("command name '{}' is already registered", name)};
        }
        return {};
    };
    if (auto free = reject_if_taken(entry.info.name); !free) {
        return free;
    }
    for (const std::string& alias : entry.info.aliases) {
        if (auto free = reject_if_taken(alias); !free) {
            return free;
        }
    }
    entries_.push_back(std::move(entry));
    return {};
}

}  // namespace stepwise::commands
