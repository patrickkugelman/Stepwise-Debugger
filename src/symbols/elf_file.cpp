#include <stepwise/symbols/elf_file.hpp>

#include <algorithm>
#include <format>
#include <fstream>
#include <optional>
#include <system_error>
#include <utility>

#include <stepwise/symbols/byte_reader.hpp>

namespace stepwise::symbols {
namespace {

constexpr std::uint16_t machine_x86_64 = 62;
constexpr std::uint32_t section_type_symtab = 2;
constexpr std::uint32_t section_type_nobits = 8;
constexpr std::size_t section_header_size = 64;
constexpr std::size_t symbol_entry_size = 24;
constexpr std::uint16_t section_index_undefined = 0;

Error malformed(std::string_view what) {
    return Error{ErrorCode::Protocol, std::format("malformed ELF file: {}", what)};
}

struct RawSection {
    std::uint32_t name_offset{};
    ElfSection section;
};

template <class T, class U>
void take(Result<T> result, U& out, std::optional<Error>& failure) {
    if (result) {
        out = static_cast<U>(result.value());
    } else if (!failure) {
        failure = result.error();
    }
}

Result<RawSection> read_section_header(ByteReader& reader) {
    RawSection raw;
    std::uint32_t info = 0;
    std::uint64_t alignment = 0;
    std::uint64_t entry_size = 0;
    std::optional<Error> failure;
    take(reader.u32(), raw.name_offset, failure);
    take(reader.u32(), raw.section.type, failure);
    take(reader.u64(), raw.section.flags, failure);
    take(reader.u64(), raw.section.address, failure);
    take(reader.u64(), raw.section.offset, failure);
    take(reader.u64(), raw.section.size, failure);
    take(reader.u32(), raw.section.link, failure);
    take(reader.u32(), info, failure);
    take(reader.u64(), alignment, failure);
    take(reader.u64(), entry_size, failure);
    if (failure) {
        return *failure;
    }
    return raw;
}

}  // namespace

Result<ElfFile> ElfFile::load(const std::filesystem::path& path) {
    std::error_code code;
    const std::uintmax_t size = std::filesystem::file_size(path, code);
    if (code) {
        return Error{ErrorCode::NotFound,
                     std::format("cannot read '{}': {}", path.string(), code.message())};
    }
    std::ifstream stream{path, std::ios::binary};
    std::vector<std::byte> image(static_cast<std::size_t>(size));
    if (!stream.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size()))) {
        return Error{ErrorCode::Os, std::format("cannot read '{}'", path.string())};
    }
    return parse(std::move(image));
}

