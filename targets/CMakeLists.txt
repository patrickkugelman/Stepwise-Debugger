foreach(target_name IN ITEMS hello loop crash_segv recursion)
    add_executable(${target_name} ${target_name}.c)
    target_compile_options(${target_name} PRIVATE -g -O0 -no-pie -fno-omit-frame-pointer)
    target_link_options(${target_name} PRIVATE -no-pie)
    target_link_libraries(${target_name} PRIVATE stepwise_warnings)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/targets")
endforeach()
