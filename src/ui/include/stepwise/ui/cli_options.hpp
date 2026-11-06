#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <stepwise/base/log.hpp>
#include <stepwise/base/result.hpp>
#include <stepwise/ui/renderer.hpp>

namespace stepwise::ui {

struct CliOptions {
    bool fake{false};
    bool batch{false};
    std::optional<std::filesystem::path> script;
    std::optional<OutputFormat> format;
    std::optional<LogLevel> verbose;
    std::string program;
    std::vector<std::string> program_args;
};

Result<CliOptions> parse_command_line(std::span<const std::string_view> args);

[[nodiscard]] std::string_view usage_text();

}  // namespace stepwise::ui
