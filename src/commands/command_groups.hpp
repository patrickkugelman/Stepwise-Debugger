#pragma once

#include <stepwise/base/result.hpp>
#include <stepwise/commands/command_registry.hpp>

namespace stepwise::commands {

Result<void> register_breakpoint_commands(CommandRegistry& registry);
Result<void> register_execution_commands(CommandRegistry& registry);
Result<void> register_info_commands(CommandRegistry& registry);
Result<void> register_inspection_commands(CommandRegistry& registry);
Result<void> register_memory_commands(CommandRegistry& registry);
Result<void> register_meta_commands(CommandRegistry& registry);
Result<void> register_source_commands(CommandRegistry& registry);

}  // namespace stepwise::commands
