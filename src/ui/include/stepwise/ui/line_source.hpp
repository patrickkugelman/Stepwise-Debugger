#pragma once

#include <istream>
#include <optional>
#include <string>

namespace stepwise::ui {

class LineSource {
public:
    virtual ~LineSource() = default;

    LineSource(const LineSource&) = delete;
    LineSource& operator=(const LineSource&) = delete;

    virtual std::optional<std::string> next_line() = 0;

protected:
    LineSource() = default;
};

class StreamLineSource final : public LineSource {
public:
    explicit StreamLineSource(std::istream& in) noexcept : in_(in) {}

    std::optional<std::string> next_line() override {
        std::string line;
        if (!std::getline(in_, line)) {
            return std::nullopt;
        }
        return line;
    }

private:
    std::istream& in_;
};

}  // namespace stepwise::ui
