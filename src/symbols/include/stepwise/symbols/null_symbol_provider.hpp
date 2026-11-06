#pragma once

#include <stepwise/symbols/symbol_provider.hpp>

namespace stepwise::symbols {

class NullSymbolProvider final : public SymbolProvider {
public:
    NullSymbolProvider() = default;

    std::optional<FunctionSymbol> find_function(std::string_view) const override { return std::nullopt; }
    std::optional<FunctionSymbol> function_at(Address) const override { return std::nullopt; }
    std::vector<std::string> source_function_names() const override { return {}; }
    Address breakpoint_address(const FunctionSymbol& function) const override { return function.address; }
    std::optional<LineInfo> line_at(Address) const override { return std::nullopt; }
    std::optional<LineInfo> find_line(std::string_view, std::uint32_t) const override {
        return std::nullopt;
    }
};

}  // namespace stepwise::symbols
