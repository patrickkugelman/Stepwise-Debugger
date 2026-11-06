add_executable(c_client c_client.c)
target_link_libraries(c_client PRIVATE stepwise::capi stepwise_warnings)
set_target_properties(c_client PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")
