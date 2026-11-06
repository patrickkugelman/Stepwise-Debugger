#include <stepwise/base/log.hpp>

#include <ostream>

namespace stepwise {

std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Error:
            return "error";
        case LogLevel::Warn:
            return "warn";
        case LogLevel::Info:
            return "info";
        case LogLevel::Debug:
            return "debug";
        case LogLevel::Trace:
            return "trace";
    }
    return "unknown";
}

std::optional<LogLevel> parse_log_level(std::string_view text) noexcept {
    for (const LogLevel level :
         {LogLevel::Error, LogLevel::Warn, LogLevel::Info, LogLevel::Debug, LogLevel::Trace}) {
        if (to_string(level) == text) {
            return level;
        }
    }
    return std::nullopt;
}

void Logger::log(LogLevel level, std::string_view message) const {
    if (enabled(level)) {
        *sink_ << '[' << to_string(level) << "] " << message << '\n';
    }
}

}  // namespace stepwise
