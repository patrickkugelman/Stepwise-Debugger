#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <stepwise/base/types.hpp>

namespace stepwise::symbols {

struct FunctionSymbol {
    std::string name;
    Address address;
    std::uint64_t size;

    friend bool operator==(const FunctionSymbol&, const FunctionSymbol&) = default;
};

struct SourceLocation {
    std::string file;
    std::uint32_t line;

    friend bool operator==(const SourceLocation&, const SourceLocation&) = default;
};

struct LineInfo {
    SourceLocation location;
    Address start;
    Address end;
    bool is_statement;
};

class SymbolProvider {
public:
    virtual ~SymbolProvider() = default;

    SymbolProvider(const SymbolProvider&) = delete;
    SymbolProvider& operator=(const SymbolProvider&) = delete;

    [[nodiscard]] virtual std::optional<FunctionSymbol> find_function(std::string_view name) const = 0;
    [[nodiscard]] virtual std::optional<FunctionSymbol> function_at(Address address) const = 0;

    [[nodiscard]] virtual std::vector<std::string> source_function_names() const = 0;

    [[nodiscard]] virtual Address breakpoint_address(const FunctionSymbol& function) const = 0;

    [[nodiscard]] virtual std::optional<LineInfo> line_at(Address address) const = 0;

    [[nodiscard]] virtual std::optional<LineInfo> find_line(std::string_view file,
                                                            std::uint32_t line) const = 0;

protected:
    SymbolProvider() = default;
};

}  // namespace stepwise::symbols
