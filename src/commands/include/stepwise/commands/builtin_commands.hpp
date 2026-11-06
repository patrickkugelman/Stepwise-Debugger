#pragma once

#include <stepwise/base/result.hpp>
#include <stepwise/commands/command_registry.hpp>

namespace stepwise::commands {

Result<void> register_builtin_commands(CommandRegistry& registry);

}  // namespace stepwise::commands
