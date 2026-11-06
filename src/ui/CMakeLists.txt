stepwise_add_layer(ui
    SOURCES
        cli_options.cpp
        completer.cpp
        config.cpp
        frontend.cpp
        json_renderer.cpp
        json_writer.cpp
        linenoise_source.cpp
        renderer.cpp
        repl.cpp
        text_renderer.cpp
    DEPS stepwise::commands)
target_link_libraries(stepwise_ui PRIVATE linenoise)
