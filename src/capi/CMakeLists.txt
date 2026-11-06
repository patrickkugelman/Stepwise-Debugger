# shared library exporting only stepwise_*. -fvisibility=hidden is not enough (libstdc++
# headers force default visibility), so a version script does the real filtering and
# also versions the symbols
add_library(stepwise_capi SHARED stepwise_c.cpp)
add_library(stepwise::capi ALIAS stepwise_capi)
set(STEPWISE_EXPORTS_MAP "${CMAKE_CURRENT_SOURCE_DIR}/exports.map")
set_target_properties(stepwise_capi PROPERTIES
    OUTPUT_NAME stepwise_c
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    LINK_DEPENDS "${STEPWISE_EXPORTS_MAP}")
target_include_directories(stepwise_capi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(stepwise_capi PRIVATE stepwise::commands stepwise_warnings)
target_link_options(stepwise_capi PRIVATE
    -Wl,--exclude-libs,ALL
    "-Wl,--version-script=${STEPWISE_EXPORTS_MAP}")
