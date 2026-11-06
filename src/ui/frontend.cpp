#include <stepwise/ui/frontend.hpp>

#include <fstream>
#include <iostream>

#include <stepwise/ui/completer.hpp>
#include <stepwise/ui/linenoise_source.hpp>
#include <stepwise/ui/repl.hpp>

namespace stepwise::ui {

int run_frontend(core::Session& session, const commands::CommandRegistry& registry, Renderer& renderer,
                 Logger logger, const CliOptions& options, const Settings& settings) {
    Repl repl{session, registry, renderer, logger};

    if (options.script) {
        std::ifstream file{*options.script};
        if (!file) {
            std::cerr << "error: cannot open script '" << options.script->string() << "'\n";
            return 2;
        }
        logger.info("running script " + options.script->string());
        StreamLineSource script{file};
        const Repl::Outcome outcome = repl.run(script, Repl::Mode::Strict);
        if (outcome.failed) {
            return 1;
        }
        if (outcome.quit || options.batch) {
            return 0;
        }
    } else if (options.batch) {
        StreamLineSource input{std::cin};
        return repl.run(input, Repl::Mode::Strict).failed ? 1 : 0;
    }

    const Completer completer{registry, session.symbols()};
    LinenoiseSource input{settings.prompt, completer, settings.history_size};
    repl.run(input, Repl::Mode::Interactive);
    return 0;
}

}  // namespace stepwise::ui
