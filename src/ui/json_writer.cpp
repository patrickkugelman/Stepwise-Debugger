#include "json_writer.hpp"

#include <format>
#include <ostream>

namespace stepwise::ui {
namespace {

std::size_t utf8_sequence_length(std::string_view text, std::size_t i) {
    const auto byte = [&](std::size_t at) { return static_cast<unsigned char>(text[at]); };
    const unsigned char lead = byte(i);
    std::size_t length = 0;
    unsigned char second_min = 0x80;
    unsigned char second_max = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        if (lead == 0xE0) {
            second_min = 0xA0;
        } else if (lead == 0xED) {
            second_max = 0x9F;
        }
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        if (lead == 0xF0) {
            second_min = 0x90;
        } else if (lead == 0xF4) {
            second_max = 0x8F;
        }
    } else {
        return 0;
    }
    if (i + length > text.size() || byte(i + 1) < second_min || byte(i + 1) > second_max) {
        return 0;
    }
    for (std::size_t k = 2; k < length; ++k) {
        if ((byte(i + k) & 0xC0U) != 0x80U) {
            return 0;
        }
    }
    return length;
}

}  // namespace

void JsonWriter::before_value() {
    if (after_key_) {
        after_key_ = false;
        return;
    }
    if (!first_in_scope_.empty()) {
        if (!first_in_scope_.back()) {
            out_ << ',';
        }
        first_in_scope_.back() = false;
    }
}

JsonWriter& JsonWriter::begin_object() {
    before_value();
    out_ << '{';
    first_in_scope_.push_back(true);
    return *this;
}

JsonWriter& JsonWriter::end_object() {
    out_ << '}';
    first_in_scope_.pop_back();
    return *this;
}

JsonWriter& JsonWriter::begin_array() {
    before_value();
    out_ << '[';
    first_in_scope_.push_back(true);
    return *this;
}

JsonWriter& JsonWriter::end_array() {
    out_ << ']';
    first_in_scope_.pop_back();
    return *this;
}

JsonWriter& JsonWriter::key(std::string_view name) {
    before_value();
    write_escaped(name);
    out_ << ':';
    after_key_ = true;
    return *this;
}

JsonWriter& JsonWriter::string(std::string_view text) {
    before_value();
    write_escaped(text);
    return *this;
}

JsonWriter& JsonWriter::number(std::int64_t value) {
    before_value();
    out_ << value;
    return *this;
}

JsonWriter& JsonWriter::number(std::uint64_t value) {
    before_value();
    out_ << value;
    return *this;
}

JsonWriter& JsonWriter::boolean(bool value) {
    before_value();
    out_ << (value ? "true" : "false");
    return *this;
}

JsonWriter& JsonWriter::null() {
    before_value();
    out_ << "null";
    return *this;
}

void JsonWriter::finish() {
    out_ << '\n' << std::flush;
}

void JsonWriter::write_escaped(std::string_view text) {
    out_ << '"';
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            switch (c) {
                case '"':
                    out_ << "\\\"";
                    break;
                case '\\':
                    out_ << "\\\\";
                    break;
                case '\n':
                    out_ << "\\n";
                    break;
                case '\r':
                    out_ << "\\r";
                    break;
                case '\t':
                    out_ << "\\t";
                    break;
                default:
                    if (c < 0x20) {
                        out_ << std::format("\\u{:04x}", static_cast<unsigned>(c));
                    } else {
                        out_ << static_cast<char>(c);
                    }
            }
            ++i;
            continue;
        }
        if (const std::size_t length = utf8_sequence_length(text, i); length != 0) {
            out_ << text.substr(i, length);
            i += length;
        } else {
            out_ << "\\ufffd";
            ++i;
        }
    }
    out_ << '"';
}

}  // namespace stepwise::ui
