#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <stepwise/base/log.hpp>
#include <stepwise/ui/renderer.hpp>

namespace stepwise::ui {

struct Settings {
    OutputFormat format{OutputFormat::Text};
    std::optional<LogLevel> verbose;
    std::string prompt{"(stepwise) "};
    int history_size{100};
};

[[nodiscard]] std::filesystem::path default_config_path();

Settings load_config(const std::filesystem::path& path, std::vector<std::string>& warnings);

}  // namespace stepwise::ui
