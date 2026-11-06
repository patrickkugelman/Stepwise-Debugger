#include <stepwise/commands/builtin_commands.hpp>

#include "command_groups.hpp"

namespace stepwise::commands {

Result<void> register_builtin_commands(CommandRegistry& registry) {
    using Register = Result<void> (*)(CommandRegistry&);
    for (const Register add : {register_execution_commands, register_breakpoint_commands,
                               register_inspection_commands, register_memory_commands,
                               register_source_commands, register_info_commands,
                               register_meta_commands}) {
        if (auto added = add(registry); !added) {
            return added;
        }
    }
    return {};
}

}  // namespace stepwise::commands
