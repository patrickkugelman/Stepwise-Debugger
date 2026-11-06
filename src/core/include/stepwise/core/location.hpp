#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include <stepwise/base/types.hpp>
#include <stepwise/symbols/symbol_provider.hpp>

namespace stepwise::core {

struct FunctionOffset {
    std::string name;
    std::uint64_t offset;
};

struct CodeLocation {
    Address address;
    std::optional<FunctionOffset> function;
    std::optional<symbols::SourceLocation> source;
};

struct Frame {
    Address pc;
    CodeLocation where;
};

struct FunctionSpec {
    std::string name;
};

struct FileLineSpec {
    std::string file;
    std::uint32_t line;
};

using LocationSpec = std::variant<Address, FunctionSpec, FileLineSpec>;

}  // namespace stepwise::core
