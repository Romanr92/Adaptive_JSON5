# Test-only target: no GoogleTest, private includes, installation or consumer runtime.
add_executable(adaptive_json5_fuzzer parser_fuzzer.cpp)
target_link_libraries(adaptive_json5_fuzzer PRIVATE AdaptiveJSON5::parser)
adaptive_json5_configure_target(adaptive_json5_fuzzer)
target_link_options(adaptive_json5_fuzzer PRIVATE -fsanitize=fuzzer)
