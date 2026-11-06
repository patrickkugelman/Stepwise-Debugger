#pragma once

#include <stepwise/base/log.hpp>
#include <stepwise/commands/command_registry.hpp>
#include <stepwise/core/event_bus.hpp>
#include <stepwise/core/session.hpp>
#include <stepwise/ui/line_source.hpp>
#include <stepwise/ui/renderer.hpp>

namespace stepwise::ui {

class Repl {
public:
    enum class Mode {
        Interactive,
        Strict,
    };

    struct Outcome {
        bool quit{false};
        bool failed{false};
    };

    Repl(core::Session& session, const commands::CommandRegistry& registry, Renderer& renderer,
         Logger logger);

    Outcome run(LineSource& source, Mode mode);

private:
    core::Session& session_;
    const commands::CommandRegistry& registry_;
    Renderer& renderer_;
    Logger logger_;
    core::Subscription subscription_;
};

}  // namespace stepwise::ui
