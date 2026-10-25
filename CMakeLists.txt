cmake_minimum_required(VERSION 3.25)
project(AdaptiveJSON5 VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(ADAPTIVE_JSON5_BUILD_TESTS "Build host GoogleTest unit tests"
    ${PROJECT_IS_TOP_LEVEL})
option(ADAPTIVE_JSON5_BUILD_LIBRARY "Build/export the library component" ON)
option(ADAPTIVE_JSON5_BUILD_SCHEMA
    "Build the optional schema component when implemented" OFF)
option(ADAPTIVE_JSON5_BUILD_EXECUTABLE
    "Build the host example executable when implemented" OFF)
option(ADAPTIVE_JSON5_ENABLE_EXCEPTIONS "Enable exceptions in project targets"
    OFF)
option(ADAPTIVE_JSON5_ENABLE_RTTI "Enable RTTI in project unit targets" OFF)
if(ADAPTIVE_JSON5_BUILD_SCHEMA)
    message(FATAL_ERROR
        "The optional schema implementation is not available yet; build the "
        "parser component only")
endif()
set(ADAPTIVE_JSON5_INSTALL_ARCHIVEDIR "${CMAKE_INSTALL_LIBDIR}" CACHE STRING
    "Static archive install directory; release bundles use . beside include/")
include(cmake/PackageMetadata.cmake)
set(ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR "" CACHE PATH
    "Existing GoogleTest 1.17.0 source checkout")
if(ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR)
    get_filename_component(ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR
        "${ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Shared C++17 consumption metadata; compiled core is defined below.
add_library(adaptive_json5_build_options INTERFACE)
add_library(AdaptiveJSON5::build_options ALIAS adaptive_json5_build_options)
set_target_properties(adaptive_json5_build_options PROPERTIES EXPORT_NAME
    build_options)
target_compile_features(adaptive_json5_build_options INTERFACE cxx_std_17)

# Apply flags only to explicitly selected project targets, never parent flags.
function(adaptive_json5_configure_target target)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_compile_features(${target} PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /w14706)
        if(ADAPTIVE_JSON5_ENABLE_EXCEPTIONS)
            target_compile_options(${target} PRIVATE /EHsc)
        else()
            target_compile_options(${target} PRIVATE /EHs-c-)
            target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
        endif()
        if(ADAPTIVE_JSON5_ENABLE_RTTI)
            target_compile_options(${target} PRIVATE /GR)
        else()
            target_compile_options(${target} PRIVATE /GR-)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options(${target} PRIVATE -Wparentheses -Werror)
        if(ADAPTIVE_JSON5_ENABLE_EXCEPTIONS)
            target_compile_options(${target} PRIVATE -fexceptions)
        else()
            target_compile_options(${target} PRIVATE -fno-exceptions)
        endif()
        if(ADAPTIVE_JSON5_ENABLE_RTTI)
            target_compile_options(${target} PRIVATE -frtti)
        else()
            target_compile_options(${target} PRIVATE -fno-rtti)
        endif()
    elseif(NOT ADAPTIVE_JSON5_ENABLE_EXCEPTIONS OR NOT
        ADAPTIVE_JSON5_ENABLE_RTTI)
        message(FATAL_ERROR
            "Configure exception/RTTI flags for ${CMAKE_CXX_COMPILER_ID} "
            "before using this mode")
    endif()
endfunction()

if(ADAPTIVE_JSON5_BUILD_LIBRARY OR ADAPTIVE_JSON5_BUILD_TESTS)
    add_library(adaptive_json5_parser STATIC
        src/src/lexer/comment.cpp
        src/src/lexer/identifier/identifier_classification.cpp
        src/src/lexer/identifier/identifier_lexer.cpp
        src/src/lexer/line_terminator.cpp
        src/src/lexer/member_name_lexer.cpp
        src/src/lexer/number/decimal_lexer.cpp
        src/src/lexer/number/hexadecimal_lexer.cpp
        src/src/lexer/number/non_finite_lexer.cpp
        src/src/lexer/number/number_lexer.cpp
        src/src/lexer/punctuation.cpp
        src/src/lexer/scalar_lexer.cpp
        src/src/lexer/string/hex_escape.cpp
        src/src/lexer/string/line_continuation.cpp
        src/src/lexer/string/non_escape.cpp
        src/src/lexer/string/simple_escape.cpp
        src/src/lexer/string/string_boundary.cpp
        src/src/lexer/string/string_escape.cpp
        src/src/lexer/string/string_lexer.cpp
        src/src/lexer/string/string_warning.cpp
        src/src/lexer/string/unicode_escape.cpp
        src/src/lexer/token_boundary.cpp
        src/src/lexer/trivia.cpp
        src/src/lexer/whitespace.cpp
        src/src/memory/accounted_storage.cpp
        src/src/memory/decoded_string_builder.cpp
        src/src/memory/owned_block.cpp
        src/src/memory/resource_budget.cpp
        src/src/memory/work_budget.cpp
        src/src/parser/parser.cpp
        src/src/parser/parser_attachment.cpp
        src/src/parser/parser_container_close.cpp
        src/src/parser/parser_container_open.cpp
        src/src/parser/parser_dispatch.cpp
        src/src/parser/parser_duplicate.cpp
        src/src/parser/parser_engine.cpp
        src/src/parser/parser_stack.cpp
        src/src/parser/primitive_parser.cpp
        src/src/unicode/utf16.cpp
        src/src/unicode/utf16_decode.cpp
        src/src/unicode/utf8.cpp
        src/src/value/decoded_name_equal.cpp
        src/src/value/decoded_string.cpp
        src/src/value/document.cpp
        src/src/value/document_links.cpp
        src/src/value/member_lookup.cpp
        src/src/value/number_analysis.cpp
        src/src/value/owned_number.cpp
        src/src/value/utf8_export.cpp
        src/src/value/value_iterator.cpp
        src/src/value/value_storage.cpp
        src/src/value/value_view.cpp
        src/src/value/integer_magnitude.cpp
        src/src/value/numeric_conversion.cpp
        src/src/value/big_unsigned.cpp
        src/src/value/floating_conversion.cpp
        src/src/value/number_comparison.cpp
        src/src/input/text_position.cpp
        src/src/value/warning_list.cpp
    )
    add_library(AdaptiveJSON5::parser ALIAS adaptive_json5_parser)
    set_target_properties(adaptive_json5_parser PROPERTIES EXPORT_NAME parser)
    target_link_libraries(adaptive_json5_parser PUBLIC
        AdaptiveJSON5::build_options)
    target_include_directories(adaptive_json5_parser
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/includes>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE "${PROJECT_SOURCE_DIR}/src/includes")
    adaptive_json5_configure_target(adaptive_json5_parser)
endif()

if(ADAPTIVE_JSON5_BUILD_TESTS)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "GoogleTest is host-only here; disable ADAPTIVE_JSON5_BUILD_TESTS "
            "for embedded builds")
    endif()
    enable_testing()
    include("${PROJECT_SOURCE_DIR}/cmake/Dependencies.cmake")
    include(GoogleTest)
    include("${PROJECT_SOURCE_DIR}/cmake/UnitTests.cmake")
    add_subdirectory(src/tests)
    add_subdirectory(tests)
endif()

set(adaptive_json5_package_dir ${CMAKE_INSTALL_LIBDIR}/cmake/AdaptiveJSON5)
if(ADAPTIVE_JSON5_BUILD_EXECUTABLE)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "The example executable is host-only; disable "
            "ADAPTIVE_JSON5_BUILD_EXECUTABLE for embedded builds")
    endif()
    add_subdirectory(examples)
