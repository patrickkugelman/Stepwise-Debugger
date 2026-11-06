#include <stepwise/ui/linenoise_source.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <memory>

#include <unistd.h>

extern "C" {
#include <linenoise.h>
}

namespace stepwise::ui {
namespace {

// the completion callback has no user data argument, hence this pointer
const Completer* active_completer = nullptr;

constexpr std::size_t max_line_length = 4096;

void complete_line(const char* line, linenoiseCompletions* completions) {
    if (active_completer == nullptr) {
        return;
    }
    for (const std::string& candidate : active_completer->complete(line)) {
        linenoiseAddCompletion(completions, candidate.c_str());
    }
}

bool is_unsupported_terminal() {
    const char* term = std::getenv("TERM");
    if (term == nullptr) {
        return false;
    }
    std::string name{term};
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
    return name == "dumb" || name == "cons25" || name == "emacs";
}

}  // namespace

LinenoiseSource::LinenoiseSource(std::string prompt, const Completer& completer, int history_size)
    : prompt_(std::move(prompt)) {
    active_completer = &completer;
    linenoiseSetCompletionCallback(&complete_line);
    linenoiseHistorySetMaxLen(history_size + 1);
    linenoiseSetMultiLine(1);
}

LinenoiseSource::~LinenoiseSource() {
    linenoiseSetCompletionCallback(nullptr);
    active_completer = nullptr;
}

std::optional<std::string> LinenoiseSource::next_line() {
    const bool is_terminal = isatty(STDIN_FILENO) != 0 || std::getenv("LINENOISE_ASSUME_TTY") != nullptr;
    if (!is_terminal || is_unsupported_terminal()) {
        if (is_terminal) {
            std::cout << prompt_ << std::flush;
        }
        std::string line;
        if (!std::getline(std::cin, line)) {
            return std::nullopt;
        }
        return line;
    }

    // step-by-step API instead of linenoise(): Ctrl-C comes through errno and linenoise()
    // prints a newline first, which can clobber it. Read it right away
    std::array<char, max_line_length> buffer{};
    linenoiseState state;
    if (linenoiseEditStart(&state, -1, -1, buffer.data(), buffer.size(), prompt_.c_str()) == -1) {
        return std::nullopt;
    }
    char* result = nullptr;
    do {
        errno = 0;
        result = linenoiseEditFeed(&state);
    } while (result == linenoiseEditMore);
    const int error = errno;
    linenoiseEditStop(&state);

    const std::unique_ptr<char, void (*)(void*)> line{result, &linenoiseFree};
    if (!line) {
        if (error == EAGAIN) {
            return std::string{};
        }
        return std::nullopt;
    }
    if (*line != '\0') {
        linenoiseHistoryAdd(line.get());
    }
    return std::string{line.get()};
}

}  // namespace stepwise::ui