Result<ElfFile> ElfFile::parse(std::vector<std::byte> image) {
    ElfFile file;
    file.image_ = std::move(image);
    ByteReader header{file.image_};

    auto ident = header.bytes(16);
    if (!ident) {
        return malformed("file too short");
    }
    const auto id = ident.value();
    if (id[0] != std::byte{0x7f} || id[1] != std::byte{'E'} || id[2] != std::byte{'L'} ||
        id[3] != std::byte{'F'}) {
        return malformed("not an ELF file");
    }
    if (id[4] != std::byte{2} || id[5] != std::byte{1}) {
        return malformed("only 64-bit little-endian ELF is supported");
    }

    auto type = header.u16();
    auto machine = header.u16();
    auto version = header.u32();
    auto entry = header.u64();
    auto program_header_offset = header.u64();
    auto section_header_offset = header.u64();
    auto flags = header.u32();
    auto header_size = header.u16();
    auto program_header_size = header.u16();
    auto program_header_count = header.u16();
    auto section_entry_size = header.u16();
    auto section_count = header.u16();
    auto name_table_index = header.u16();
    if (!name_table_index || !section_count || !section_entry_size || !section_header_offset ||
        !type || !machine || !version || !entry || !program_header_offset || !flags ||
        !header_size || !program_header_size || !program_header_count) {
        return malformed("truncated header");
    }
    if (machine.value() != machine_x86_64) {
        return malformed("not an x86-64 binary");
    }
    file.type_ = type.value();

    if (section_count.value() == 0) {
        return file;
    }
    if (section_entry_size.value() < section_header_size) {
        return malformed("section header entries are too small");
    }
    if (name_table_index.value() >= section_count.value()) {
        return malformed("section name table index out of range");
    }

    ByteReader table{file.image_};
    if (!table.seek(section_header_offset.value())) {
        return malformed("section header table is outside the file");
    }

    std::vector<RawSection> raw_sections;
    for (std::uint16_t i = 0; i < section_count.value(); ++i) {
        auto entry_reader = table.sub(section_entry_size.value());
        if (!entry_reader) {
            return malformed("section header table is truncated");
        }
        auto raw = read_section_header(entry_reader.value());
        if (!raw) {
            return raw.error();
        }
        raw_sections.push_back(std::move(raw.value()));
    }

    auto names = file.section_data(raw_sections[name_table_index.value()].section);
    if (!names) {
        return malformed("section name table is outside the file");
    }
    for (RawSection& raw : raw_sections) {
        ByteReader name_reader{names.value()};
        if (!name_reader.seek(raw.name_offset)) {
            return malformed("section name is outside the name table");
        }
        auto name = name_reader.cstring();
        if (!name) {
            return malformed("unterminated section name");
        }
        raw.section.name = std::string{name.value()};
        file.sections_.push_back(std::move(raw.section));
    }

    for (const ElfSection& section : file.sections_) {
        if (section.type != section_type_symtab) {
            continue;
        }
        if (section.link >= file.sections_.size()) {
            return malformed("symbol table links to a missing string table");
        }
        auto data = file.section_data(section);
        auto strings = file.section_data(file.sections_[section.link]);
        if (!data || !strings) {
            return malformed("symbol table is outside the file");
        }
        ByteReader symbols{data.value()};
        while (symbols.remaining() >= symbol_entry_size) {
            auto name_offset = symbols.u32();
            auto info = symbols.u8();
            auto other = symbols.u8();
            auto section_index = symbols.u16();
            auto value = symbols.u64();
            auto size = symbols.u64();
            if (!name_offset || !info || !other || !section_index || !value || !size) {
                return malformed("symbol table is truncated");
            }
            ByteReader name_reader{strings.value()};
            if (!name_reader.seek(name_offset.value())) {
                return malformed("symbol name is outside the string table");
            }
            auto name = name_reader.cstring();
            if (!name) {
                return malformed("unterminated symbol name");
            }
            const unsigned type_bits = info.value() & 0x0fU;
            ElfSymbol symbol;
            symbol.name = std::string{name.value()};
            symbol.value = value.value();
            symbol.size = size.value();
            symbol.type = type_bits <= 2 ? static_cast<ElfSymbolType>(type_bits) : ElfSymbolType::Other;
            symbol.defined = section_index.value() != section_index_undefined;
            file.symbols_.push_back(std::move(symbol));
        }
    }
    return file;
}

const ElfSection* ElfFile::find_section(std::string_view name) const noexcept {
    const auto match =
        std::ranges::find_if(sections_, [name](const ElfSection& section) { return section.name == name; });
    return match == sections_.end() ? nullptr : &*match;
}

Result<std::span<const std::byte>> ElfFile::section_data(const ElfSection& section) const {
    if (section.type == section_type_nobits || section.size == 0) {
        return std::span<const std::byte>{};
    }
    if (section.offset > image_.size() || section.size > image_.size() - section.offset) {
        return Error{ErrorCode::Protocol,
                     std::format("section '{}' extends past the end of the file", section.name)};
    }
    return std::span<const std::byte>{image_}.subspan(static_cast<std::size_t>(section.offset),
                                                      static_cast<std::size_t>(section.size));
}

}  // namespace stepwise::symbols
