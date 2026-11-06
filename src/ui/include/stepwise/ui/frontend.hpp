#pragma once

#include <stepwise/base/log.hpp>
#include <stepwise/commands/command_registry.hpp>
#include <stepwise/core/session.hpp>
#include <stepwise/ui/cli_options.hpp>
#include <stepwise/ui/config.hpp>
#include <stepwise/ui/renderer.hpp>

namespace stepwise::ui {

int run_frontend(core::Session& session, const commands::CommandRegistry& registry, Renderer& renderer,
                 Logger logger, const CliOptions& options, const Settings& settings);

}  // namespace stepwise::ui
