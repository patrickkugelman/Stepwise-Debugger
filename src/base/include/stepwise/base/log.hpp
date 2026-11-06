#pragma once

#include <iosfwd>
#include <optional>
#include <string_view>

namespace stepwise {

enum class LogLevel { Error, Warn, Info, Debug, Trace };

[[nodiscard]] std::string_view to_string(LogLevel level) noexcept;
[[nodiscard]] std::optional<LogLevel> parse_log_level(std::string_view text) noexcept;

class Logger {
public:
    Logger() = default;
    Logger(LogLevel threshold, std::ostream& sink) noexcept : sink_(&sink), threshold_(threshold) {}

    [[nodiscard]] bool enabled(LogLevel level) const noexcept {
        return sink_ != nullptr && level <= threshold_;
    }

    void log(LogLevel level, std::string_view message) const;
    void error(std::string_view message) const { log(LogLevel::Error, message); }
    void warn(std::string_view message) const { log(LogLevel::Warn, message); }
    void info(std::string_view message) const { log(LogLevel::Info, message); }
    void debug(std::string_view message) const { log(LogLevel::Debug, message); }
    void trace(std::string_view message) const { log(LogLevel::Trace, message); }

private:
    std::ostream* sink_{nullptr};
    LogLevel threshold_{LogLevel::Error};
};

}  // namespace stepwise
