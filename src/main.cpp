#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <stepwise/backend/fake_debuggee.hpp>
#include <stepwise/backend/ptrace_debuggee.hpp>
#include <stepwise/base/log.hpp>
#include <stepwise/commands/builtin_commands.hpp>
#include <stepwise/core/session.hpp>
#include <stepwise/symbols/elf_symbol_provider.hpp>
#include <stepwise/symbols/null_symbol_provider.hpp>
#include <stepwise/ui/cli_options.hpp>
#include <stepwise/ui/config.hpp>
#include <stepwise/ui/frontend.hpp>

int main(int argc, char** argv) {
    using namespace stepwise;

    const std::vector<std::string_view> args(argv + 1, argv + argc);
    const auto options = ui::parse_command_line(args);
    if (!options) {
        std::cerr << "error: " << options.error().message() << '\n' << ui::usage_text();
        return 2;
    }

    std::vector<std::string> warnings;
    ui::Settings settings = ui::load_config(ui::default_config_path(), warnings);
    for (const std::string& warning : warnings) {
        std::cerr << "warning: " << warning << '\n';
    }
    if (options.value().format) {
        settings.format = *options.value().format;
    }
    if (options.value().verbose) {
        settings.verbose = options.value().verbose;
    }
    const Logger logger = settings.verbose ? Logger{*settings.verbose, std::cerr} : Logger{};

    std::unique_ptr<backend::Debuggee> debuggee;
    std::unique_ptr<symbols::SymbolProvider> symbols;
    backend::LaunchSpec spec;
    if (options.value().fake) {
        debuggee = std::make_unique<backend::FakeDebuggee>(backend::demo_fake_program());
        symbols = std::make_unique<symbols::NullSymbolProvider>();
        spec.program = "fake";
    } else {
        debuggee = std::make_unique<backend::PtraceDebuggee>();
        spec.program = options.value().program;
        spec.args = options.value().program_args;
        spec.stdout_to_stderr = settings.format == ui::OutputFormat::Json;

        auto loaded = symbols::ElfSymbolProvider::load(spec.program);
        if (loaded) {
            symbols = std::move(loaded.value());
            logger.info("loaded symbols from " + spec.program);
        } else {
            std::cerr << "warning: no symbols loaded: " << loaded.error().message() << '\n';
            symbols = std::make_unique<symbols::NullSymbolProvider>();
        }
    }

    core::Session session{std::move(debuggee), std::move(symbols), std::move(spec), logger};

    commands::CommandRegistry registry;
    if (auto registered = commands::register_builtin_commands(registry); !registered) {
        std::cerr << "error: " << registered.error().message() << '\n';
        return 1;
    }

    const auto renderer = ui::make_renderer(settings.format, session, std::cout, std::cerr);
    return ui::run_frontend(session, registry, *renderer, logger, options.value(), settings);
}
