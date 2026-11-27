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

# Eisel-Lemire conversion step and its power-of-five table (ADR 0005, decision 3). OFF saves
# the read-only table; conversion results are identical, only more numbers need scratch.
if(ADAPTIVE_JSON5_EISEL_LEMIRE)
    target_sources(adaptive_json5_parser PRIVATE src/value/power_of_five_table.cpp)
    target_compile_definitions(adaptive_json5_parser PRIVATE ADAPTIVE_JSON5_EISEL_LEMIRE=1)
else()
    target_compile_definitions(adaptive_json5_parser PRIVATE ADAPTIVE_JSON5_EISEL_LEMIRE=0)
endif()

# Optional hosted convenience API. It uses the global heap, so it is a separate
# target and never part of the embedded parser library (J5-101).
if(ADAPTIVE_JSON5_BUILD_HOSTED OR ADAPTIVE_JSON5_BUILD_TESTS)
    add_library(adaptive_json5_hosted STATIC src/hosted/parse.cpp)
    add_library(AdaptiveJSON5::hosted ALIAS adaptive_json5_hosted)
    set_target_properties(adaptive_json5_hosted PROPERTIES EXPORT_NAME hosted)
    target_link_libraries(adaptive_json5_hosted PUBLIC AdaptiveJSON5::parser)
    # Private headers shared with the parser, e.g. the json5_document construction access.
    target_include_directories(adaptive_json5_hosted PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")
    adaptive_json5_configure_target(adaptive_json5_hosted)
endif()

# Optional Draft-07 schema component (ADR 0006 decision 1). A separate target, so
# parser-only consumers build, link and install no schema code (J5-068).
if(ADAPTIVE_JSON5_BUILD_SCHEMA)
    add_library(adaptive_json5_schema STATIC
        src/schema/assertions.cpp
        src/schema/format.cpp
        src/schema/evaluator.cpp
        src/schema/idna.cpp
        src/schema/exact_number.cpp
        src/schema/instance_domain.cpp
        src/schema/json_pointer.cpp
        src/schema/meta_schema.cpp
        src/schema/pod_vector.cpp
        src/schema/regex.cpp
        src/schema/schema_compiler.cpp
        src/schema/schema_registry.cpp
        src/schema/schema_values.cpp
        src/schema/schema_work.cpp
        src/schema/unicode_property.cpp
        src/schema/uri.cpp
        src/schema/value_equality.cpp)
    add_library(AdaptiveJSON5::schema ALIAS adaptive_json5_schema)
    set_target_properties(adaptive_json5_schema PROPERTIES EXPORT_NAME schema)
    target_link_libraries(adaptive_json5_schema PUBLIC AdaptiveJSON5::parser)
    target_include_directories(adaptive_json5_schema PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")
    adaptive_json5_configure_target(adaptive_json5_schema)
endif()

if(ADAPTIVE_JSON5_BUILD_TESTS)
    add_subdirectory(tests)
endif()
