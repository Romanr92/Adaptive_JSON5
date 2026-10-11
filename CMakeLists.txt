cmake_minimum_required(VERSION 3.25)
project(AdaptiveJSON5 VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(ADAPTIVE_JSON5_BUILD_TESTS "Build host GoogleTest unit tests" ${PROJECT_IS_TOP_LEVEL})
option(ADAPTIVE_JSON5_ENABLE_EXCEPTIONS "Enable exceptions in project unit targets" OFF)
option(ADAPTIVE_JSON5_ENABLE_RTTI "Enable RTTI in project unit targets" OFF)
set(ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR "" CACHE PATH "Existing GoogleTest 1.17.0 source checkout")
if(ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR)
    get_filename_component(ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR
        "${ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Development metadata only: no parser implementation or public C++ API exists yet.
add_library(adaptive_json5_build_options INTERFACE)
add_library(AdaptiveJSON5::build_options ALIAS adaptive_json5_build_options)
set_target_properties(adaptive_json5_build_options PROPERTIES EXPORT_NAME build_options)
target_compile_features(adaptive_json5_build_options INTERFACE cxx_std_17)

# Apply flags only to explicitly selected project targets, never parent flags.
function(adaptive_json5_configure_target target)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_features(${target} PRIVATE cxx_std_17)
    if(MSVC)
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
    elseif(NOT ADAPTIVE_JSON5_ENABLE_EXCEPTIONS OR NOT ADAPTIVE_JSON5_ENABLE_RTTI)
        message(FATAL_ERROR "Configure exception/RTTI flags for ${CMAKE_CXX_COMPILER_ID} before using this mode")
    endif()
endfunction()

if(ADAPTIVE_JSON5_BUILD_TESTS)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "GoogleTest is host-only here; disable ADAPTIVE_JSON5_BUILD_TESTS for embedded builds")
    endif()
    enable_testing()
    add_subdirectory(tests)
endif()

set(adaptive_json5_package_dir ${CMAKE_INSTALL_LIBDIR}/cmake/AdaptiveJSON5)
configure_package_config_file(cmake/AdaptiveJSON5Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveJSON5Config.cmake
    INSTALL_DESTINATION ${adaptive_json5_package_dir})
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveJSON5ConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(TARGETS adaptive_json5_build_options EXPORT AdaptiveJSON5Targets)
install(EXPORT AdaptiveJSON5Targets FILE AdaptiveJSON5Targets.cmake
    NAMESPACE AdaptiveJSON5:: DESTINATION ${adaptive_json5_package_dir})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveJSON5Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AdaptiveJSON5ConfigVersion.cmake
    DESTINATION ${adaptive_json5_package_dir})
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/AdaptiveJSON5)
