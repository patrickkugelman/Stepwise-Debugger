#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <stepwise/base/result.hpp>

namespace stepwise::symbols {

inline constexpr std::uint16_t elf_type_executable = 2;
inline constexpr std::uint64_t elf_flag_compressed = 0x800;

struct ElfSection {
    std::string name;
    std::uint32_t type{};
    std::uint64_t flags{};
    std::uint64_t address{};
    std::uint64_t offset{};
    std::uint64_t size{};
    std::uint32_t link{};
};

enum class ElfSymbolType : std::uint8_t { NoType = 0, Object = 1, Function = 2, Other = 255 };

struct ElfSymbol {
    std::string name;
    std::uint64_t value{};
    std::uint64_t size{};
    ElfSymbolType type{ElfSymbolType::NoType};
    bool defined{};
};

class ElfFile {
public:
    static Result<ElfFile> load(const std::filesystem::path& path);
    static Result<ElfFile> parse(std::vector<std::byte> image);

    [[nodiscard]] std::uint16_t type() const noexcept { return type_; }
    [[nodiscard]] const std::vector<ElfSection>& sections() const noexcept { return sections_; }
    [[nodiscard]] const std::vector<ElfSymbol>& symbols() const noexcept { return symbols_; }

    [[nodiscard]] const ElfSection* find_section(std::string_view name) const noexcept;
    [[nodiscard]] Result<std::span<const std::byte>> section_data(const ElfSection& section) const;

private:
    ElfFile() = default;

    std::vector<std::byte> image_;
    std::uint16_t type_{};
    std::vector<ElfSection> sections_;
    std::vector<ElfSymbol> symbols_;
};

}  // namespace stepwise::symbols
