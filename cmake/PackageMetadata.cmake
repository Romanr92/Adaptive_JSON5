# Package metadata does not add public C++ APIs or change consumer flags.
set(ADAPTIVE_JSON5_PACKAGE_PROFILE "source" CACHE STRING "Binary release profile, or source")
set(ADAPTIVE_JSON5_SDK_SHA256 "" CACHE STRING "SHA-256 of the SDK used for a binary release")
find_package(Git QUIET)
set(ADAPTIVE_JSON5_SOURCE_REVISION "unknown")
if(Git_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE ADAPTIVE_JSON5_SOURCE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE adaptive_json5_git_result ERROR_QUIET)
    if(NOT adaptive_json5_git_result EQUAL 0)
        set(ADAPTIVE_JSON5_SOURCE_REVISION "unknown")
    endif()
endif()
set(ADAPTIVE_JSON5_TARGET_TRIPLE "")
set(ADAPTIVE_JSON5_GLIBCXX_ABI "")
set(ADAPTIVE_JSON5_MINGW_CRT "")
set(ADAPTIVE_JSON5_THREAD_MODEL "")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    separate_arguments(adaptive_json5_compiler_args NATIVE_COMMAND "${CMAKE_CXX_COMPILER_ARG1}")
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" ${adaptive_json5_compiler_args} -dumpmachine
        OUTPUT_VARIABLE ADAPTIVE_JSON5_TARGET_TRIPLE OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" ${adaptive_json5_compiler_args} -v
        ERROR_VARIABLE adaptive_json5_compiler_description OUTPUT_QUIET)
    if(adaptive_json5_compiler_description MATCHES "Thread model: ([a-z]+)")
        set(ADAPTIVE_JSON5_THREAD_MODEL "${CMAKE_MATCH_1}")
    endif()
    # Probe only a static object: an SDK need not be able to run/link host programs.
    include(CheckCXXSourceCompiles)
    function(adaptive_json5_detect_abi)
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
        unset(adaptive_json5_has_new_string_abi CACHE)
        unset(adaptive_json5_has_old_string_abi CACHE)
        check_cxx_source_compiles("#include <string>\nstatic_assert(_GLIBCXX_USE_CXX11_ABI == 1);"
            adaptive_json5_has_new_string_abi)
        if(adaptive_json5_has_new_string_abi)
            set(ADAPTIVE_JSON5_GLIBCXX_ABI "1" PARENT_SCOPE)
        else()
            check_cxx_source_compiles("#include <string>\nstatic_assert(_GLIBCXX_USE_CXX11_ABI == 0);"
                adaptive_json5_has_old_string_abi)
            if(adaptive_json5_has_old_string_abi)
                set(ADAPTIVE_JSON5_GLIBCXX_ABI "0" PARENT_SCOPE)
            endif()
        endif()
        if(MINGW)
            unset(adaptive_json5_has_ucrt CACHE)
            check_cxx_source_compiles("#include <_mingw.h>\n#ifndef _UCRT\n#error Not UCRT\n#endif"
                adaptive_json5_has_ucrt)
            if(adaptive_json5_has_ucrt)
                set(ADAPTIVE_JSON5_MINGW_CRT "UCRT" PARENT_SCOPE)
            else()
                set(ADAPTIVE_JSON5_MINGW_CRT "MSVCRT" PARENT_SCOPE)
            endif()
        endif()
    endfunction()
    adaptive_json5_detect_abi()
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/AdaptiveJSON5BuildInfo.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AdaptiveJSON5BuildInfo.cmake" @ONLY)

# Escape strings before writing JSON, including Windows paths and user flags.
function(adaptive_json5_json_string key value)
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(REPLACE "\n" "\\n" value "${value}")
    string(REPLACE "\r" "\\r" value "${value}")
    string(REPLACE "\t" "\\t" value "${value}")
    string(JSON adaptive_json5_info SET "${adaptive_json5_info}" "${key}" "\"${value}\"")
    set(adaptive_json5_info "${adaptive_json5_info}" PARENT_SCOPE)
endfunction()
set(adaptive_json5_info "{}")
foreach(pair IN ITEMS
    "version|PROJECT_VERSION" "source_revision|ADAPTIVE_JSON5_SOURCE_REVISION"
    "profile|ADAPTIVE_JSON5_PACKAGE_PROFILE" "system|CMAKE_SYSTEM_NAME"
    "processor|CMAKE_SYSTEM_PROCESSOR" "compiler_id|CMAKE_CXX_COMPILER_ID"
    "compiler_version|CMAKE_CXX_COMPILER_VERSION" "target_triple|ADAPTIVE_JSON5_TARGET_TRIPLE"
    "pointer_size|CMAKE_SIZEOF_VOID_P" "mingw_crt|ADAPTIVE_JSON5_MINGW_CRT"
    "thread_model|ADAPTIVE_JSON5_THREAD_MODEL"
    "glibcxx_cxx11_abi|ADAPTIVE_JSON5_GLIBCXX_ABI" "build_type|CMAKE_BUILD_TYPE"
    "exceptions|ADAPTIVE_JSON5_ENABLE_EXCEPTIONS" "rtti|ADAPTIVE_JSON5_ENABLE_RTTI"
    "sdk_sha256|ADAPTIVE_JSON5_SDK_SHA256" "cxx_flags|CMAKE_CXX_FLAGS"
    "compiler_arguments|CMAKE_CXX_COMPILER_ARG1"
    "release_flags|CMAKE_CXX_FLAGS_RELEASE")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    list(GET fields 1 variable)
    adaptive_json5_json_string("${key}" "${${variable}}")
endforeach()
adaptive_json5_json_string("cxx_standard" "17")
adaptive_json5_json_string("unicode_version" "17.0.0; historical identifier baseline 3.0.0")
adaptive_json5_json_string("runtime_dependencies" "C/C++ runtime supplied by the matching toolchain; no external parser library")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/build-info.json" "${adaptive_json5_info}\n")
