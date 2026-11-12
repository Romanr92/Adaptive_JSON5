adaptive_json5_add_unit(adaptive_json5_document_unit_tests document_units
    document_tests.cpp
    access_boundary_tests.cpp
    member_name_span_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_owned_number_unit_tests owned_number_units
    owned_number_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_numeric_conversion_unit_tests numeric_units
    integer_conversion_tests.cpp
    floating_conversion_tests.cpp
    number_comparison_tests.cpp
    numeric_locale_tests.cpp)
# Scratch-free expectations depend on whether the Eisel-Lemire step is built (ADR 0005).
target_compile_definitions(adaptive_json5_numeric_conversion_unit_tests PRIVATE
    ADAPTIVE_JSON5_EXPECT_EISEL_LEMIRE=$<BOOL:${ADAPTIVE_JSON5_EISEL_LEMIRE}>)

adaptive_json5_add_unit(adaptive_json5_public_string_storage_unit_tests string_storage_units
    string_storage_tests.cpp)
