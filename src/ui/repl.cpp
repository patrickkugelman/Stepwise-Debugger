#include <stepwise/ui/repl.hpp>

#include <format>
#include <string_view>
#include <variant>

namespace stepwise::ui {
namespace {

std::string_view trim(std::string_view text) {
    constexpr std::string_view blanks = " \t\r\n";
    const std::size_t begin = text.find_first_not_of(blanks);
    if (begin == std::string_view::npos) {
        return {};
    }
    return text.substr(begin, text.find_last_not_of(blanks) - begin + 1);
}

}  // namespace

Repl::Repl(core::Session& session, const commands::CommandRegistry& registry, Renderer& renderer,
           Logger logger)
    : session_(session),
      registry_(registry),
      renderer_(renderer),
      logger_(logger),
      subscription_(session.events().subscribe(
          [&renderer](const core::Event& event) { renderer.on_event(event); })) {}

Repl::Outcome Repl::run(LineSource& source, Mode mode) {
    Outcome outcome;
    for (std::size_t line_number = 1;; ++line_number) {
        const auto raw = source.next_line();
        if (!raw) {
            return outcome;
        }
        const std::string_view line = trim(*raw);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        logger_.debug(std::format("command: {}", line));

        std::optional<Error> failure;
        if (auto command = registry_.parse_line(line)) {
            commands::CommandContext context{session_, registry_};
            if (auto output = command.value()->execute(context)) {
                if (std::holds_alternative<commands::QuitOutput>(output.value())) {
                    outcome.quit = true;
                    return outcome;
                }
                renderer_.on_output(output.value());
            } else {
                failure = output.error();
            }
        } else {
            failure = command.error();
        }

        if (failure) {
            if (mode == Mode::Strict) {
                renderer_.on_error(
                    Error{failure->code(), std::format("line {}: {}", line_number, failure->message())});
                outcome.failed = true;
                return outcome;
            }
            renderer_.on_error(*failure);
        }
    }
}

}  // namespace stepwise::ui