endif()

if(ADAPTIVE_JSON5_BUILD_LIBRARY)

    configure_package_config_file(cmake/AdaptiveJSON5Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveJSON5Config.cmake
        INSTALL_DESTINATION ${adaptive_json5_package_dir})
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveJSON5ConfigVersion.cmake
        VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
    install(TARGETS adaptive_json5_build_options adaptive_json5_parser EXPORT
        AdaptiveJSON5Targets
        ARCHIVE DESTINATION ${ADAPTIVE_JSON5_INSTALL_ARCHIVEDIR})
    install(DIRECTORY includes/adaptive_json5 DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp")
    install(EXPORT AdaptiveJSON5Targets FILE AdaptiveJSON5Targets.cmake
        NAMESPACE AdaptiveJSON5:: DESTINATION ${adaptive_json5_package_dir})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveJSON5Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveJSON5ConfigVersion.cmake
        DESTINATION ${adaptive_json5_package_dir})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveJSON5BuildInfo.cmake
        DESTINATION ${adaptive_json5_package_dir})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/build-info.json
        DESTINATION ${CMAKE_INSTALL_DATADIR}/AdaptiveJSON5)
    install(FILES LICENSE NOTICE DESTINATION
        ${CMAKE_INSTALL_DATADIR}/AdaptiveJSON5)
    install(FILES docs/licensing.md docs/dependencies.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/AdaptiveJSON5/docs)
    install(FILES cmake/ImportAdaptiveJSON5.cmake
        DESTINATION ${CMAKE_INSTALL_DATADIR}/AdaptiveJSON5/cmake)
    install(FILES LICENSES/GoogleTest-BSD-3-Clause.txt
        LICENSES/Unicode-License-V3.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/AdaptiveJSON5/LICENSES)
endif()
