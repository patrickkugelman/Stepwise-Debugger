#include <stepwise/symbols/dwarf_line.hpp>

#include <algorithm>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <string_view>
#include <utility>

#include <stepwise/symbols/byte_reader.hpp>

namespace stepwise::symbols {
namespace {

constexpr std::uint16_t supported_version = 5;

constexpr std::uint8_t lns_copy = 1;
constexpr std::uint8_t lns_advance_pc = 2;
constexpr std::uint8_t lns_advance_line = 3;
constexpr std::uint8_t lns_set_file = 4;
constexpr std::uint8_t lns_set_column = 5;
constexpr std::uint8_t lns_negate_stmt = 6;
constexpr std::uint8_t lns_set_basic_block = 7;
constexpr std::uint8_t lns_const_add_pc = 8;
constexpr std::uint8_t lns_fixed_advance_pc = 9;
constexpr std::uint8_t lns_set_prologue_end = 10;
constexpr std::uint8_t lns_set_epilogue_begin = 11;
constexpr std::uint8_t lns_set_isa = 12;

constexpr std::uint8_t lne_end_sequence = 1;
constexpr std::uint8_t lne_set_address = 2;

constexpr std::uint64_t lnct_path = 1;
constexpr std::uint64_t lnct_directory_index = 2;

constexpr std::uint64_t form_block = 0x09;
constexpr std::uint64_t form_data1 = 0x0b;
constexpr std::uint64_t form_data2 = 0x05;
constexpr std::uint64_t form_data4 = 0x06;
constexpr std::uint64_t form_data8 = 0x07;
constexpr std::uint64_t form_data16 = 0x1e;
constexpr std::uint64_t form_string = 0x08;
constexpr std::uint64_t form_strp = 0x0e;
constexpr std::uint64_t form_line_strp = 0x1f;
constexpr std::uint64_t form_udata = 0x0f;

Error bad_line_table(std::string_view what) {
    return Error{ErrorCode::Protocol, std::format("malformed .debug_line: {}", what)};
}

#define STEPWISE_TRY(var, expr)                 \
    auto var = (expr);                          \
    if (!var) {                                 \
        return var.error();                     \
    }

struct FormValue {
    std::optional<std::string> text;
    std::uint64_t number{};
};

Result<std::string> string_at(std::span<const std::byte> section, std::uint64_t offset,
                              std::string_view section_name) {
    ByteReader reader{section};
    if (section.empty() || !reader.seek(offset)) {
        return bad_line_table(std::format("string offset {:#x} is outside {}", offset, section_name));
    }
    STEPWISE_TRY(text, reader.cstring());
    return std::string{text.value()};
}

Result<FormValue> read_form(ByteReader& reader, std::uint64_t form, unsigned offset_size,
                            const DebugLineSections& sections) {
    FormValue value;
    switch (form) {
        case form_string: {
            STEPWISE_TRY(text, reader.cstring());
            value.text = std::string{text.value()};
            return value;
        }
        case form_line_strp:
        case form_strp: {
            STEPWISE_TRY(offset, reader.unsigned_of_size(offset_size));
            const bool line_strp = form == form_line_strp;
            STEPWISE_TRY(text, string_at(line_strp ? sections.line_str : sections.str, offset.value(),
                                         line_strp ? ".debug_line_str" : ".debug_str"));
            value.text = std::move(text.value());
            return value;
        }
        case form_data1:
        case form_data2:
        case form_data4:
        case form_data8: {
            const unsigned size = form == form_data1 ? 1 : form == form_data2 ? 2 : form == form_data4 ? 4 : 8;
            STEPWISE_TRY(number, reader.unsigned_of_size(size));
            value.number = number.value();
            return value;
        }
        case form_udata: {
            STEPWISE_TRY(number, reader.uleb128());
            value.number = number.value();
            return value;
        }
        case form_data16: {
            STEPWISE_TRY(skipped, reader.skip(16));
            return value;
        }
        case form_block: {
            STEPWISE_TRY(length, reader.uleb128());
            STEPWISE_TRY(skipped, reader.skip(length.value()));
            return value;
        }
        default:
            return bad_line_table(std::format("unsupported attribute form {:#x}", form));
    }
}

struct Entry {
    std::string path;
    std::uint64_t directory{};
};

Result<std::vector<Entry>> read_entry_table(ByteReader& reader, unsigned offset_size,
                                            const DebugLineSections& sections) {
    STEPWISE_TRY(format_count, reader.u8());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> formats;
    for (unsigned i = 0; i < format_count.value(); ++i) {
        STEPWISE_TRY(content, reader.uleb128());
        STEPWISE_TRY(form, reader.uleb128());
        formats.emplace_back(content.value(), form.value());
    }

    STEPWISE_TRY(count, reader.uleb128());
    if (count.value() != 0 && formats.empty()) {
        return bad_line_table("entries without a format description");
    }
    std::vector<Entry> entries;
    for (std::uint64_t i = 0; i < count.value(); ++i) {
        Entry entry;
        for (const auto& [content, form] : formats) {
            STEPWISE_TRY(value, read_form(reader, form, offset_size, sections));
            if (content == lnct_path) {
                entry.path = value.value().text.value_or("");
            } else if (content == lnct_directory_index) {
                entry.directory = value.value().number;
            }
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

bool is_absolute(std::string_view path) {
    return !path.empty() && path.front() == '/';
}

std::string join(std::string_view directory, std::string_view name) {
    if (directory.empty()) {
        return std::string{name};
    }
    return std::format("{}/{}", directory, name);
}

struct Header {
    unsigned offset_size{4};
    std::uint8_t min_instruction_length{1};
    bool default_is_statement{true};
    std::int8_t line_base{};
    std::uint8_t line_range{1};
    std::uint8_t opcode_base{1};
    std::vector<std::uint8_t> opcode_lengths;
    std::vector<std::string> files;
};

struct Registers {
    std::uint64_t address = 0;
    std::uint64_t file = 1;
    std::int64_t line = 1;
    bool is_statement = true;
};

class LineTableBuilder {
public:
    std::uint32_t intern(const std::string& path) {
        const auto [it, inserted] = index_.try_emplace(path, static_cast<std::uint32_t>(table_.files.size()));
        if (inserted) {
            table_.files.push_back(path);
        }
        return it->second;
    }

    void add(const LineRow& row) { table_.rows.push_back(row); }

    LineTable finish() && {
        std::ranges::stable_sort(table_.rows, [](const LineRow& a, const LineRow& b) {
            if (a.address != b.address) {
                return a.address < b.address;
            }
            return a.end_sequence && !b.end_sequence;
        });
        return std::move(table_);
    }

private:
    LineTable table_;
    std::map<std::string, std::uint32_t> index_;
};

Result<Header> read_header(ByteReader& unit, unsigned offset_size, const DebugLineSections& sections,
                           std::uint64_t& program_start) {
    Header header;
    header.offset_size = offset_size;

    STEPWISE_TRY(address_size, unit.u8());
    STEPWISE_TRY(segment_selector_size, unit.u8());
    STEPWISE_TRY(header_length, unit.unsigned_of_size(offset_size));
    program_start = unit.position() + header_length.value();
    if (address_size.value() != 8 || segment_selector_size.value() != 0) {
        return bad_line_table("only 8-byte addresses without segments are supported");
    }

    STEPWISE_TRY(min_length, unit.u8());
    STEPWISE_TRY(max_ops, unit.u8());
    STEPWISE_TRY(default_is_stmt, unit.u8());
    STEPWISE_TRY(line_base, unit.i8());
    STEPWISE_TRY(line_range, unit.u8());
    STEPWISE_TRY(opcode_base, unit.u8());
    if (max_ops.value() != 1) {
        return bad_line_table("VLIW line programs are not supported");
    }
    if (line_range.value() == 0 || opcode_base.value() == 0) {
        return bad_line_table("invalid line_range or opcode_base");
    }
    header.min_instruction_length = min_length.value();
    header.default_is_statement = default_is_stmt.value() != 0;
    header.line_base = line_base.value();
    header.line_range = line_range.value();
    header.opcode_base = opcode_base.value();
    for (unsigned i = 1; i < header.opcode_base; ++i) {
        STEPWISE_TRY(length, unit.u8());
        header.opcode_lengths.push_back(length.value());
    }

    STEPWISE_TRY(directories, read_entry_table(unit, offset_size, sections));
    STEPWISE_TRY(files, read_entry_table(unit, offset_size, sections));
    if (directories.value().empty()) {
        return bad_line_table("empty directory table");
    }

    const auto& dirs = directories.value();
    for (const Entry& file : files.value()) {
        if (is_absolute(file.path)) {
            header.files.push_back(file.path);
            continue;
        }
        if (file.directory >= dirs.size()) {
            return bad_line_table("file entry refers to a missing directory");
        }
        const std::string& directory = dirs[static_cast<std::size_t>(file.directory)].path;
        const std::string base = is_absolute(directory) ? directory : join(dirs.front().path, directory);
        header.files.push_back(join(base, file.path));
    }
    return header;
}

Result<void> run_program(ByteReader& program, const Header& header,
                         const std::vector<std::uint32_t>& file_ids, LineTableBuilder& builder) {
    Registers state;
    state.is_statement = header.default_is_statement;

    const auto emit = [&](bool end_sequence) -> Result<void> {
        if (state.file >= file_ids.size()) {
            return bad_line_table("line program refers to a missing file");
        }
        builder.add(LineRow{state.address, file_ids[static_cast<std::size_t>(state.file)],
                            static_cast<std::uint32_t>(state.line), state.is_statement, end_sequence});
        return {};
    };
    const auto advance_address = [&](std::uint64_t operation_advance) {
        state.address += operation_advance * header.min_instruction_length;
    };
    const auto reset = [&] {
        state = Registers{};
        state.is_statement = header.default_is_statement;
    };

    while (!program.at_end()) {
        STEPWISE_TRY(opcode, program.u8());
        const std::uint8_t op = opcode.value();

        if (op >= header.opcode_base) {
            const unsigned adjusted = op - header.opcode_base;
            advance_address(adjusted / header.line_range);
            state.line += header.line_base + static_cast<int>(adjusted % header.line_range);
            if (state.line < 0) {
                return bad_line_table("line number went negative");
            }
            STEPWISE_TRY(emitted, emit(false));
            continue;
        }

        if (op == 0) {
            STEPWISE_TRY(length, program.uleb128());
            if (length.value() == 0) {
                continue;
            }
            STEPWISE_TRY(body, program.sub(length.value()));
            STEPWISE_TRY(sub_opcode, body.value().u8());
            if (sub_opcode.value() == lne_end_sequence) {
                STEPWISE_TRY(emitted, emit(true));
                reset();
            } else if (sub_opcode.value() == lne_set_address) {
                STEPWISE_TRY(address, body.value().u64());
                state.address = address.value();
            }
            continue;
        }

        switch (op) {
            case lns_copy: {
                STEPWISE_TRY(emitted, emit(false));
                break;
            }
            case lns_advance_pc: {
                STEPWISE_TRY(advance, program.uleb128());
                advance_address(advance.value());
                break;
            }
            case lns_advance_line: {
                STEPWISE_TRY(delta, program.sleb128());
                state.line += delta.value();
                if (state.line < 0 || state.line > std::numeric_limits<std::uint32_t>::max()) {
                    return bad_line_table("line number out of range");
                }
                break;
            }
            case lns_set_file: {
                STEPWISE_TRY(file, program.uleb128());
                state.file = file.value();
                break;
            }
            case lns_set_column: {
                STEPWISE_TRY(column, program.uleb128());
                break;
            }
            case lns_negate_stmt:
                state.is_statement = !state.is_statement;
                break;
            case lns_set_basic_block:
            case lns_set_prologue_end:
            case lns_set_epilogue_begin:
                break;
            case lns_const_add_pc:
                advance_address((255U - header.opcode_base) / header.line_range);
                break;
            case lns_fixed_advance_pc: {
                STEPWISE_TRY(advance, program.u16());
                state.address += advance.value();
                break;
            }
            case lns_set_isa: {
                STEPWISE_TRY(isa, program.uleb128());
                break;
            }
            default:
                for (std::uint8_t i = 0; i < header.opcode_lengths[op - 1]; ++i) {
                    STEPWISE_TRY(argument, program.uleb128());
                }
                break;
        }
    }
    return {};
}

}  // namespace

Result<LineTable> parse_debug_line(const DebugLineSections& sections) {
    LineTableBuilder builder;
    ByteReader section{sections.line};

    while (!section.at_end()) {
        STEPWISE_TRY(length32, section.u32());
        unsigned offset_size = 4;
        std::uint64_t length = length32.value();
        if (length32.value() == 0xffffffffU) {
            STEPWISE_TRY(length64, section.u64());
            offset_size = 8;
            length = length64.value();
        }
        STEPWISE_TRY(unit_result, section.sub(length));
        ByteReader unit = unit_result.value();

        STEPWISE_TRY(version, unit.u16());
        if (version.value() != supported_version) {
            continue;
        }

        std::uint64_t program_start = 0;
        STEPWISE_TRY(header, read_header(unit, offset_size, sections, program_start));
        STEPWISE_TRY(moved, unit.seek(program_start));

        std::vector<std::uint32_t> file_ids;
        for (const std::string& path : header.value().files) {
            file_ids.push_back(builder.intern(path));
        }
        STEPWISE_TRY(ran, run_program(unit, header.value(), file_ids, builder));
    }
    return std::move(builder).finish();
}

}  // namespace stepwise::symbols
