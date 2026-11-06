#include <stepwise/ui/cli_options.hpp>

#include <format>

namespace stepwise::ui {
namespace {

Error bad_option(std::string message) {
    return Error{ErrorCode::InvalidArgument, std::move(message)};
}

Result<std::string_view> option_value(std::span<const std::string_view> args, std::size_t& index,
                                      std::string_view name, std::string_view inline_value,
                                      bool has_inline_value) {
    if (has_inline_value) {
        return inline_value;
    }
    if (index + 1 >= args.size()) {
        return bad_option(std::format("{} needs a value", name));
    }
    return args[++index];
}

}  // namespace

std::string_view usage_text() {
    return "usage: stepwise [options] <program> [args...]\n"
           "       stepwise [options] --fake\n"
           "options:\n"
           "  --script <file>      run the commands in <file> first, then continue interactively\n"
           "  --batch              no prompt and no interactive session: with --script, exit after\n"
           "                       the script; without it, read commands from standard input\n"
           "  --format text|json   output format (default text); json prints one object per line\n"
           "  --verbose[=level]    log to standard error; level is error, warn, info (default),\n"
           "                       debug or trace\n"
           "  --fake               use the built-in demo process instead of a real program\n"
           "  --                   end of options\n"
           "Settings can also be put in ~/.stepwiserc; options override them.\n";
}

Result<CliOptions> parse_command_line(std::span<const std::string_view> args) {
    CliOptions options;
    std::size_t index = 0;
    for (; index < args.size(); ++index) {
        std::string_view arg = args[index];
        if (arg == "--") {
            ++index;
            break;
        }
        if (!arg.starts_with("--")) {
            break;
        }

        std::string_view name = arg;
        std::string_view inline_value;
        bool has_inline_value = false;
        if (const std::size_t equals = arg.find('='); equals != std::string_view::npos) {
            name = arg.substr(0, equals);
            inline_value = arg.substr(equals + 1);
            has_inline_value = true;
        }

        if (name == "--fake" || name == "--batch") {
            if (has_inline_value) {
                return bad_option(std::format("{} takes no value", name));
            }
            (name == "--fake" ? options.fake : options.batch) = true;
        } else if (name == "--script") {
            auto value = option_value(args, index, name, inline_value, has_inline_value);
            if (!value) {
                return value.error();
            }
            options.script = std::filesystem::path{value.value()};
        } else if (name == "--format") {
            auto value = option_value(args, index, name, inline_value, has_inline_value);
            if (!value) {
                return value.error();
            }
            options.format = parse_output_format(value.value());
            if (!options.format) {
                return bad_option(std::format("--format must be text or json, not '{}'", value.value()));
            }
        } else if (name == "--verbose") {
            options.verbose = LogLevel::Info;
            if (has_inline_value) {
                options.verbose = parse_log_level(inline_value);
                if (!options.verbose) {
                    return bad_option(std::format(
                        "--verbose level must be error, warn, info, debug or trace, not '{}'", inline_value));
                }
            }
        } else {
            return bad_option(std::format("unknown option '{}'", name));
        }
    }

    if (options.fake) {
        if (index < args.size()) {
            return bad_option("--fake takes no program");
        }
        return options;
    }
    if (index >= args.size()) {
        return bad_option("missing program");
    }
    options.program = std::string{args[index]};
    for (++index; index < args.size(); ++index) {
        options.program_args.emplace_back(args[index]);
    }
    return options;
}

}  // namespace stepwise::ui
