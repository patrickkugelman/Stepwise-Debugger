#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <stepwise/base/result.hpp>
#include <stepwise/symbols/dwarf_line.hpp>
#include <stepwise/symbols/symbol_provider.hpp>

namespace stepwise::symbols {

class ElfSymbolProvider final : public SymbolProvider {
public:
    static Result<std::unique_ptr<ElfSymbolProvider>> load(const std::filesystem::path& path);

    ElfSymbolProvider(std::vector<FunctionSymbol> functions, LineTable lines);

    std::optional<FunctionSymbol> find_function(std::string_view name) const override;
    std::optional<FunctionSymbol> function_at(Address address) const override;
    std::vector<std::string> source_function_names() const override;
    Address breakpoint_address(const FunctionSymbol& function) const override;
    std::optional<LineInfo> line_at(Address address) const override;
    std::optional<LineInfo> find_line(std::string_view file, std::uint32_t line) const override;

private:
    [[nodiscard]] LineInfo describe_row(std::size_t index) const;

    std::vector<FunctionSymbol> functions_;
    std::unordered_map<std::string, std::size_t> by_name_;
    LineTable lines_;
};

}  // namespace stepwise::symbols
