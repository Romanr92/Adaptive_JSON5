# Compiled parser and private implementation boundary.
add_subdirectory(src)
add_library(AdaptiveJSON5::parser ALIAS adaptive_json5_parser)
set_target_properties(adaptive_json5_parser PROPERTIES EXPORT_NAME parser)
target_link_libraries(adaptive_json5_parser PUBLIC
    AdaptiveJSON5::build_options)
target_include_directories(adaptive_json5_parser
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/includes>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")
adaptive_json5_configure_target(adaptive_json5_parser)

if(ADAPTIVE_JSON5_BUILD_TESTS)
    add_subdirectory(tests)
endif()
