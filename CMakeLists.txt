cmake_minimum_required(VERSION 3.25)
project(AdaptiveJSON5 VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(ADAPTIVE_JSON5_BUILD_TESTS "Build host GoogleTest unit tests"
    ${PROJECT_IS_TOP_LEVEL})
option(ADAPTIVE_JSON5_BUILD_VALIDATION_TESTS
    "Build the Official JSON & JSON5 Validation Tests" OFF)
option(ADAPTIVE_JSON5_BUILD_LIBRARY "Build/export the library component" ON)
option(ADAPTIVE_JSON5_BUILD_SCHEMA
    "Build the optional schema component when implemented" OFF)
option(ADAPTIVE_JSON5_BUILD_EXECUTABLE
    "Build the host example executable when implemented" OFF)
option(ADAPTIVE_JSON5_ENABLE_EXCEPTIONS "Enable exceptions in project targets"
    OFF)
option(ADAPTIVE_JSON5_ENABLE_RTTI "Enable RTTI in project unit targets" OFF)
option(ADAPTIVE_JSON5_BUILD_HOSTED
    "Build the optional hosted one-call parse API (uses the global heap)" OFF)
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

if(ADAPTIVE_JSON5_BUILD_TESTS OR ADAPTIVE_JSON5_BUILD_VALIDATION_TESTS)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "GoogleTest is host-only here; disable "
            "ADAPTIVE_JSON5_BUILD_TESTS and "
            "ADAPTIVE_JSON5_BUILD_VALIDATION_TESTS for embedded builds")
    endif()
    enable_testing()
    include("${PROJECT_SOURCE_DIR}/cmake/Dependencies.cmake")
    include(GoogleTest)
    include("${PROJECT_SOURCE_DIR}/cmake/UnitTests.cmake")
endif()
if(ADAPTIVE_JSON5_BUILD_LIBRARY OR ADAPTIVE_JSON5_BUILD_TESTS
        OR ADAPTIVE_JSON5_BUILD_VALIDATION_TESTS)
    add_subdirectory(core)
endif()
if(ADAPTIVE_JSON5_BUILD_TESTS OR ADAPTIVE_JSON5_BUILD_VALIDATION_TESTS)
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
        if(ADAPTIVE_JSON5_BUILD_HOSTED)
            install(TARGETS adaptive_json5_hosted EXPORT AdaptiveJSON5Targets
                ARCHIVE DESTINATION ${ADAPTIVE_JSON5_INSTALL_ARCHIVEDIR})
              set(adaptive_json5_header_excludes)
        else()
        # Without the hosted library its header would only produce link errors.
            set(adaptive_json5_header_excludes PATTERN "hosted" EXCLUDE)
        endif()
        install(DIRECTORY includes/adaptive_json5 DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.hpp" ${adaptive_json5_header_excludes})
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
