#pragma once

#include <optional>
#include <string>

#include <stepwise/ui/completer.hpp>
#include <stepwise/ui/line_source.hpp>

namespace stepwise::ui {

class LinenoiseSource final : public LineSource {
public:
    LinenoiseSource(std::string prompt, const Completer& completer, int history_size);
    ~LinenoiseSource() override;

    std::optional<std::string> next_line() override;

private:
    std::string prompt_;
};

}  // namespace stepwise::ui
