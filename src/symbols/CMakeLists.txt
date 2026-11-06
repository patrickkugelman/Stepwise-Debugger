stepwise_add_layer(symbols
    SOURCES
        byte_reader.cpp
        dwarf_line.cpp
        elf_file.cpp
        elf_symbol_provider.cpp
        source_cache.cpp
    DEPS stepwise::base)
