stepwise_add_layer(commands
    SOURCES
        argument_parsing.cpp
        breakpoint_commands.cpp
        builtin_commands.cpp
        command_registry.cpp
        execution_commands.cpp
        info_commands.cpp
        inspection_commands.cpp
        memory_commands.cpp
        meta_commands.cpp
        source_commands.cpp
    DEPS stepwise::core)
