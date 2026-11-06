#pragma once

#include <cstdint>
#include <iosfwd>
#include <string_view>
#include <vector>

namespace stepwise::ui {

class JsonWriter {
public:
    explicit JsonWriter(std::ostream& out) : out_(out) {}

    JsonWriter& begin_object();
    JsonWriter& end_object();
    JsonWriter& begin_array();
    JsonWriter& end_array();
    JsonWriter& key(std::string_view name);

    JsonWriter& string(std::string_view text);
    JsonWriter& number(std::int64_t value);
    JsonWriter& number(std::uint64_t value);
    JsonWriter& boolean(bool value);
    JsonWriter& null();

    void finish();

private:
    void before_value();
    void write_escaped(std::string_view text);

    std::ostream& out_;
    std::vector<bool> first_in_scope_;
    bool after_key_{false};
};

}  // namespace stepwise::ui
