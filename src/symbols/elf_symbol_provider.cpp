#include <stepwise/symbols/elf_symbol_provider.hpp>

#include <algorithm>
#include <format>
#include <utility>

#include <stepwise/symbols/elf_file.hpp>

namespace stepwise::symbols {
namespace {

std::span<const std::byte> section_or_empty(const ElfFile& elf, std::string_view name,
                                            std::optional<Error>& failure) {
    const ElfSection* section = elf.find_section(name);
    if (section == nullptr) {
        return {};
    }
    if ((section->flags & elf_flag_compressed) != 0) {
        failure = Error{ErrorCode::InvalidArgument,
                        std::format("{} is compressed, which is not supported", name)};
        return {};
    }
    auto data = elf.section_data(*section);
    if (!data) {
        failure = data.error();
        return {};
    }
    return data.value();
}

bool path_matches(const std::string& path, std::string_view file) {
    if (path == file) {
        return true;
    }
    return path.size() > file.size() && path.ends_with(file) && path[path.size() - file.size() - 1] == '/';
}

}  // namespace

Result<std::unique_ptr<ElfSymbolProvider>> ElfSymbolProvider::load(const std::filesystem::path& path) {
    auto elf = ElfFile::load(path);
    if (!elf) {
        return elf.error();
    }
    if (elf.value().type() != elf_type_executable) {
        return Error{ErrorCode::InvalidArgument,
                     "only non-PIE executables are supported (link with -no-pie)"};
    }

    std::optional<Error> failure;
    DebugLineSections sections;
    sections.line = section_or_empty(elf.value(), ".debug_line", failure);
    sections.line_str = section_or_empty(elf.value(), ".debug_line_str", failure);
    sections.str = section_or_empty(elf.value(), ".debug_str", failure);
    if (failure) {
        return *failure;
    }
    auto lines = parse_debug_line(sections);
    if (!lines) {
        return lines.error();
    }

    std::vector<FunctionSymbol> functions;
    for (const ElfSymbol& symbol : elf.value().symbols()) {
        if (symbol.type == ElfSymbolType::Function && symbol.defined && !symbol.name.empty()) {
            functions.push_back(FunctionSymbol{symbol.name, Address{symbol.value}, symbol.size});
        }
    }
    return std::make_unique<ElfSymbolProvider>(std::move(functions), std::move(lines.value()));
}

ElfSymbolProvider::ElfSymbolProvider(std::vector<FunctionSymbol> functions, LineTable lines)
    : functions_(std::move(functions)), lines_(std::move(lines)) {
    std::ranges::stable_sort(functions_, {}, &FunctionSymbol::address);
    for (std::size_t i = 0; i < functions_.size(); ++i) {
        by_name_.try_emplace(functions_[i].name, i);
    }
}

std::optional<FunctionSymbol> ElfSymbolProvider::find_function(std::string_view name) const {
    const auto match = by_name_.find(std::string{name});
    if (match == by_name_.end()) {
        return std::nullopt;
    }
    return functions_[match->second];
}

std::optional<FunctionSymbol> ElfSymbolProvider::function_at(Address address) const {
    const auto next = std::ranges::upper_bound(functions_, address, {}, &FunctionSymbol::address);
    if (next == functions_.begin()) {
        return std::nullopt;
    }
    const FunctionSymbol& candidate = *std::prev(next);
    if (address.value() - candidate.address.value() >= candidate.size) {
        return std::nullopt;
    }
    return candidate;
}

std::vector<std::string> ElfSymbolProvider::source_function_names() const {
    std::vector<std::string> names;
    for (const FunctionSymbol& function : functions_) {
        if (line_at(function.address)) {
            names.push_back(function.name);
        }
    }
    std::ranges::sort(names);
    names.erase(std::ranges::unique(names).begin(), names.end());
    return names;
}

LineInfo ElfSymbolProvider::describe_row(std::size_t index) const {
    const LineRow& row = lines_.rows[index];
    return LineInfo{SourceLocation{lines_.files[row.file], row.line}, Address{row.address},
                    Address{lines_.rows[index + 1].address}, row.is_statement};
}

std::optional<LineInfo> ElfSymbolProvider::line_at(Address address) const {
    const auto next = std::ranges::upper_bound(lines_.rows, address.value(), {}, &LineRow::address);
    if (next == lines_.rows.begin() || next == lines_.rows.end()) {
        return std::nullopt;
    }
    const auto row = std::prev(next);
    if (row->end_sequence) {
        return std::nullopt;
    }
    return describe_row(static_cast<std::size_t>(row - lines_.rows.begin()));
}

Address ElfSymbolProvider::breakpoint_address(const FunctionSymbol& function) const {
    const std::uint64_t entry = function.address.value();
    auto row = std::ranges::lower_bound(lines_.rows, entry, {}, &LineRow::address);
    while (row != lines_.rows.end() && row->address == entry && row->end_sequence) {
        ++row;
    }
    if (row == lines_.rows.end() || row->address != entry) {
        return function.address;
    }
    for (; row != lines_.rows.end() && row->address < entry + function.size; ++row) {
        if (row->address > entry && row->is_statement && !row->end_sequence) {
            return Address{row->address};
        }
    }
    return function.address;
}

std::optional<LineInfo> ElfSymbolProvider::find_line(std::string_view file, std::uint32_t line) const {
    std::vector<bool> file_matches;
    file_matches.reserve(lines_.files.size());
    for (const std::string& path : lines_.files) {
        file_matches.push_back(path_matches(path, file));
    }

    std::optional<std::size_t> best;
    for (std::size_t i = 0; i + 1 < lines_.rows.size(); ++i) {
        const LineRow& row = lines_.rows[i];
        const bool has_code = lines_.rows[i + 1].address > row.address;
        if (row.end_sequence || !row.is_statement || !has_code || row.line < line ||
            !file_matches[row.file]) {
            continue;
        }
        const LineRow* current = best ? &lines_.rows[*best] : nullptr;
        if (current == nullptr || row.line < current->line ||
            (row.line == current->line && row.address < current->address)) {
            best = i;
        }
    }
    if (!best) {
        return std::nullopt;
    }
    return describe_row(*best);
}

}  // namespace stepwise::symbols
