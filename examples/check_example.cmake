# Run one example program; check its exit code and compare its output with a file.
#
# cmake -DPROGRAM=<executable> -DEXPECTED=<file> -P check_example.cmake
#
# Run from the repository root, because the programs use paths relative to it.
execute_process(COMMAND "${PROGRAM}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE errors)

if(NOT exit_code EQUAL 0)
    message(FATAL_ERROR "Expected exit code 0, got ${exit_code}\n${actual}${errors}")
endif()

file(READ "${EXPECTED}" expected)

# Line endings vary by platform and checkout.
string(REPLACE "\r" "" actual "${actual}")
string(REPLACE "\r" "" expected "${expected}")

# The byte count changes with internal layouts, so only its presence is checked.
string(REGEX REPLACE "[0-9]+ bytes used" "<N> bytes used" actual "${actual}")

if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Output differs from ${EXPECTED}\n--- expected\n${expected}--- actual\n${actual}")
endif()
