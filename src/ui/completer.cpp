#include <stepwise/ui/completer.hpp>

#include <algorithm>
#include <cctype>

namespace stepwise::ui {

std::vector<std::string> Completer::complete(std::string_view line) const {
    auto tokens = commands::tokenize(line);
    const bool starting_new_word = tokens.empty() || std::isspace(static_cast<unsigned char>(line.back())) != 0;
    std::string_view partial;
    if (!starting_new_word) {
        partial = tokens.back();
        tokens.pop_back();
    }
    const std::size_t position = tokens.size();

    std::vector<std::string> candidates;
    const auto offer = [&](std::string_view word) {
        if (word.starts_with(partial)) {
            candidates.emplace_back(word);
        }
    };

    if (position == 0) {
        for (const auto& entry : registry_.entries()) {
            offer(entry.info.name);
            for (const std::string& alias : entry.info.aliases) {
                offer(alias);
            }
        }
    } else if (const auto* entry = registry_.find(tokens.front());
               entry != nullptr && position - 1 < entry->info.arguments.size()) {
        const commands::ArgumentSlot& slot = entry->info.arguments[position - 1];
        switch (slot.kind) {
            case commands::ArgumentKind::Word:
                for (const std::string& word : slot.words) {
                    offer(word);
                }
                break;
            case commands::ArgumentKind::Location:
                for (const std::string& name : symbols_.source_function_names()) {
                    offer(name);
                }
                break;
            case commands::ArgumentKind::CommandName:
                for (const auto& other : registry_.entries()) {
                    offer(other.info.name);
                }
                break;
        }
    }

    std::ranges::sort(candidates);
    candidates.erase(std::ranges::unique(candidates).begin(), candidates.end());

    std::string typed;
    for (const std::string_view word : tokens) {
        typed.append(word).push_back(' ');
    }
    for (std::string& candidate : candidates) {
        candidate.insert(0, typed);
    }
    return candidates;
}

}  // namespace stepwise::ui
