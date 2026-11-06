#include <stepwise/ui/config.hpp>

#include <cstdlib>
#include <format>
#include <fstream>

#include <stepwise/base/number_parsing.hpp>

namespace stepwise::ui {
namespace {

constexpr std::string_view blanks = " \t\r";
constexpr int max_history_size = 10000;

std::string_view trim(std::string_view text) {
    const std::size_t begin = text.find_first_not_of(blanks);
    if (begin == std::string_view::npos) {
        return {};
    }
    return text.substr(begin, text.find_last_not_of(blanks) - begin + 1);
}

std::string_view unquote(std::string_view text) {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

std::optional<std::string> apply(Settings& settings, std::string_view key, std::string_view value) {
    if (key == "format") {
        const auto format = parse_output_format(value);
        if (!format) {
            return std::format("format must be text or json, not '{}'", value);
        }
        settings.format = *format;
    } else if (key == "verbose") {
        if (value == "off") {
            settings.verbose.reset();
        } else if (const auto level = parse_log_level(value)) {
            settings.verbose = level;
        } else {
            return std::format("verbose must be error, warn, info, debug, trace or off, not '{}'", value);
        }
    } else if (key == "prompt") {
        settings.prompt = std::string{unquote(value)};
    } else if (key == "history_size") {
        const auto size = parse_unsigned(value);
        if (!size || size.value() == 0 || size.value() > max_history_size) {
            return std::format("history_size must be between 1 and {}, not '{}'", max_history_size, value);
        }
        settings.history_size = static_cast<int>(size.value());
    } else {
        return std::format("unknown key '{}'", key);
    }
    return std::nullopt;
}

}  // namespace

std::filesystem::path default_config_path() {
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') {
        return {};
    }
    return std::filesystem::path{home} / ".stepwiserc";
}

Settings load_config(const std::filesystem::path& path, std::vector<std::string>& warnings) {
    Settings settings;
    if (path.empty()) {
        return settings;
    }
    std::ifstream file{path};
    if (!file) {
        return settings;
    }

    std::string raw;
    for (int line_number = 1; std::getline(file, raw); ++line_number) {
        const std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            warnings.push_back(std::format("{}:{}: expected key = value", path.string(), line_number));
            continue;
        }
        const auto key = trim(line.substr(0, equals));
        const auto value = trim(line.substr(equals + 1));
        if (auto problem = apply(settings, key, value)) {
            warnings.push_back(std::format("{}:{}: {}", path.string(), line_number, *problem));
        }
    }
    return settings;
}

}  // namespace stepwise::ui
