#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <stepwise/commands/command_registry.hpp>
#include <stepwise/symbols/symbol_provider.hpp>

namespace stepwise::ui {

class Completer {
public:
    Completer(const commands::CommandRegistry& registry, const symbols::SymbolProvider& symbols) noexcept
        : registry_(registry), symbols_(symbols) {}

    [[nodiscard]] std::vector<std::string> complete(std::string_view line) const;

private:
    const commands::CommandRegistry& registry_;
    const symbols::SymbolProvider& symbols_;
};

}  // namespace stepwise::ui
